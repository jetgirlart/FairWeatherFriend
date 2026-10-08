#include "save.h"
#include "gear.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <stddef.h>
#include <initializer_list>
#include <string.h>

namespace {
constexpr uint32_t SAVE_MAGIC = 0x46574631; // FWF1 envelope, stable across versions.
constexpr size_t PAYLOAD_BYTES = 148;
constexpr size_t HEADER_BYTES = 24;
constexpr size_t RECORD_BYTES = HEADER_BYTES + PAYLOAD_BYTES;
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
void encodeV1(const BuddySaveData &s, uint8_t *p) {
  put32(p, s.saveVersion); put64(p, s.createdAt); put64(p, s.totalObservations);
  put32(p, s.uniqueDaysObserved); put64(p, s.latestObservationAt); put32(p, s.lastObservedDate);
  put32(p, s.latestTemperatureDeciF); put32(p, s.latestWeatherCode);
  put32(p, static_cast<uint8_t>(s.latestCategory));
  put32(p, s.highestTemperatureDeciF); put64(p, s.highestTemperatureAt);
  put32(p, s.lowestTemperatureDeciF); put64(p, s.lowestTemperatureAt);
  for (uint64_t count : s.weatherCounts) put64(p, count);
  put32(p, s.discoveredWeather); put32(p, s.unlockedGear);
  put32(p, static_cast<uint8_t>(s.equippedGear));
}
bool decodeV1(const uint8_t *p, size_t length, BuddySaveData &s) {
  if (length != PAYLOAD_BYTES) return false;
  s.saveVersion = get32(p); s.createdAt = get64(p); s.totalObservations = get64(p);
  s.uniqueDaysObserved = get32(p); s.latestObservationAt = get64(p); s.lastObservedDate = get32(p);
  s.latestTemperatureDeciF = int32_t(get32(p)); s.latestWeatherCode = int32_t(get32(p));
  uint32_t category = get32(p);
  if (category >= WEATHER_CATEGORY_COUNT && category != 255) return false;
  s.latestCategory = static_cast<WeatherCategory>(category);
  s.highestTemperatureDeciF = int32_t(get32(p)); s.highestTemperatureAt = get64(p);
  s.lowestTemperatureDeciF = int32_t(get32(p)); s.lowestTemperatureAt = get64(p);
  for (uint64_t &count : s.weatherCounts) count = get64(p);
  s.discoveredWeather = get32(p); s.unlockedGear = get32(p);
  uint32_t gear = get32(p);
  if (gear > 7) return false;
  s.equippedGear = static_cast<GearId>(gear);
  return validateBuddySave(s);
}
// Migration dispatch: future versions decode their old payload explicitly,
// then call migrateV1ToV2(), etc. Unknown versions never fall through to reset.
bool migrateSupportedSave(uint32_t version, const uint8_t *payload, size_t length, BuddySaveData &data) {
  switch (version) {
    case 1: return decodeV1(payload, length, data);
    default: return false;
  }
}

bool readSlot(Preferences &prefs, const char *key, BuddySaveData &data,
              uint64_t &slotGeneration, bool &unsupported) {
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
  if (length != RECORD_BYTES) {
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
  uint8_t bytes[RECORD_BYTES];
  if (prefs.getBytes(key, bytes, sizeof(bytes)) != sizeof(bytes)) {
    unsupported = true; // An unread key is not evidence that there is no buddy.
    Serial.printf("Buddy %s cannot be read; preserving NVS.\n", key);
    return false;
  }
  memcpy(header, bytes, HEADER_BYTES);
  const uint8_t *p = header;
  uint32_t magic = get32(p), version = get32(p), payloadLength = get32(p);
  slotGeneration = get64(p);
  uint32_t checksum = get32(p);
  if (version != SAVE_VERSION) {
    unsupported = true;
    Serial.printf("ERROR: unsupported buddy saveVersion %lu in %s; writes disabled.\n",
                  static_cast<unsigned long>(version), key);
    return false;
  }
  uint32_t calculated = hashBytes(bytes + HEADER_BYTES, PAYLOAD_BYTES, hashBytes(bytes, 20));
  return magic == SAVE_MAGIC && payloadLength == PAYLOAD_BYTES && slotGeneration > 0 &&
         checksum == calculated && migrateSupportedSave(version, bytes + HEADER_BYTES, PAYLOAD_BYTES, data);
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
  bool found = false, unsupported = false;
  for (const char *key : {"state0", "state1", "state"}) {
    if (!prefs.isKey(key)) continue;
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
  return found ? 1 : 0;
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
  if (s.saveVersion != SAVE_VERSION || s.createdAt < 0 || s.latestObservationAt < 0 ||
      s.highestTemperatureAt < 0 || s.lowestTemperatureAt < 0 ||
      (s.discoveredWeather & ~0xffUL) || (s.unlockedGear & ~0x7fUL) ||
      static_cast<uint8_t>(s.equippedGear) > 7 || (s.createdAt > 0 && dateAt(s.createdAt) == 0)) return false;
  uint64_t total = 0;
  uint32_t discovered = 0;
  for (uint8_t i = 0; i < WEATHER_CATEGORY_COUNT; ++i) {
    if (UINT64_MAX - total < s.weatherCounts[i]) return false;
    total += s.weatherCounts[i];
    if (s.weatherCounts[i] > 0) discovered |= 1UL << i;
  }
  if (total != s.totalObservations || discovered != s.discoveredWeather ||
      s.uniqueDaysObserved > total || s.unlockedGear != eligibleGear(s) ||
      (s.equippedGear != GearId::NONE && !(s.unlockedGear & gearFlag(s.equippedGear)))) return false;
  if (total == 0) {
    return s.uniqueDaysObserved == 0 && s.latestObservationAt == 0 && s.lastObservedDate == 0 &&
           s.latestCategory == WeatherCategory::UNKNOWN && s.latestWeatherCode == -1 &&
           s.latestTemperatureDeciF == 0 && s.highestTemperatureDeciF == 0 && s.lowestTemperatureDeciF == 0 &&
           s.highestTemperatureAt == 0 && s.lowestTemperatureAt == 0;
  }
  uint8_t category = static_cast<uint8_t>(s.latestCategory);
  return s.createdAt > 0 && s.createdAt <= s.latestObservationAt && s.uniqueDaysObserved > 0 &&
         validDate(s.lastObservedDate) && s.lastObservedDate == dateAt(s.latestObservationAt) &&
         category < WEATHER_CATEGORY_COUNT && s.weatherCounts[category] > 0 &&
         s.latestWeatherCode >= 0 && s.highestTemperatureAt >= s.createdAt && s.lowestTemperatureAt >= s.createdAt &&
         s.highestTemperatureAt <= s.latestObservationAt && s.lowestTemperatureAt <= s.latestObservationAt &&
         s.lowestTemperatureDeciF >= -2000 && s.highestTemperatureDeciF <= 2000 &&
         s.lowestTemperatureDeciF <= s.latestTemperatureDeciF && s.latestTemperatureDeciF <= s.highestTemperatureDeciF;
}

uint32_t buddySaveChecksum(const BuddySaveData &data) {
  uint8_t bytes[PAYLOAD_BYTES]; encodeV1(data, bytes);
  return hashBytes(bytes, sizeof(bytes));
}

SaveLoadResult loadBuddySave(BuddySaveData &data) {
  writable = false; generation = 0; data = BuddySaveData{};
  Preferences prefs;
  if (!prefs.begin("fwf-buddy", false)) {
    Serial.println("ERROR: Buddy NVS unavailable; no progress will be overwritten.");
    return SaveLoadResult::UNAVAILABLE;
  }
  bool found = false, unsupported = false;
  for (const char *key : {"save0", "save1"}) {
    BuddySaveData candidate;
    uint64_t candidateGeneration = 0;
    if (readSlot(prefs, key, candidate, candidateGeneration, unsupported) &&
        (!found || candidateGeneration > generation)) {
      data = candidate; generation = candidateGeneration; found = true;
    } else if (prefs.isKey(key)) Serial.printf("Buddy %s was not selected (invalid or older).\n", key);
  }
  prefs.end();
  if (unsupported) return SaveLoadResult::PROTECTED;
  if (found) { writable = true; return SaveLoadResult::LOADED; }
  int legacy = migrateLegacyPet(data);
  if (legacy < 0) {
    Serial.println("ERROR: Legacy pet save unavailable/unsupported; preserving data, writes disabled.");
    return SaveLoadResult::PROTECTED;
  }
  writable = true;
  return legacy > 0 ? SaveLoadResult::MIGRATED_PET : SaveLoadResult::NEW_BUDDY;
}

bool buddySaveWritable() { return writable; }

bool persistBuddySave(const BuddySaveData &data) {
  if (!writable || !validateBuddySave(data) || generation == UINT64_MAX) return false;
  uint8_t bytes[RECORD_BYTES] = {};
  uint8_t *p = bytes;
  uint64_t next = generation + 1;
  put32(p, SAVE_MAGIC); put32(p, data.saveVersion); put32(p, PAYLOAD_BYTES); put64(p, next);
  p += 4; encodeV1(data, bytes + HEADER_BYTES);
  uint32_t checksum = hashBytes(bytes + HEADER_BYTES, PAYLOAD_BYTES, hashBytes(bytes, 20));
  p = bytes + 20; put32(p, checksum);
  Preferences prefs;
  if (!prefs.begin("fwf-buddy", false)) return false;
  const char *key = next % 2 ? "save1" : "save0";
  // Each putBytes commits one atomic NVS blob; alternate slots retain fallback.
  uint8_t verified[RECORD_BYTES];
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
  doc["researchBeganAt"] = s.createdAt;
  doc["totalObservations"] = s.totalObservations;
  doc["uniqueDaysObserved"] = s.uniqueDaysObserved;
  doc["latestObservationAt"] = s.latestObservationAt;
  doc["lastObservedDate"] = s.lastObservedDate;
  doc["latestTemperatureDeciF"] = s.latestTemperatureDeciF;
  doc["latestWeatherCode"] = s.latestWeatherCode;
  doc["latestCategory"] = static_cast<uint8_t>(s.latestCategory);
  doc["highestTemperatureDeciF"] = s.highestTemperatureDeciF;
  doc["highestTemperatureAt"] = s.highestTemperatureAt;
  doc["lowestTemperatureDeciF"] = s.lowestTemperatureDeciF;
  doc["lowestTemperatureAt"] = s.lowestTemperatureAt;
  JsonObject counts = doc["weatherCounts"].to<JsonObject>();
  for (uint8_t i = 0; i < WEATHER_CATEGORY_COUNT; ++i) counts[weatherCategoryName(static_cast<WeatherCategory>(i))] = s.weatherCounts[i];
  doc["discoveredWeather"] = s.discoveredWeather;
  doc["unlockedGear"] = s.unlockedGear;
  doc["equippedGear"] = static_cast<uint8_t>(s.equippedGear);
  char checksum[9]; snprintf(checksum, sizeof(checksum), "%08lx", static_cast<unsigned long>(buddySaveChecksum(s)));
  doc["checksum"] = checksum;
  if (doc.overflowed()) return false;
  return serializeJsonPretty(doc, output) > 0;
}

bool deserializeBuddySave(const char *json, size_t length, BuddySaveData &data, const char *&error) {
  error = "Invalid JSON";
  if (!json || length == 0 || length > 4096) { error = "Import must be 1-4096 bytes"; return false; }
  JsonDocument doc;
  if (deserializeJson(doc, json, length, DeserializationOption::NestingLimit(4))) return false;
  if (!doc.is<JsonObject>()) return false;
  if (!doc["saveVersion"].is<uint32_t>() || doc["saveVersion"].as<uint32_t>() != SAVE_VERSION) {
    error = "Unsupported saveVersion; existing buddy preserved"; return false;
  }
  BuddySaveData s;
#define READ_FIELD(key, field, type) \
  if (!doc[key].is<type>()) { error = "Missing or invalid " key; return false; } \
  s.field = doc[key].as<type>();
  READ_FIELD("saveVersion", saveVersion, uint32_t)
  READ_FIELD("researchBeganAt", createdAt, int64_t)
  READ_FIELD("totalObservations", totalObservations, uint64_t)
  READ_FIELD("uniqueDaysObserved", uniqueDaysObserved, uint32_t)
  READ_FIELD("latestObservationAt", latestObservationAt, int64_t)
  READ_FIELD("lastObservedDate", lastObservedDate, uint32_t)
  READ_FIELD("latestTemperatureDeciF", latestTemperatureDeciF, int32_t)
  READ_FIELD("latestWeatherCode", latestWeatherCode, int32_t)
  READ_FIELD("highestTemperatureDeciF", highestTemperatureDeciF, int32_t)
  READ_FIELD("highestTemperatureAt", highestTemperatureAt, int64_t)
  READ_FIELD("lowestTemperatureDeciF", lowestTemperatureDeciF, int32_t)
  READ_FIELD("lowestTemperatureAt", lowestTemperatureAt, int64_t)
  READ_FIELD("discoveredWeather", discoveredWeather, uint32_t)
  READ_FIELD("unlockedGear", unlockedGear, uint32_t)
#undef READ_FIELD
  if (!doc["latestCategory"].is<uint32_t>() || !doc["equippedGear"].is<uint32_t>()) return false;
  uint32_t category = doc["latestCategory"].as<uint32_t>(), gear = doc["equippedGear"].as<uint32_t>();
  if ((category >= WEATHER_CATEGORY_COUNT && category != 255) || gear > 7) return false;
  s.latestCategory = static_cast<WeatherCategory>(category); s.equippedGear = static_cast<GearId>(gear);
  JsonObjectConst counts = doc["weatherCounts"].as<JsonObjectConst>();
  if (counts.isNull() || counts.size() != WEATHER_CATEGORY_COUNT) { error = "Missing weather counts"; return false; }
  for (uint8_t i = 0; i < WEATHER_CATEGORY_COUNT; ++i) {
    JsonVariantConst value = counts[weatherCategoryName(static_cast<WeatherCategory>(i))];
    if (!value.is<uint64_t>()) { error = "Invalid weather count"; return false; }
    s.weatherCounts[i] = value.as<uint64_t>();
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
  if (strtoul(text, nullptr, 16) != buddySaveChecksum(s)) { error = "Checksum mismatch"; return false; }
  data = s; error = nullptr; return true;
}
