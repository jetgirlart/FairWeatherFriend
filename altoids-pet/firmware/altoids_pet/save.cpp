#include "save.h"
#include "field_notes.h"
#ifdef ARDUINO
#include "config.h"
#endif
#ifndef LATITUDE
#define LATITUDE 0.0
#define LONGITUDE 0.0
#endif
#ifndef SOUND_ENABLED
#define SOUND_ENABLED true
#endif
#include "gear.h"
#include "gear_variants.h"
#include "field_locations.h"
#include "measurement_units.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <stddef.h>
#include <initializer_list>
#include <string.h>
#include <math.h>

namespace {
constexpr uint32_t SAVE_MAGIC = 0x46574631; // FWF1 envelope, stable across versions.
constexpr size_t V1_PAYLOAD_BYTES = 148;
constexpr size_t COMMON_PAYLOAD_BYTES = 144;
constexpr size_t V2_PAYLOAD_BYTES = COMMON_PAYLOAD_BYTES + 4 * GEAR_SLOT_COUNT;
constexpr size_t V3_PAYLOAD_BYTES = V2_PAYLOAD_BYTES + 8;
constexpr size_t V4_PAYLOAD_BYTES = V3_PAYLOAD_BYTES + 20;
constexpr size_t V5_PAYLOAD_BYTES = V4_PAYLOAD_BYTES + 24 + 12 * RECORD_COUNT;
constexpr size_t V6_PAYLOAD_BYTES = V5_PAYLOAD_BYTES + 20 * FIELD_EVENT_COUNT + 16 * RECENT_ALERT_COUNT;
constexpr size_t V7_PAYLOAD_BYTES = V6_PAYLOAD_BYTES + 28 * FIELD_LOCATION_COUNT + 48;
constexpr size_t V8_PAYLOAD_BYTES = V7_PAYLOAD_BYTES + 8 + 4 * 7 + 4 * GEAR_SLOT_COUNT;
constexpr size_t FIELD_NOTE_BYTES = 56;
constexpr size_t PAYLOAD_BYTES = V8_PAYLOAD_BYTES + 8 + FIELD_NOTE_BYTES * FIELD_NOTE_COUNT;
constexpr size_t HEADER_BYTES = 24;
constexpr size_t RECORD_BYTES = HEADER_BYTES + PAYLOAD_BYTES;
// Save operations run synchronously on the firmware loop. Fixed scratch buffers
// keep the larger checkpoints off ESP32's small loop-task stack; never serialized.
uint8_t saveBytes[RECORD_BYTES], saveVerified[RECORD_BYTES];
uint64_t generation = 0;
bool writable = false;

uint32_t hashBytes(const uint8_t *bytes, size_t length, uint32_t hash = 2166136261UL) {
  for (size_t i = 0; i < length; ++i) hash = (hash ^ bytes[i]) * 16777619UL;
  return hash;
}
void put32(uint8_t *&p, uint32_t value) {
  for (int i = 0; i < 4; ++i) { *p++ = value & 0xff; value >>= 8; }
}
void put64(uint8_t *&p, uint64_t value) {
  for (int i = 0; i < 8; ++i) { *p++ = value & 0xff; value >>= 8; }
}
uint32_t get32(const uint8_t *&p) {
  uint32_t value = 0;
  for (int i = 0; i < 4; ++i) value |= uint32_t(*p++) << (8 * i);
  return value;
}
uint64_t get64(const uint8_t *&p) {
  uint64_t value = 0;
  for (int i = 0; i < 8; ++i) value |= uint64_t(*p++) << (8 * i);
  return value;
}
void encodeCommon(const BuddySaveData &s, uint8_t *p) {
  put32(p, s.saveVersion); put64(p, s.createdAt); put64(p, s.totalObservations);
  put32(p, s.uniqueDaysObserved); put64(p, s.latestObservationAt); put32(p, s.lastObservedDate);
  put32(p, s.saveVersion < 5 && s.totalObservations ? milliCToFahrenheitDeci(s.latestTemperatureMilliC) : s.latestTemperatureMilliC); put32(p, s.latestWeatherCode);
  put32(p, static_cast<uint8_t>(s.latestCategory));
  put32(p, s.saveVersion < 5 && s.totalObservations ? milliCToFahrenheitDeci(s.highestTemperatureMilliC) : s.highestTemperatureMilliC); put64(p, s.highestTemperatureAt);
  put32(p, s.saveVersion < 5 && s.totalObservations ? milliCToFahrenheitDeci(s.lowestTemperatureMilliC) : s.lowestTemperatureMilliC); put64(p, s.lowestTemperatureAt);
  for (uint64_t count : s.weatherCounts) put64(p, count);
  put32(p, s.discoveredWeather); put32(p, s.unlockedGear);
}
void encodeCurrent(const BuddySaveData &s, uint8_t *p) {
  encodeCommon(s, p);
  p += COMMON_PAYLOAD_BYTES;
  for (GearId gear : s.equippedSlots) put32(p, static_cast<uint8_t>(gear));
  put32(p, static_cast<uint8_t>(s.furPalette)); put32(p, s.setupComplete ? 1 : 0);
  put32(p, s.soundEnabled ? 1 : 0); put32(p, static_cast<uint8_t>(s.units));
  put32(p, s.locationConfigured ? 1 : 0);
  put32(p, s.latitudeMicrodegrees); put32(p, s.longitudeMicrodegrees);
  put32(p, s.latestMetrics.validMask);
  for (int32_t value : s.latestMetrics.values) put32(p, value);
  for (const auto &record : s.records) { put32(p, record.value); put64(p, record.timestamp); }
}
// v6 appends compact events and bounded alert fingerprints.
void encodeEvents(const BuddySaveData &s, uint8_t *p) {
  for (const auto &event : s.fieldEvents) { put32(p, event.count); put64(p, event.firstAt); put64(p, event.latestAt); }
  for (const auto &alert : s.recentAlerts) { put64(p, alert.hash); put64(p, alert.seenAt); }
}
void encodeLocations(const BuddySaveData &s, uint8_t *p) {
  for (const auto &location : s.locations) {
    put32(p, (location.used ? 1 : 0) | (location.visited ? 2 : 0));
    memcpy(p, location.name, LOCATION_NAME_BYTES); p += LOCATION_NAME_BYTES;
    put32(p, location.latitudeMicrodegrees); put32(p, location.longitudeMicrodegrees);
  }
  put32(p, s.activeLocation); put64(p, s.fieldSitesVisited);
  put32(p, s.latestLocation); put32(p, s.highestTemperatureLocation); put32(p, s.lowestTemperatureLocation);
  for (const auto &record : s.records) put32(p, record.location);
}
void encodeVariants(const BuddySaveData &s, uint8_t *p) {
  put64(p, s.freezingObservations);
  for (uint32_t mask : s.unlockedVariants) put32(p, mask);
  for (uint8_t variant : s.equippedVariants) put32(p, variant);
}
void encodeNotes(const BuddySaveData &s, uint8_t *p) {
  put32(p, s.fieldNoteCount); put32(p, s.fieldNoteNext);
  for (const auto &n : s.fieldNotes) {
    put64(p, n.timestamp); put32(p, n.temperatureMilliC); put32(p, n.weatherCode);
    put32(p, uint8_t(n.category)); put32(p, n.location); put32(p, n.metrics.validMask);
    for (auto value : n.metrics.values) put32(p, value);
    put32(p, n.outcomes); put32(p, n.severeEvents);
  }
}
void migrateVariants(BuddySaveData &s) {
  initializeGearVariants(s);
  // Historical observations do not contain a freezing count. Only one is known.
  s.freezingObservations = s.totalObservations && s.lowestTemperatureMilliC < 0 ? 1 : 0;
}
void migrateHome(BuddySaveData &s) {
  initializeHomeLocation(s, s.locationConfigured ? s.latitudeMicrodegrees / 1000000.0 : LATITUDE,
                         s.locationConfigured ? s.longitudeMicrodegrees / 1000000.0 : LONGITUDE);
}
// Reconstruct the exact v1 payload for verifying old JSON checksums.
uint32_t legacyChecksum(const BuddySaveData &s, GearId gear) {
  uint8_t bytes[V1_PAYLOAD_BYTES];
  BuddySaveData old = s; old.saveVersion = 1;
  encodeCommon(old, bytes);
  uint8_t *p = bytes + COMMON_PAYLOAD_BYTES; put32(p, static_cast<uint8_t>(gear));
  return hashBytes(bytes, sizeof(bytes));
}
size_t payloadSize(uint32_t version) {
  switch (version) { case 1: return V1_PAYLOAD_BYTES; case 2: return V2_PAYLOAD_BYTES; case 3: return V3_PAYLOAD_BYTES; case 4: return V4_PAYLOAD_BYTES; case 5: return V5_PAYLOAD_BYTES; case 6: return V6_PAYLOAD_BYTES; case 7: return V7_PAYLOAD_BYTES; case 8: return V8_PAYLOAD_BYTES; case SAVE_VERSION: return PAYLOAD_BYTES; default: return 0; }
}
uint32_t v2Checksum(const BuddySaveData &s) {
  uint8_t bytes[V2_PAYLOAD_BYTES]; BuddySaveData old = s; old.saveVersion = 2;
  encodeCommon(old, bytes); uint8_t *p = bytes + COMMON_PAYLOAD_BYTES;
  for (GearId gear : old.equippedSlots) put32(p, static_cast<uint8_t>(gear));
  return hashBytes(bytes, sizeof(bytes));
}
uint32_t v3Checksum(const BuddySaveData &s) {
  uint8_t bytes[V5_PAYLOAD_BYTES]; BuddySaveData old = s; old.saveVersion = 3;
  encodeCurrent(old, bytes); return hashBytes(bytes, V3_PAYLOAD_BYTES);
}
uint32_t v4Checksum(const BuddySaveData &s) {
  uint8_t bytes[V5_PAYLOAD_BYTES]; BuddySaveData old = s; old.saveVersion = 4;
  encodeCurrent(old, bytes); return hashBytes(bytes, V4_PAYLOAD_BYTES);
}
uint32_t v5Checksum(const BuddySaveData &s) {
  uint8_t bytes[V5_PAYLOAD_BYTES];
  encodeCurrent(s, bytes); uint8_t *version = bytes; put32(version, 5); return hashBytes(bytes, sizeof(bytes));
}
uint32_t v6Checksum(const BuddySaveData &s) {
  uint8_t bytes[V6_PAYLOAD_BYTES];
  encodeCurrent(s, bytes); encodeEvents(s, bytes + V5_PAYLOAD_BYTES);
  uint8_t *version = bytes; put32(version, 6);
  return hashBytes(bytes, sizeof(bytes));
}
uint32_t v7Checksum(const BuddySaveData &s) {
  uint8_t bytes[V7_PAYLOAD_BYTES];
  encodeCurrent(s, bytes); encodeEvents(s, bytes + V5_PAYLOAD_BYTES); encodeLocations(s, bytes + V6_PAYLOAD_BYTES);
  uint8_t *version = bytes; put32(version, 7);
  return hashBytes(bytes, sizeof(bytes));
}
uint32_t v8Checksum(const BuddySaveData &s) {
  uint8_t bytes[V8_PAYLOAD_BYTES];
  encodeCurrent(s, bytes); encodeEvents(s, bytes + V5_PAYLOAD_BYTES);
  encodeLocations(s, bytes + V6_PAYLOAD_BYTES); encodeVariants(s, bytes + V7_PAYLOAD_BYTES);
  uint8_t *version = bytes; put32(version, 8);
  return hashBytes(bytes, sizeof(bytes));
}
bool decodeSupported(uint32_t version, const uint8_t *p, size_t length, BuddySaveData &s) {
  if (length != payloadSize(version)) return false;
  s = BuddySaveData{}; s.soundEnabled = SOUND_ENABLED;
  if (get32(p) != version) return false; s.createdAt = get64(p); s.totalObservations = get64(p);
  s.uniqueDaysObserved = get32(p); s.latestObservationAt = get64(p); s.lastObservedDate = get32(p);
  s.latestTemperatureMilliC = int32_t(get32(p)); s.latestWeatherCode = int32_t(get32(p));
  uint32_t category = get32(p);
  if (category >= WEATHER_CATEGORY_COUNT && category != 255) return false;
  s.latestCategory = static_cast<WeatherCategory>(category);
  s.highestTemperatureMilliC = int32_t(get32(p)); s.highestTemperatureAt = get64(p);
  s.lowestTemperatureMilliC = int32_t(get32(p)); s.lowestTemperatureAt = get64(p);
  for (uint64_t &count : s.weatherCounts) count = get64(p);
  s.discoveredWeather = get32(p); s.unlockedGear = get32(p);
  if (version == 1) {
    uint32_t gear = get32(p);
    if (gear > 7) return false;
    if (gear != 0) s.equippedSlots[static_cast<uint8_t>(gearSlot(static_cast<GearId>(gear)))] = static_cast<GearId>(gear);
  } else {
    for (GearId &gear : s.equippedSlots) {
      uint32_t id = get32(p); if (id > 7) return false;
      gear = static_cast<GearId>(id);
    }
  }
  if (version < 3) {
    s.furPalette = FurPaletteId::ORANGE; s.setupComplete = true;
  } else {
    uint32_t palette = get32(p), complete = get32(p);
    if (palette >= FUR_PALETTE_COUNT || complete > 1) return false;
    s.furPalette = static_cast<FurPaletteId>(palette); s.setupComplete = complete == 1;
  }
  if (version >= 4) {
    uint32_t sound = get32(p), units = get32(p), configured = get32(p);
    if (sound > 1 || units > 1 || configured > 1) return false;
    s.soundEnabled = sound == 1; s.units = static_cast<UnitsId>(units);
    s.locationConfigured = configured == 1;
    s.latitudeMicrodegrees = int32_t(get32(p)); s.longitudeMicrodegrees = int32_t(get32(p));
  }
  if (version < 5 && s.totalObservations) {
    if (s.lowestTemperatureMilliC < -2000 || s.highestTemperatureMilliC > 2000 ||
        s.latestTemperatureMilliC < s.lowestTemperatureMilliC || s.latestTemperatureMilliC > s.highestTemperatureMilliC) return false;
    s.latestTemperatureMilliC = fahrenheitDeciToMilliC(s.latestTemperatureMilliC);
    s.highestTemperatureMilliC = fahrenheitDeciToMilliC(s.highestTemperatureMilliC);
    s.lowestTemperatureMilliC = fahrenheitDeciToMilliC(s.lowestTemperatureMilliC);
  }
  if (version >= 5) {
    s.latestMetrics.validMask = get32(p);
    for (auto &value : s.latestMetrics.values) value = int32_t(get32(p));
    for (auto &record : s.records) { record.value = int32_t(get32(p)); record.timestamp = get64(p); }
  }
  if (version >= 6) {
    for (auto &event : s.fieldEvents) { event.count = get32(p); event.firstAt = get64(p); event.latestAt = get64(p); }
    for (auto &alert : s.recentAlerts) { alert.hash = get64(p); alert.seenAt = get64(p); }
  }
  if (version >= 7) {
    for (auto &location : s.locations) {
      uint32_t flags = get32(p); if (flags > 3) return false;
      location.used = flags & 1; location.visited = flags & 2;
      memcpy(location.name, p, LOCATION_NAME_BYTES); p += LOCATION_NAME_BYTES;
      location.latitudeMicrodegrees = int32_t(get32(p)); location.longitudeMicrodegrees = int32_t(get32(p));
    }
    auto ref = [&p](uint8_t &id) { uint32_t value = get32(p); if (value != 255 && value >= FIELD_LOCATION_COUNT) return false; id = value; return true; };
    if (!ref(s.activeLocation)) return false; s.fieldSitesVisited = get64(p);
    if (!ref(s.latestLocation) || !ref(s.highestTemperatureLocation) || !ref(s.lowestTemperatureLocation)) return false;
    for (auto &record : s.records) if (!ref(record.location)) return false;
  }
  if (version >= 8) {
    s.freezingObservations = get64(p);
    for (auto &mask : s.unlockedVariants) mask = get32(p);
    for (auto &variant : s.equippedVariants) { uint32_t value = get32(p); if (value >= MAX_GEAR_VARIANTS) return false; variant = value; }
  } else migrateVariants(s);
  if (version >= 9) {
    uint32_t count = get32(p), next = get32(p);
    if (count > FIELD_NOTE_COUNT || next >= FIELD_NOTE_COUNT) return false;
    s.fieldNoteCount = count; s.fieldNoteNext = next;
    for (auto &n : s.fieldNotes) {
      n.timestamp = get64(p); n.temperatureMilliC = int32_t(get32(p)); n.weatherCode = int32_t(get32(p));
      uint32_t category = get32(p), location = get32(p);
      if ((category >= WEATHER_CATEGORY_COUNT && category != 255) || (location >= FIELD_LOCATION_COUNT && location != 255)) return false;
      n.category = WeatherCategory(category); n.location = location; n.metrics.validMask = get32(p);
      for (auto &value : n.metrics.values) value = int32_t(get32(p));
      n.outcomes = get32(p); n.severeEvents = get32(p);
    }
  }
  if (!validateBuddySave(s)) return false;
  if (version < 7) migrateHome(s);
  return validateBuddySave(s);
}
// Supported payloads decode explicitly into the current slot model.
// Unknown versions never fall through to reset.
bool migrateSupportedSave(uint32_t version, const uint8_t *payload, size_t length, BuddySaveData &data) {
  switch (version) {
    case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case SAVE_VERSION: return decodeSupported(version, payload, length, data);
    default: return false;
  }
}

bool readSlot(Preferences &prefs, const char *key, BuddySaveData &data,
              uint64_t &slotGeneration, bool &unsupported, bool &migrated) {
  if (!prefs.isKey(key)) return false;
  size_t length = prefs.getBytesLength(key);
  if (length < HEADER_BYTES) {
    unsupported = true;
    Serial.printf("Buddy %s is truncated/unrecognized; preserving NVS.\n", key);
    return false;
  }
  uint8_t header[HEADER_BYTES];
  // Preferences getBytes requires enough space for the entire blob. Inspect
  // bounded blobs only; a differently sized future save must not be overwritten.
  if (length != RECORD_BYTES && length != HEADER_BYTES + V1_PAYLOAD_BYTES && length != HEADER_BYTES + V2_PAYLOAD_BYTES && length != HEADER_BYTES + V3_PAYLOAD_BYTES && length != HEADER_BYTES + V4_PAYLOAD_BYTES && length != HEADER_BYTES + V5_PAYLOAD_BYTES && length != HEADER_BYTES + V6_PAYLOAD_BYTES && length != HEADER_BYTES + V7_PAYLOAD_BYTES && length != HEADER_BYTES + V8_PAYLOAD_BYTES) {
    unsupported = true;
    // Read only bounded future blobs to report their header version. Never
    // allocate arbitrary NVS lengths or infer that an unread record is absent.
    uint8_t *future = length <= 4096 ? static_cast<uint8_t *>(malloc(length)) : nullptr;
    if (future && prefs.getBytes(key, future, length) == length) {
      const uint8_t *p = future;
      get32(p);
      uint32_t version = get32(p);
      Serial.printf("ERROR: unsupported buddy saveVersion %lu/size %u in %s; preserving NVS.\n",
                    static_cast<unsigned long>(version), unsigned(length), key);
    } else Serial.printf("Buddy %s has an unsupported size (%u); preserving NVS.\n", key, unsigned(length));
    free(future);
    return false;
  }
  auto &bytes = saveBytes;
  if (prefs.getBytes(key, bytes, sizeof(bytes)) != length) {
    unsupported = true; // An unread key is not evidence that there is no buddy.
    Serial.printf("Buddy %s cannot be read; preserving NVS.\n", key);
    return false;
  }
  memcpy(header, bytes, HEADER_BYTES);
  const uint8_t *p = header;
  uint32_t magic = get32(p), version = get32(p), payloadLength = get32(p);
  slotGeneration = get64(p);
  uint32_t checksum = get32(p);
  if (payloadSize(version) == 0) {
    unsupported = true;
    Serial.printf("ERROR: unsupported buddy saveVersion %lu in %s; writes disabled.\n",
                  static_cast<unsigned long>(version), key);
    return false;
  }
  size_t expected = payloadSize(version);
  if (length != HEADER_BYTES + expected || payloadLength != expected) return false;
  uint32_t calculated = hashBytes(bytes + HEADER_BYTES, expected, hashBytes(bytes, 20));
  migrated = version < SAVE_VERSION;
  return magic == SAVE_MAGIC && slotGeneration > 0 && checksum == calculated &&
         migrateSupportedSave(version, bytes + HEADER_BYTES, expected, data);
}

// Exact legacy layout, retained only to recover FIELD RESEARCH BEGAN. Never
// write these records, convert interaction counts, or revive friendship logic.
enum class LegacyMood : uint8_t { HAPPY, CALM, CURIOUS, SLEEPY, EXCITED };
struct LegacyState { LegacyMood mood; uint8_t friendship; uint64_t interactions; int64_t createdAt; };
struct LegacyPet { uint32_t magic, version; LegacyState state; uint32_t cooldown; int64_t observedEpoch; };
struct LegacySlot { LegacyPet record; uint64_t generation; uint32_t checksum; };
bool validLegacy(const LegacyPet &r) {
  return r.magic == 0x50455431 && r.version == 1 && r.state.createdAt >= 0 &&
         r.state.friendship <= 100 && r.state.mood <= LegacyMood::EXCITED &&
         r.cooldown <= 600000 && r.observedEpoch >= 0;
}
// -1 protects an unreadable/unsupported old save; 0 means no useful old data.
int migrateLegacyPet(BuddySaveData &data) {
  Preferences prefs;
  // Namespace creation is harmless; no old key is changed or deleted.
  if (!prefs.begin("altoids-pet", false)) return -1;
  int64_t birthday = 0;
  bool birthdayValid = prefs.isKey("birthday") && prefs.getBytesLength("birthday") == sizeof(birthday) &&
      prefs.getBytes("birthday", &birthday, sizeof(birthday)) == sizeof(birthday) && birthday > 0;
  uint64_t newest = 0;
  bool found = false, unsupported = false, hadLegacyKeys = prefs.isKey("birthday");
  for (const char *key : {"state0", "state1", "state"}) {
    if (!prefs.isKey(key)) continue;
    hadLegacyKeys = true;
    size_t length = prefs.getBytesLength(key);
    LegacyPet record = {};
    uint64_t candidateGeneration = 0;
    bool valid = false;
    if (length == sizeof(LegacySlot) && strcmp(key, "state") != 0) {
      LegacySlot slot = {};
      if (prefs.getBytes(key, &slot, sizeof(slot)) != sizeof(slot)) { unsupported = true; continue; }
      record = slot.record; candidateGeneration = slot.generation;
      valid = candidateGeneration > 0 && slot.checksum == hashBytes(
          reinterpret_cast<const uint8_t *>(&slot), offsetof(LegacySlot, checksum));
    } else if (length == sizeof(LegacyPet) && strcmp(key, "state") == 0) {
      if (prefs.getBytes(key, &record, sizeof(record)) != sizeof(record)) { unsupported = true; continue; }
      valid = true;
    } else { unsupported = true; continue; }
    if (record.version != 1) { unsupported = true; continue; }
    if (valid && validLegacy(record) && (!found || candidateGeneration > newest)) {
      newest = candidateGeneration; data.createdAt = record.state.createdAt; found = true;
    }
  }
  prefs.end();
  if (unsupported) return -1;
  if (birthdayValid) { data.createdAt = birthday; found = true; }
  return hadLegacyKeys && !found ? -1 : found ? 1 : 0;
}

bool validDate(uint32_t date) {
  uint32_t year = date / 10000, month = (date / 100) % 100, day = date % 100;
  if (year < 1970 || year > 9999 || month < 1 || month > 12 || day < 1) return false;
  const uint8_t lengths[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  unsigned maxDay = lengths[month - 1];
  if (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ++maxDay;
  return day <= maxDay;
}

uint32_t dateAt(int64_t timestamp) {
  time_t epoch = static_cast<time_t>(timestamp);
  struct tm local = {};
  if (timestamp <= 0 || !localtime_r(&epoch, &local) || local.tm_year < 70 || local.tm_year > 8099) return 0;
  return uint32_t(local.tm_year + 1900) * 10000 + (local.tm_mon + 1) * 100 + local.tm_mday;
}
} // namespace

const char *weatherCategoryName(WeatherCategory category) {
  const char *names[] = {"clear", "mainlyClear", "partlyCloudy", "cloudy", "rain", "storm", "snow", "fog"};
  uint8_t id = static_cast<uint8_t>(category);
  return id < WEATHER_CATEGORY_COUNT ? names[id] : "unknown";
}

bool validateBuddySave(const BuddySaveData &s) {
  if (s.freezingObservations > s.totalObservations ||
      (s.freezingObservations && s.lowestTemperatureMilliC >= 0)) return false;
  for (uint8_t id = 1; id <= 7; ++id) {
    uint32_t mask = s.unlockedVariants[id - 1]; bool owned = s.unlockedGear & gearFlag(GearId(id));
    if ((mask & ~((1UL << gearVariantCount(GearId(id))) - 1)) || (owned ? !(mask & 1) : mask != 0)) return false;
  }
  for (uint8_t slot = 0; slot < GEAR_SLOT_COUNT; ++slot) {
    if (s.equippedSlots[slot] == GearId::NONE ? s.equippedVariants[slot] != 0 :
        !gearVariantUnlocked(s, s.equippedSlots[slot], s.equippedVariants[slot])) return false;
  }
  auto validRef = [&s](uint8_t id) { return id == UNKNOWN_LOCATION || (locationExists(s, id) && s.locations[id].visited); };
  if (s.fieldNoteCount > FIELD_NOTE_COUNT || s.fieldNoteNext >= FIELD_NOTE_COUNT ||
      s.fieldNoteCount > s.totalObservations ||
      (s.fieldNoteCount < FIELD_NOTE_COUNT && s.fieldNoteNext != s.fieldNoteCount)) return false;
  int64_t newer = 0;
  for (uint8_t age = 0; age < s.fieldNoteCount; ++age) {
    const auto &n = *fieldNoteAt(s, age);
    if (n.timestamp <= 0 || n.timestamp < s.createdAt || n.timestamp > s.latestObservationAt || !dateAt(n.timestamp) ||
        (age == 0 && n.timestamp != s.latestObservationAt) || (age && newer - n.timestamp < 60) ||
        uint8_t(n.category) >= WEATHER_CATEGORY_COUNT || n.weatherCode < 0 ||
        !(s.discoveredWeather & (1UL << uint8_t(n.category))) ||
        n.temperatureMilliC < s.lowestTemperatureMilliC || n.temperatureMilliC > s.highestTemperatureMilliC ||
        !validRef(n.location) || (n.metrics.validMask & ~31UL) || (n.outcomes & ~FIELD_NOTE_OUTCOME_MASK) ||
        (n.severeEvents & ~((1UL << FIELD_EVENT_COUNT) - 1)) ||
        bool(n.outcomes & NEW_SEVERE_EVENT) != bool(n.severeEvents)) return false;
    for (uint8_t i = 0; i < METRIC_COUNT; ++i)
      if (n.metrics.has(MetricId(i)) ? !validMetric(MetricId(i), n.metrics.values[i]) : n.metrics.values[i] != 0) return false;
    for (uint8_t i = 0; i < FIELD_EVENT_COUNT; ++i)
      if ((n.severeEvents & (1UL << i)) && (!s.fieldEvents[i].count || s.fieldEvents[i].firstAt != n.timestamp)) return false;
    if (age == 0 && (n.temperatureMilliC != s.latestTemperatureMilliC || n.weatherCode != s.latestWeatherCode ||
        n.category != s.latestCategory || n.location != s.latestLocation || n.metrics.validMask != s.latestMetrics.validMask)) return false;
    if (age == 0) for (uint8_t i = 0; i < METRIC_COUNT; ++i) if (n.metrics.values[i] != s.latestMetrics.values[i]) return false;
    const uint32_t metricFlags[METRIC_COUNT] = {NEW_HUMIDITY_RECORD, NEW_WIND_RECORD, NEW_GUST_RECORD, NEW_PRESSURE_RECORD, NEW_PRECIP_RECORD};
    for (uint8_t i = 0; i < METRIC_COUNT; ++i) if ((n.outcomes & metricFlags[i]) && !n.metrics.has(MetricId(i))) return false;
    newer = n.timestamp;
  }
  // Unused slots have one canonical representation, including default UNKNOWN IDs.
  for (uint8_t i = s.fieldNoteCount; i < FIELD_NOTE_COUNT; ++i) {
    const auto &n = s.fieldNotes[i];
    if (n.timestamp || n.temperatureMilliC || n.weatherCode != -1 || n.category != WeatherCategory::UNKNOWN ||
        n.location != UNKNOWN_LOCATION || n.metrics.validMask || n.outcomes || n.severeEvents) return false;
    for (auto value : n.metrics.values) if (value) return false;
  }
  unsigned used = 0, visited = 0;
  for (const auto &location : s.locations) {
    if (location.used) {
      ++used; if (location.visited) ++visited;
      if (!validLocationName(location.name) || location.latitudeMicrodegrees < -90000000 || location.latitudeMicrodegrees > 90000000 ||
          location.longitudeMicrodegrees < -180000000 || location.longitudeMicrodegrees > 180000000) return false;
      // Canonical zero padding prevents hidden/unexported name bytes affecting checksums.
      for (size_t i = strlen(location.name) + 1; i < LOCATION_NAME_BYTES; ++i) if (location.name[i]) return false;
    } else {
      if (location.visited || location.latitudeMicrodegrees || location.longitudeMicrodegrees) return false;
      for (char c : location.name) if (c) return false;
    }
  }
  if (visited > s.fieldSitesVisited || s.fieldSitesVisited > s.totalObservations ||
      (used ? !locationExists(s, s.activeLocation) : s.activeLocation != UNKNOWN_LOCATION) ||
      !validRef(s.latestLocation) || !validRef(s.highestTemperatureLocation) || !validRef(s.lowestTemperatureLocation) ||
      (!s.totalObservations && (s.latestLocation != UNKNOWN_LOCATION || s.highestTemperatureLocation != UNKNOWN_LOCATION || s.lowestTemperatureLocation != UNKNOWN_LOCATION))) return false;
  for (const auto &record : s.records) if (!validRef(record.location) || (!record.timestamp && record.location != UNKNOWN_LOCATION)) return false;
  if (used && s.locationConfigured && (s.latitudeMicrodegrees != s.locations[s.activeLocation].latitudeMicrodegrees || s.longitudeMicrodegrees != s.locations[s.activeLocation].longitudeMicrodegrees)) return false;
  for (const auto &event : s.fieldEvents) {
    if (event.count == 0) { if (event.firstAt != 0 || event.latestAt != 0) return false; }
    else if (s.totalObservations == 0 || event.firstAt <= 0 || event.firstAt < s.createdAt ||
             event.latestAt < event.firstAt || event.latestAt > s.latestObservationAt) return false;
  }
  for (uint8_t i = 0; i < RECENT_ALERT_COUNT; ++i) {
    const auto &alert = s.recentAlerts[i];
    if (alert.hash == 0) { if (alert.seenAt != 0) return false; }
    else {
      if (s.totalObservations == 0 || alert.seenAt <= 0 || alert.seenAt < s.createdAt || alert.seenAt > s.latestObservationAt) return false;
      for (uint8_t j = 0; j < i; ++j) if (s.recentAlerts[j].hash == alert.hash) return false;
    }
  }
  if (s.latestMetrics.validMask & ~31UL) return false;
  for (uint8_t i = 0; i < METRIC_COUNT; ++i) {
    bool present = s.latestMetrics.has(static_cast<MetricId>(i));
    if ((present && (s.totalObservations == 0 || !validMetric(static_cast<MetricId>(i), s.latestMetrics.values[i]))) ||
        (!present && s.latestMetrics.values[i] != 0)) return false;
  }
  for (uint8_t i = 0; i < RECORD_COUNT; ++i) {
    const auto &record = s.records[i]; MetricId metric = recordMetric(static_cast<RecordId>(i));
    if (record.timestamp == 0) {
      if (record.value != 0 || s.latestMetrics.has(metric)) return false;
    } else {
      if (record.timestamp < s.createdAt || record.timestamp > s.latestObservationAt ||
          !validMetric(metric, record.value)) return false;
      if (s.latestMetrics.has(metric)) {
        int32_t latest = s.latestMetrics.values[uint8_t(metric)];
        if (i == uint8_t(RecordId::LOW_PRESSURE) ? record.value > latest : record.value < latest) return false;
      }
    }
  }
  const auto &lowPressure = s.records[uint8_t(RecordId::LOW_PRESSURE)];
  const auto &highPressure = s.records[uint8_t(RecordId::HIGH_PRESSURE)];
  if ((lowPressure.timestamp == 0) != (highPressure.timestamp == 0) || lowPressure.value > highPressure.value) return false;
  if (static_cast<uint8_t>(s.units) > 1 || s.latitudeMicrodegrees < -90000000 || s.latitudeMicrodegrees > 90000000 ||
      s.longitudeMicrodegrees < -180000000 || s.longitudeMicrodegrees > 180000000 ||
      (!s.locationConfigured && (s.latitudeMicrodegrees != 0 || s.longitudeMicrodegrees != 0))) return false;
  if (static_cast<uint8_t>(s.furPalette) >= FUR_PALETTE_COUNT || s.saveVersion != SAVE_VERSION || s.createdAt < 0 || s.latestObservationAt < 0 ||
      s.highestTemperatureAt < 0 || s.lowestTemperatureAt < 0 ||
      (s.discoveredWeather & ~0xffUL) || (s.unlockedGear & ~0x7fUL) ||
      (s.createdAt > 0 && dateAt(s.createdAt) == 0)) return false;
  uint64_t total = 0;
  uint32_t discovered = 0;
  for (uint8_t i = 0; i < WEATHER_CATEGORY_COUNT; ++i) {
    if (UINT64_MAX - total < s.weatherCounts[i]) return false;
    total += s.weatherCounts[i];
    if (s.weatherCounts[i] > 0) discovered |= 1UL << i;
  }
  if (total != s.totalObservations || discovered != s.discoveredWeather ||
      s.uniqueDaysObserved > total || s.unlockedGear != eligibleGear(s)) return false;
  for (uint8_t i = 0; i < GEAR_SLOT_COUNT; ++i) {
    GearId gear = s.equippedSlots[i];
    if (!gearFitsSlot(gear, static_cast<GearSlot>(i)) ||
        (gear != GearId::NONE && !(s.unlockedGear & gearFlag(gear)))) return false;
  }
  if (total == 0) {
    return s.uniqueDaysObserved == 0 && s.latestObservationAt == 0 && s.lastObservedDate == 0 &&
           s.latestCategory == WeatherCategory::UNKNOWN && s.latestWeatherCode == -1 &&
           s.latestTemperatureMilliC == 0 && s.highestTemperatureMilliC == 0 && s.lowestTemperatureMilliC == 0 &&
           s.highestTemperatureAt == 0 && s.lowestTemperatureAt == 0;
  }
  uint8_t category = static_cast<uint8_t>(s.latestCategory);
  return s.createdAt > 0 && s.createdAt <= s.latestObservationAt && s.uniqueDaysObserved > 0 &&
         validDate(s.lastObservedDate) && s.lastObservedDate == dateAt(s.latestObservationAt) &&
         category < WEATHER_CATEGORY_COUNT && s.weatherCounts[category] > 0 &&
         s.latestWeatherCode >= 0 && s.highestTemperatureAt >= s.createdAt && s.lowestTemperatureAt >= s.createdAt &&
         s.highestTemperatureAt <= s.latestObservationAt && s.lowestTemperatureAt <= s.latestObservationAt &&
         s.lowestTemperatureMilliC >= -130000 && s.highestTemperatureMilliC <= 100000 &&
         s.lowestTemperatureMilliC <= s.latestTemperatureMilliC && s.latestTemperatureMilliC <= s.highestTemperatureMilliC;
}

uint32_t buddySaveChecksum(const BuddySaveData &data) {
  uint8_t bytes[PAYLOAD_BYTES]; encodeCurrent(data, bytes); encodeEvents(data, bytes + V5_PAYLOAD_BYTES); encodeLocations(data, bytes + V6_PAYLOAD_BYTES); encodeVariants(data, bytes + V7_PAYLOAD_BYTES); encodeNotes(data, bytes + V8_PAYLOAD_BYTES);
  return hashBytes(bytes, sizeof(bytes));
}

SaveLoadResult loadBuddySave(BuddySaveData &data) {
  writable = false; generation = 0; data = BuddySaveData{}; data.soundEnabled = SOUND_ENABLED;
  Preferences prefs;
  if (!prefs.begin("fwf-buddy", false)) {
    Serial.println("ERROR: Buddy NVS unavailable; no progress will be overwritten.");
    return SaveLoadResult::UNAVAILABLE;
  }
  bool found = false, unsupported = false, selectedMigrated = false;
  for (const char *key : {"save0", "save1"}) {
    BuddySaveData candidate;
    uint64_t candidateGeneration = 0;
    bool migrated = false;
    if (readSlot(prefs, key, candidate, candidateGeneration, unsupported, migrated) &&
        (!found || candidateGeneration > generation)) {
      data = candidate; generation = candidateGeneration; found = true; selectedMigrated = migrated;
    } else if (prefs.isKey(key)) Serial.printf("Buddy %s was not selected (invalid or older).\n", key);
  }
  bool hadBuddyKeys = prefs.isKey("save0") || prefs.isKey("save1");
  prefs.end();
  if (unsupported || (hadBuddyKeys && !found)) return SaveLoadResult::PROTECTED;
  if (found) { writable = true; return selectedMigrated ? SaveLoadResult::MIGRATED_SAVE : SaveLoadResult::LOADED; }
  int legacy = migrateLegacyPet(data);
  if (legacy < 0) {
    Serial.println("ERROR: Legacy pet save unavailable/unsupported; preserving data, writes disabled.");
    return SaveLoadResult::PROTECTED;
  }
  // Existing legacy buddies skip setup; their birthday remains authoritative.
  data.setupComplete = legacy > 0;
  writable = true;
  return legacy > 0 ? SaveLoadResult::MIGRATED_PET : SaveLoadResult::NEW_BUDDY;
}

bool buddySaveWritable() { return writable; }

bool persistBuddySave(const BuddySaveData &data) {
  if (!writable || !validateBuddySave(data) || generation == UINT64_MAX) return false;
  auto &bytes = saveBytes;
  memset(bytes, 0, sizeof(bytes));
  uint8_t *p = bytes;
  uint64_t next = generation + 1;
  put32(p, SAVE_MAGIC); put32(p, data.saveVersion); put32(p, PAYLOAD_BYTES); put64(p, next);
  p += 4; encodeCurrent(data, bytes + HEADER_BYTES); encodeEvents(data, bytes + HEADER_BYTES + V5_PAYLOAD_BYTES); encodeLocations(data, bytes + HEADER_BYTES + V6_PAYLOAD_BYTES); encodeVariants(data, bytes + HEADER_BYTES + V7_PAYLOAD_BYTES); encodeNotes(data, bytes + HEADER_BYTES + V8_PAYLOAD_BYTES);
  uint32_t checksum = hashBytes(bytes + HEADER_BYTES, PAYLOAD_BYTES, hashBytes(bytes, 20));
  p = bytes + 20; put32(p, checksum);
  Preferences prefs;
  if (!prefs.begin("fwf-buddy", false)) return false;
  const char *key = next % 2 ? "save1" : "save0";
  // Each putBytes commits one atomic NVS blob; alternate slots retain fallback.
  auto &verified = saveVerified;
  bool saved = prefs.putBytes(key, bytes, sizeof(bytes)) == sizeof(bytes) &&
               prefs.getBytesLength(key) == sizeof(bytes) &&
               prefs.getBytes(key, verified, sizeof(verified)) == sizeof(verified) &&
               memcmp(bytes, verified, sizeof(bytes)) == 0;
  prefs.end();
  if (saved) generation = next;
  return saved;
}

bool serializeBuddySave(const BuddySaveData &s, Print &output) {
  if (!validateBuddySave(s)) return false;
  JsonDocument doc;
  doc["saveVersion"] = s.saveVersion;
  doc["furPalette"] = static_cast<uint8_t>(s.furPalette);
  doc["setupComplete"] = s.setupComplete;
  doc["soundEnabled"] = s.soundEnabled; doc["units"] = static_cast<uint8_t>(s.units);
  doc["locationConfigured"] = s.locationConfigured;
  doc["latitudeMicrodegrees"] = s.latitudeMicrodegrees;
  doc["longitudeMicrodegrees"] = s.longitudeMicrodegrees;
  doc["researchBeganAt"] = s.createdAt;
  doc["totalObservations"] = s.totalObservations;
  doc["uniqueDaysObserved"] = s.uniqueDaysObserved;
  doc["latestObservationAt"] = s.latestObservationAt;
  doc["lastObservedDate"] = s.lastObservedDate;
  doc["latestTemperatureMilliC"] = s.latestTemperatureMilliC;
  doc["latestWeatherCode"] = s.latestWeatherCode;
  doc["latestCategory"] = static_cast<uint8_t>(s.latestCategory);
  doc["highestTemperatureMilliC"] = s.highestTemperatureMilliC;
  doc["highestTemperatureAt"] = s.highestTemperatureAt;
  doc["lowestTemperatureMilliC"] = s.lowestTemperatureMilliC;
  doc["lowestTemperatureAt"] = s.lowestTemperatureAt;
  JsonObject latest = doc["latestMetrics"].to<JsonObject>();
  for (uint8_t i = 0; i < METRIC_COUNT; ++i) {
    const char *key = metricName(static_cast<MetricId>(i));
    if (s.latestMetrics.has(static_cast<MetricId>(i))) latest[key] = s.latestMetrics.values[i];
    else latest[key] = nullptr;
  }
  JsonObject records = doc["records"].to<JsonObject>();
  for (uint8_t i = 0; i < RECORD_COUNT; ++i) {
    JsonObject record = records[recordName(static_cast<RecordId>(i))].to<JsonObject>();
    record["value"] = s.records[i].value; record["timestamp"] = s.records[i].timestamp; record["location"] = s.records[i].location;
  }
  JsonObject counts = doc["weatherCounts"].to<JsonObject>();
  for (uint8_t i = 0; i < WEATHER_CATEGORY_COUNT; ++i) counts[weatherCategoryName(static_cast<WeatherCategory>(i))] = s.weatherCounts[i];
  doc["discoveredWeather"] = s.discoveredWeather;
  doc["unlockedGear"] = s.unlockedGear;
  JsonObject slots = doc["equippedSlots"].to<JsonObject>();
  for (uint8_t i = 0; i < GEAR_SLOT_COUNT; ++i)
    slots[gearSlotName(static_cast<GearSlot>(i))] = static_cast<uint8_t>(s.equippedSlots[i]);
  JsonArray events = doc["fieldEvents"].to<JsonArray>();
  for (uint8_t i = 0; i < FIELD_EVENT_COUNT; ++i) {
    JsonObject e = events.add<JsonObject>(); const auto &record = s.fieldEvents[i];
    e["id"] = i; e["name"] = fieldEventName(FieldEventId(i)); e["discovered"] = record.count > 0;
    e["count"] = record.count; e["firstAt"] = record.firstAt; e["latestAt"] = record.latestAt;
  }
  JsonArray recent = doc["recentAlerts"].to<JsonArray>();
  for (const auto &alert : s.recentAlerts) {
    JsonArray a = recent.add<JsonArray>();
    char hash[17]; snprintf(hash, sizeof(hash), "%016llx", (unsigned long long)alert.hash);
    a.add(hash); a.add(alert.seenAt);
  }
  JsonArray locations = doc["fieldLocations"].to<JsonArray>();
  for (uint8_t i = 0; i < FIELD_LOCATION_COUNT; ++i) {
    if (!s.locations[i].used) continue;
    const auto &site = s.locations[i]; JsonObject location = locations.add<JsonObject>();
    location["id"] = i; location["name"] = site.name;
    location["latitudeMicrodegrees"] = site.latitudeMicrodegrees; location["longitudeMicrodegrees"] = site.longitudeMicrodegrees;
    location["visited"] = site.visited;
  }
  doc["activeLocation"] = s.activeLocation; doc["fieldSitesVisited"] = s.fieldSitesVisited;
  doc["latestLocation"] = s.latestLocation;
  doc["highestTemperatureLocation"] = s.highestTemperatureLocation; doc["lowestTemperatureLocation"] = s.lowestTemperatureLocation;
  doc["freezingObservations"] = s.freezingObservations;
  JsonObject variants = doc["unlockedVariants"].to<JsonObject>();
  for (uint8_t id = 1; id <= 7; ++id) variants[gearName(GearId(id))] = s.unlockedVariants[id - 1];
  JsonObject equipped = doc["equippedVariants"].to<JsonObject>();
  for (uint8_t slot = 0; slot < GEAR_SLOT_COUNT; ++slot) equipped[gearSlotName(GearSlot(slot))] = s.equippedVariants[slot];
  doc["fieldNoteNext"] = s.fieldNoteNext;
  JsonArray notes = doc["fieldNotes"].to<JsonArray>(); // Newest first; no unused slots exported.
  for (uint8_t age = 0; age < s.fieldNoteCount; ++age) {
    const auto &n = *fieldNoteAt(s, age); JsonObject note = notes.add<JsonObject>();
    note["timestamp"] = n.timestamp; note["temperatureMilliC"] = n.temperatureMilliC;
    note["weatherCode"] = n.weatherCode; note["category"] = uint8_t(n.category); note["location"] = n.location;
    note["validMetrics"] = n.metrics.validMask;
    JsonObject metrics = note["metrics"].to<JsonObject>();
    for (uint8_t i = 0; i < METRIC_COUNT; ++i) {
      if (n.metrics.has(MetricId(i))) metrics[metricName(MetricId(i))] = n.metrics.values[i];
      else metrics[metricName(MetricId(i))] = nullptr;
    }
    note["outcomes"] = n.outcomes; note["severeEvents"] = n.severeEvents;
  }
  char checksum[9]; snprintf(checksum, sizeof(checksum), "%08lx", static_cast<unsigned long>(buddySaveChecksum(s)));
  doc["checksum"] = checksum;
  if (doc.overflowed()) return false;
  return serializeJsonPretty(doc, output) > 0;
}

bool deserializeBuddySave(const char *json, size_t length, BuddySaveData &data, const char *&error) {
  error = "Invalid JSON";
  if (!json || length == 0 || length > BUDDY_IMPORT_BYTES) { error = "Import must be 1-24576 bytes"; return false; }
  JsonDocument doc;
  if (deserializeJson(doc, json, length, DeserializationOption::NestingLimit(4))) return false;
  if (!doc.is<JsonObject>()) return false;
  if (!doc["saveVersion"].is<uint32_t>() || payloadSize(doc["saveVersion"].as<uint32_t>()) == 0) {
    error = "Unsupported saveVersion; existing buddy preserved"; return false;
  }
  BuddySaveData s; s.soundEnabled = SOUND_ENABLED;
#define READ_FIELD(key, field, type) \
  if (!doc[key].is<type>()) { error = "Missing or invalid " key; return false; } \
  s.field = doc[key].as<type>();
  uint32_t importedVersion = doc["saveVersion"].as<uint32_t>();
  if (importedVersion < 3) {
    s.furPalette = FurPaletteId::ORANGE; s.setupComplete = true;
  } else {
    if (!doc["furPalette"].is<uint32_t>() || doc["furPalette"].as<uint32_t>() >= FUR_PALETTE_COUNT ||
        !doc["setupComplete"].is<bool>()) { error = "Invalid fur palette/setup state"; return false; }
    s.furPalette = static_cast<FurPaletteId>(doc["furPalette"].as<uint32_t>());
    s.setupComplete = doc["setupComplete"].as<bool>();
  }
  if (importedVersion >= 4) {
    if (!doc["soundEnabled"].is<bool>() || !doc["units"].is<uint32_t>() || doc["units"].as<uint32_t>() > 1 ||
        !doc["locationConfigured"].is<bool>()) { error = "Invalid settings"; return false; }
    s.soundEnabled = doc["soundEnabled"].as<bool>(); s.units = static_cast<UnitsId>(doc["units"].as<uint32_t>());
    s.locationConfigured = doc["locationConfigured"].as<bool>();
    READ_FIELD("latitudeMicrodegrees", latitudeMicrodegrees, int32_t)
    READ_FIELD("longitudeMicrodegrees", longitudeMicrodegrees, int32_t)
  }
  READ_FIELD("researchBeganAt", createdAt, int64_t)
  READ_FIELD("totalObservations", totalObservations, uint64_t)
  READ_FIELD("uniqueDaysObserved", uniqueDaysObserved, uint32_t)
  READ_FIELD("latestObservationAt", latestObservationAt, int64_t)
  READ_FIELD("lastObservedDate", lastObservedDate, uint32_t)
  if (importedVersion < 5) { READ_FIELD("latestTemperatureDeciF", latestTemperatureMilliC, int32_t) }
  else { READ_FIELD("latestTemperatureMilliC", latestTemperatureMilliC, int32_t) }
  READ_FIELD("latestWeatherCode", latestWeatherCode, int32_t)
  if (importedVersion < 5) { READ_FIELD("highestTemperatureDeciF", highestTemperatureMilliC, int32_t) }
  else { READ_FIELD("highestTemperatureMilliC", highestTemperatureMilliC, int32_t) }
  READ_FIELD("highestTemperatureAt", highestTemperatureAt, int64_t)
  if (importedVersion < 5) { READ_FIELD("lowestTemperatureDeciF", lowestTemperatureMilliC, int32_t) }
  else { READ_FIELD("lowestTemperatureMilliC", lowestTemperatureMilliC, int32_t) }
  READ_FIELD("lowestTemperatureAt", lowestTemperatureAt, int64_t)
  READ_FIELD("discoveredWeather", discoveredWeather, uint32_t)
  READ_FIELD("unlockedGear", unlockedGear, uint32_t)
#undef READ_FIELD
  if (!doc["latestCategory"].is<uint32_t>()) return false;
  uint32_t category = doc["latestCategory"].as<uint32_t>();
  if (category >= WEATHER_CATEGORY_COUNT && category != 255) return false;
  s.latestCategory = static_cast<WeatherCategory>(category);
  GearId legacyGear = GearId::NONE;
  if (importedVersion == 1) {
    if (!doc["equippedGear"].is<uint32_t>() || doc["equippedGear"].as<uint32_t>() > 7) return false;
    legacyGear = static_cast<GearId>(doc["equippedGear"].as<uint32_t>());
    if (legacyGear != GearId::NONE) s.equippedSlots[static_cast<uint8_t>(gearSlot(legacyGear))] = legacyGear;
  } else {
    JsonObjectConst slots = doc["equippedSlots"].as<JsonObjectConst>();
    if (slots.isNull() || slots.size() != GEAR_SLOT_COUNT) { error = "Missing equipped slots"; return false; }
    for (uint8_t i = 0; i < GEAR_SLOT_COUNT; ++i) {
      JsonVariantConst value = slots[gearSlotName(static_cast<GearSlot>(i))];
      if (!value.is<uint32_t>() || value.as<uint32_t>() > 7) { error = "Invalid equipped slot"; return false; }
      s.equippedSlots[i] = static_cast<GearId>(value.as<uint32_t>());
    }
  }
  JsonObjectConst counts = doc["weatherCounts"].as<JsonObjectConst>();
  if (counts.isNull() || counts.size() != WEATHER_CATEGORY_COUNT) { error = "Missing weather counts"; return false; }
  for (uint8_t i = 0; i < WEATHER_CATEGORY_COUNT; ++i) {
    JsonVariantConst value = counts[weatherCategoryName(static_cast<WeatherCategory>(i))];
    if (!value.is<uint64_t>()) { error = "Invalid weather count"; return false; }
    s.weatherCounts[i] = value.as<uint64_t>();
  }
  if (importedVersion < 5 && s.totalObservations) {
    if (s.lowestTemperatureMilliC < -2000 || s.highestTemperatureMilliC > 2000 ||
        s.latestTemperatureMilliC < s.lowestTemperatureMilliC || s.latestTemperatureMilliC > s.highestTemperatureMilliC) return false;
    s.latestTemperatureMilliC = fahrenheitDeciToMilliC(s.latestTemperatureMilliC);
    s.highestTemperatureMilliC = fahrenheitDeciToMilliC(s.highestTemperatureMilliC);
    s.lowestTemperatureMilliC = fahrenheitDeciToMilliC(s.lowestTemperatureMilliC);
  }
  if (importedVersion >= 5) {
    JsonObjectConst latest = doc["latestMetrics"].as<JsonObjectConst>();
    JsonObjectConst records = doc["records"].as<JsonObjectConst>();
    if (latest.isNull() || latest.size() != METRIC_COUNT || records.isNull() || records.size() != RECORD_COUNT) {
      error = "Missing metric/record fields"; return false;
    }
    for (uint8_t i = 0; i < METRIC_COUNT; ++i) {
      const char *key = metricName(static_cast<MetricId>(i));
      if (latest[key].isUnbound()) return false;
      auto value = latest[key];
      if (!value.isNull()) {
        if (!value.is<int32_t>()) return false;
        s.latestMetrics.validMask |= 1UL << i; s.latestMetrics.values[i] = value.as<int32_t>();
      }
    }
    for (uint8_t i = 0; i < RECORD_COUNT; ++i) {
      JsonObjectConst record = records[recordName(static_cast<RecordId>(i))].as<JsonObjectConst>();
      if (record.isNull() || record.size() != (importedVersion >= 7 ? 3 : 2) || !record["value"].is<int32_t>() || !record["timestamp"].is<int64_t>()) return false;
      s.records[i].value = record["value"].as<int32_t>(); s.records[i].timestamp = record["timestamp"].as<int64_t>();
    }
  }
  if (importedVersion >= 6) {
    JsonArrayConst events = doc["fieldEvents"].as<JsonArrayConst>();
    JsonArrayConst recent = doc["recentAlerts"].as<JsonArrayConst>();
    if (events.isNull() || events.size() != FIELD_EVENT_COUNT || recent.isNull() || recent.size() > RECENT_ALERT_COUNT) return false;
    uint32_t ids = 0;
    for (JsonObjectConst e : events) {
      if (e.size() != 6 || !e["id"].is<uint32_t>() || e["id"].as<uint32_t>() >= FIELD_EVENT_COUNT ||
          !e["name"].is<const char *>() || !e["discovered"].is<bool>() || !e["count"].is<uint32_t>() ||
          !e["firstAt"].is<int64_t>() || !e["latestAt"].is<int64_t>()) return false;
      uint8_t id = e["id"].as<uint8_t>();
      if ((ids & (1UL << id)) || strcmp(e["name"], fieldEventName(FieldEventId(id))) ||
          e["discovered"].as<bool>() != (e["count"].as<uint32_t>() > 0)) return false;
      ids |= 1UL << id;
      s.fieldEvents[id] = {e["count"].as<uint32_t>(), e["firstAt"].as<int64_t>(), e["latestAt"].as<int64_t>()};
    }
    uint8_t i = 0;
    for (JsonArrayConst a : recent) {
      const char *hash = a[0].as<const char *>();
      if (a.size() != 2 || !hash || strlen(hash) != 16 || !a[1].is<int64_t>()) return false;
      for (uint8_t j = 0; j < 16; ++j) if (!((hash[j] >= '0' && hash[j] <= '9') || (hash[j] >= 'a' && hash[j] <= 'f'))) return false;
      uint64_t value = strtoull(hash, nullptr, 16);
      s.recentAlerts[i++] = {value, a[1].as<int64_t>()};
    }
  }
  if (importedVersion >= 7) {
    JsonArrayConst locations = doc["fieldLocations"].as<JsonArrayConst>();
    if (locations.isNull() || locations.size() > FIELD_LOCATION_COUNT) return false;
    uint32_t ids = 0;
    for (JsonObjectConst site : locations) {
      if (site.size() != 5 || !site["id"].is<uint32_t>() || site["id"].as<uint32_t>() >= FIELD_LOCATION_COUNT ||
          !site["name"].is<const char *>() || !validLocationName(site["name"].as<const char *>()) ||
          !site["latitudeMicrodegrees"].is<int32_t>() || !site["longitudeMicrodegrees"].is<int32_t>() || !site["visited"].is<bool>()) return false;
      uint8_t id = site["id"].as<uint8_t>(); if (ids & (1UL << id)) return false; ids |= 1UL << id;
      auto &location = s.locations[id]; location.used = true; strcpy(location.name, site["name"]);
      location.latitudeMicrodegrees = site["latitudeMicrodegrees"].as<int32_t>(); location.longitudeMicrodegrees = site["longitudeMicrodegrees"].as<int32_t>(); location.visited = site["visited"].as<bool>();
    }
    auto ref = [](JsonVariantConst value, uint8_t &id) { if (!value.is<uint32_t>() || (value.as<uint32_t>() != 255 && value.as<uint32_t>() >= FIELD_LOCATION_COUNT)) return false; id = value.as<uint8_t>(); return true; };
    if (!ref(doc["activeLocation"], s.activeLocation) || !ref(doc["latestLocation"], s.latestLocation) ||
        !ref(doc["highestTemperatureLocation"], s.highestTemperatureLocation) || !ref(doc["lowestTemperatureLocation"], s.lowestTemperatureLocation) || !doc["fieldSitesVisited"].is<uint64_t>()) return false;
    s.fieldSitesVisited = doc["fieldSitesVisited"].as<uint64_t>();
    for (uint8_t i = 0; i < RECORD_COUNT; ++i) if (!ref(doc["records"][recordName(RecordId(i))]["location"], s.records[i].location)) return false;
  }
  if (importedVersion >= 8) {
    JsonObjectConst variants = doc["unlockedVariants"].as<JsonObjectConst>();
    JsonObjectConst equipped = doc["equippedVariants"].as<JsonObjectConst>();
    if (variants.isNull() || variants.size() != 7 || equipped.isNull() || equipped.size() != GEAR_SLOT_COUNT || !doc["freezingObservations"].is<uint64_t>()) return false;
    s.freezingObservations = doc["freezingObservations"].as<uint64_t>();
    for (uint8_t id = 1; id <= 7; ++id) {
      auto mask = variants[gearName(GearId(id))]; if (!mask.is<uint32_t>()) return false; s.unlockedVariants[id - 1] = mask.as<uint32_t>();
    }
    for (uint8_t slot = 0; slot < GEAR_SLOT_COUNT; ++slot) {
      auto value = equipped[gearSlotName(GearSlot(slot))]; if (!value.is<uint32_t>() || value.as<uint32_t>() >= MAX_GEAR_VARIANTS) return false;
      s.equippedVariants[slot] = value.as<uint8_t>();
    }
  } else migrateVariants(s);
  if (importedVersion >= 9) {
    JsonArrayConst notes = doc["fieldNotes"].as<JsonArrayConst>();
    if (notes.isNull() || notes.size() > FIELD_NOTE_COUNT || !doc["fieldNoteNext"].is<uint32_t>() ||
        doc["fieldNoteNext"].as<uint32_t>() >= FIELD_NOTE_COUNT) { error = "Invalid field-note ring"; return false; }
    s.fieldNoteCount = notes.size(); s.fieldNoteNext = doc["fieldNoteNext"].as<uint8_t>();
    uint8_t age = 0;
    for (JsonObjectConst note : notes) {
      if (note.size() != 9 || !note["timestamp"].is<int64_t>() || !note["temperatureMilliC"].is<int32_t>() ||
          !note["weatherCode"].is<int32_t>() || !note["category"].is<uint32_t>() || note["category"].as<uint32_t>() >= WEATHER_CATEGORY_COUNT ||
          !note["location"].is<uint32_t>() || (note["location"].as<uint32_t>() != 255 && note["location"].as<uint32_t>() >= FIELD_LOCATION_COUNT) ||
          !note["validMetrics"].is<uint32_t>() || !note["outcomes"].is<uint32_t>() || !note["severeEvents"].is<uint32_t>()) { error = "Invalid field note"; return false; }
      FieldNote &n = s.fieldNotes[(s.fieldNoteNext + FIELD_NOTE_COUNT - 1 - age++) % FIELD_NOTE_COUNT];
      n.timestamp = note["timestamp"].as<int64_t>(); n.temperatureMilliC = note["temperatureMilliC"].as<int32_t>();
      n.weatherCode = note["weatherCode"].as<int32_t>(); n.category = WeatherCategory(note["category"].as<uint8_t>());
      n.location = note["location"].as<uint8_t>(); n.metrics.validMask = note["validMetrics"].as<uint32_t>();
      n.outcomes = note["outcomes"].as<uint32_t>(); n.severeEvents = note["severeEvents"].as<uint32_t>();
      JsonObjectConst metrics = note["metrics"].as<JsonObjectConst>();
      if (metrics.isNull() || metrics.size() != METRIC_COUNT) return false;
      for (uint8_t i = 0; i < METRIC_COUNT; ++i) {
        auto value = metrics[metricName(MetricId(i))];
        if (value.isUnbound() || (n.metrics.has(MetricId(i)) ? !value.is<int32_t>() : !value.isNull())) return false;
        if (n.metrics.has(MetricId(i))) n.metrics.values[i] = value.as<int32_t>();
      }
    }
  }
  if (!validateBuddySave(s)) { error = "Inconsistent journal, records or gear"; return false; }
  const char *text = doc["checksum"].as<const char *>();
  if (!text || strlen(text) != 8) { error = "Missing checksum"; return false; }
  for (size_t i = 0; i < 8; ++i) {
    char c = text[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
      error = "Invalid checksum"; return false;
    }
  }
  if (strtoul(text, nullptr, 16) != (importedVersion == 1 ? legacyChecksum(s, legacyGear) : importedVersion == 2 ? v2Checksum(s) : importedVersion == 3 ? v3Checksum(s) : importedVersion == 4 ? v4Checksum(s) : importedVersion == 5 ? v5Checksum(s) : importedVersion == 6 ? v6Checksum(s) : importedVersion == 7 ? v7Checksum(s) : importedVersion == 8 ? v8Checksum(s) : buddySaveChecksum(s))) { error = "Checksum mismatch"; return false; }
  if (importedVersion < 7) migrateHome(s);
  data = s; error = nullptr; return true;
}

const char *metricName(MetricId id) {
  const char *names[] = {"humidityCentiPercent", "windCentiKmh", "gustCentiKmh", "pressureCentiHpa", "precipitationCentiMm"};
  return uint8_t(id) < METRIC_COUNT ? names[uint8_t(id)] : "unknown";
}
const char *recordName(RecordId id) {
  const char *names[] = {"strongestWind", "strongestGust", "highestHumidity", "lowestPressure", "highestPressure", "wettestObservation"};
  return uint8_t(id) < RECORD_COUNT ? names[uint8_t(id)] : "unknown";
}
MetricId recordMetric(RecordId id) {
  const MetricId metrics[] = {MetricId::WIND, MetricId::GUST, MetricId::HUMIDITY, MetricId::PRESSURE, MetricId::PRESSURE, MetricId::PRECIPITATION};
  return uint8_t(id) < RECORD_COUNT ? metrics[uint8_t(id)] : MetricId::COUNT;
}
bool validMetric(MetricId id, int32_t value) {
  switch (id) {
    case MetricId::HUMIDITY: return value >= 0 && value <= 10000;
    case MetricId::WIND: case MetricId::GUST: return value >= 0 && value <= 50000;
    case MetricId::PRESSURE: return value >= 10000 && value <= 120000;
    case MetricId::PRECIPITATION: return value >= 0 && value <= 100000;
    default: return false;
  }
}

const char *fieldEventName(FieldEventId id) {
  static const char *names[] = {
    "Tornado Watch", "Tornado Warning", "Severe Thunderstorm Watch", "Severe Thunderstorm Warning",
    "Flash Flood Warning", "Flood Warning", "Hurricane Watch", "Hurricane Warning",
    "Tropical Storm Watch", "Tropical Storm Warning", "Winter Storm Warning", "Blizzard Warning",
    "Ice Storm Warning", "Extreme Heat Warning", "Extreme Cold Warning"
  };
  return uint8_t(id) < FIELD_EVENT_COUNT ? names[uint8_t(id)] : "Unknown";
}

void initializeDefaultFieldLocation(BuddySaveData &data) { migrateHome(data); }
