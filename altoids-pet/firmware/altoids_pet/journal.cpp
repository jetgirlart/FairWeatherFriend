#include "journal.h"
#include "achievements.h"
#include "sound.h"
#include "field_notes.h"
#include "gear.h"
#include "gear_variants.h"
#include "field_locations.h"
#include <ArduinoJson.h>
#include "weather.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

namespace {
BuddySaveData buddy;
BuddySaveData pendingImport;
bool available = false, dirty = false, importPending = false;
unsigned long lastSaveAttempt = 0, importStarted = 0;
bool retryPending = false;
constexpr unsigned long SAVE_RETRY_MS = 60000;
constexpr int64_t OBSERVATION_GUARD_SECONDS = 60;
constexpr unsigned long IMPORT_CONFIRM_MS = 60000;
char serialLine[BUDDY_IMPORT_BYTES + 14];
size_t serialLength = 0;
bool serialOverflow = false;
uint32_t newFieldEvents = 0;
uint32_t newAchievements = 0;
uint32_t newGearVariants[GEAR_ITEM_COUNT] = {};
void queueVariantUnlocks(const BuddySaveData &next) {
  if (addedGearVariant(buddy, next)) queueSoundEvent(SoundEvent::GEAR_VARIANT);
  for (uint8_t i = 0; i < GEAR_ITEM_COUNT; ++i) {
    uint32_t added = next.unlockedVariants[i] & ~buddy.unlockedVariants[i];
    newGearVariants[i] |= added;
    for (uint8_t variant = 0; variant < gearVariantCount(GearId(i + 1)); ++variant)
      if (added & (1UL << variant)) Serial.printf("New gear color: %s - %s\n", gearName(GearId(i + 1)), gearVariantName(GearId(i + 1), variant));
  }
}
bool buttonImportRequired = false;
BuddyTransferStatus transferStatus = BuddyTransferStatus::NONE;

uint32_t localDate(int64_t timestamp) {
  time_t epoch = static_cast<time_t>(timestamp);
  struct tm local = {};
  if (timestamp <= 0 || !localtime_r(&epoch, &local) || local.tm_year < 70 || local.tm_year > 8099) return 0;
  return uint32_t(local.tm_year + 1900) * 10000 + (local.tm_mon + 1) * 100 + local.tm_mday;
}
void processSerialLine() {
  if (strcmp(serialLine, "EXPORT_BUDDY") == 0) {
    if (buttonImportRequired) { Serial.println("Cancel import before exporting."); return; }
    transferStatus = exportBuddy(Serial) ? BuddyTransferStatus::EXPORT_COMPLETE : BuddyTransferStatus::EXPORT_FAILED;
  } else if (strncmp(serialLine, "IMPORT_BUDDY ", 13) == 0) {
    if (!buttonImportRequired) { Serial.println("Open SETTINGS > IMPORT BUDDY, then resend import data."); return; }
    transferStatus = importBuddy(serialLine + 13, serialLength - 13) ? BuddyTransferStatus::IMPORT_READY : BuddyTransferStatus::IMPORT_FAILED;
  } else if (strncmp(serialLine, "CONFIRM_IMPORT ", 15) == 0) {
    Serial.println("Serial confirmation disabled: press B on IMPORT BUDDY to replace; C cancels.");
  } else if (strcmp(serialLine, "CANCEL_IMPORT") == 0) {
    cancelBuddyImport(); transferStatus = BuddyTransferStatus::IMPORT_CANCELED; Serial.println("Import canceled.");
  } else if (strcmp(serialLine, "LIST_LOCATIONS") == 0) {
    listFieldLocations(Serial);
  } else if (strncmp(serialLine, "UPSERT_LOCATION ", 16) == 0) {
    unsigned id; double latitude, longitude; int consumed = 0;
    bool valid = sscanf(serialLine + 16, "%u %lf %lf %n", &id, &latitude, &longitude, &consumed) == 3 && consumed > 0 && id < FIELD_LOCATION_COUNT;
    if (!valid || !upsertFieldLocation(uint8_t(id), serialLine + 16 + consumed, latitude, longitude)) Serial.println("LOCATION_ERROR invalid location or save failed");
    else Serial.println("LOCATION_OK saved");
  } else if (strncmp(serialLine, "ACTIVE_LOCATION ", 16) == 0) {
    unsigned id; char extra;
    if (sscanf(serialLine + 16, "%u %c", &id, &extra) != 1 || id >= FIELD_LOCATION_COUNT || !activateFieldLocation(uint8_t(id))) Serial.println("LOCATION_ERROR invalid slot or save failed");
    else Serial.println("LOCATION_OK active");
  } else if (strncmp(serialLine, "DELETE_LOCATION ", 16) == 0) {
    unsigned id, replacement = UNKNOWN_LOCATION; char extra;
    int fields = sscanf(serialLine + 16, "%u %u %c", &id, &replacement, &extra);
    if ((fields != 1 && fields != 2) || id >= FIELD_LOCATION_COUNT || (fields == 2 && replacement >= FIELD_LOCATION_COUNT) || !deleteFieldLocation(uint8_t(id), uint8_t(replacement))) Serial.println("LOCATION_ERROR invalid slot, active location needs replacement, or save failed");
    else Serial.println("LOCATION_OK deleted");
  } else if (strncmp(serialLine, "SET_LOCATION ", 13) == 0) {
    const char *start = serialLine + 13; char *end = nullptr;
    double latitude = strtod(start, &end);
    bool valid = end != start && *end == ' ';
    start = end; double longitude = strtod(start, &end);
    bool longitudeParsed = end != start;
    while (*end == ' ') ++end;
    valid = valid && longitudeParsed && *end == 0;
    if (!valid || !saveBuddyLocation(latitude, longitude)) Serial.println("SET_LOCATION failed: use latitude [-90,90] longitude [-180,180]; save must be writable.");
    else Serial.println("Location saved; next live sync uses the new coordinates.");
  } else if (serialLength != 0) {
    Serial.println("Buddy commands: EXPORT_BUDDY, IMPORT_BUDDY <one-line JSON>, CANCEL_IMPORT, SET_LOCATION <latitude> <longitude>");
  }
}
} // namespace

WeatherCategory observationCategoryForCode(int32_t code) {
  // Reuse the working Open-Meteo mapping, independent of stable save IDs.
  switch (mapWeatherCode(code)) {
    case WEATHER_CLEAR: return WeatherCategory::CLEAR;
    case WEATHER_MAINLY_CLEAR: return WeatherCategory::MAINLY_CLEAR;
    case WEATHER_PARTLY_CLOUDY: return WeatherCategory::PARTLY_CLOUDY;
    case WEATHER_CLOUDY: return WeatherCategory::CLOUDY;
    case WEATHER_RAIN: return WeatherCategory::RAIN;
    case WEATHER_STORM: return WeatherCategory::STORM;
    case WEATHER_SNOW: return WeatherCategory::SNOW;
    case WEATHER_FOG: return WeatherCategory::FOG;
    default: return WeatherCategory::UNKNOWN;
  }
}

void initializeJournal() {
  SaveLoadResult result = loadBuddySave(buddy);
  available = buddySaveWritable();
  dirty = available && result != SaveLoadResult::LOADED;
  if (available && buddy.activeLocation == UNKNOWN_LOCATION) {
    initializeDefaultFieldLocation(buddy); dirty = true;
  }
  retryPending = false; importPending = false;
  buttonImportRequired = false; transferStatus = BuddyTransferStatus::NONE;
  serialLength = 0; serialOverflow = false; newFieldEvents = 0; newAchievements = 0; memset(newGearVariants, 0, sizeof(newGearVariants));
  const char *source = result == SaveLoadResult::LOADED ? "NVS" :
                       result == SaveLoadResult::MIGRATED_SAVE ? "older NVS (settings migration queued)" :
                       result == SaveLoadResult::MIGRATED_PET ? "legacy pet birthday (migration queued)" :
                       result == SaveLoadResult::NEW_BUDDY ? "newly initialized" : "protected/unavailable NVS";
  Serial.printf("Field Journal loaded from %s: observations %llu, days %lu, research began %lld\n",
                source, static_cast<unsigned long long>(buddy.totalObservations),
                static_cast<unsigned long>(buddy.uniqueDaysObserved), static_cast<long long>(buddy.createdAt));
}

const BuddySaveData &getBuddySave() { return buddy; }
bool journalAvailable() { return available; }
bool buddyNeedsSetup() { return available && !buddy.setupComplete; }
bool confirmBuddySetup(FurPaletteId palette) {
  if (!buddyNeedsSetup() || static_cast<uint8_t>(palette) >= FUR_PALETTE_COUNT) return false;
  BuddySaveData next = buddy;
  next.furPalette = palette; next.setupComplete = true;
  // Commit and verify before completing setup or replacing pending runtime data.
  if (!persistBuddySave(next)) return false;
  buddy = next; dirty = false; retryPending = false;
  Serial.printf("Buddy setup saved: fur palette %u. Ready for field work!\n", static_cast<uint8_t>(palette));
  return true;
}

void checkpointJournal(bool beforeSleep) {
  if (!available) return;
  // Capture the first valid clock once. Never change an existing start date.
  if (buddy.createdAt == 0 && timeValid) {
    int64_t now = static_cast<int64_t>(time(nullptr));
    if (localDate(now) != 0) { buddy.createdAt = now; dirty = true; }
  }
  if (!dirty || (!beforeSleep && retryPending && millis() - lastSaveAttempt < SAVE_RETRY_MS)) return;
  lastSaveAttempt = millis();
  if (persistBuddySave(buddy)) {
    dirty = false; retryPending = false;
    Serial.printf("Field Journal NVS saved: observations %llu, days %lu\n",
                  static_cast<unsigned long long>(buddy.totalObservations),
                  static_cast<unsigned long>(buddy.uniqueDaysObserved));
  } else {
    retryPending = true;
    Serial.println("ERROR: Field Journal save failed; previous NVS checkpoint preserved. Pending RAM progress can be lost if power is removed.");
  }
}

bool recordWeatherObservation(const WeatherObservation &observation) {
  if (!available) { Serial.println("Observation not recorded: buddy storage is protected/unavailable."); return false; }
  uint8_t category = static_cast<uint8_t>(observation.category);
  uint32_t date = localDate(observation.timestamp);
  if (category >= WEATHER_CATEGORY_COUNT || observation.category != observationCategoryForCode(observation.weatherCode) ||
      date == 0 || observation.temperatureMilliC < -130000 || observation.temperatureMilliC > 100000 ||
      observation.timestamp < buddy.createdAt) {
    Serial.println("Observation not recorded: invalid time, temperature or category."); return false;
  }
  if (buddy.latestObservationAt > 0 &&
      (observation.timestamp <= buddy.latestObservationAt ||
       observation.timestamp - buddy.latestObservationAt < OBSERVATION_GUARD_SECONDS)) {
    Serial.println("Observation not recorded: recent/backward timestamp (60-second guard)."); return false;
  }
  if (buddy.totalObservations == UINT64_MAX ||
      (date != buddy.lastObservedDate && buddy.uniqueDaysObserved == UINT32_MAX)) {
    Serial.println("Observation counter limit reached; progress preserved."); return false;
  }
  BuddySaveData next = buddy;
  if (next.createdAt == 0) next.createdAt = observation.timestamp;
  bool first = next.totalObservations == 0;
  ++next.totalObservations;
  if (date != next.lastObservedDate) ++next.uniqueDaysObserved;
  next.lastObservedDate = date;
  next.latestObservationAt = observation.timestamp;
  next.latestLocation = locationExists(next, observation.location) ? observation.location : next.activeLocation;
  if (locationExists(next, next.latestLocation) && !next.locations[next.latestLocation].visited) {
    next.locations[next.latestLocation].visited = true;
    if (next.fieldSitesVisited < UINT64_MAX) ++next.fieldSitesVisited;
  }
  next.latestTemperatureMilliC = observation.temperatureMilliC;
  next.latestWeatherCode = observation.weatherCode;
  next.latestCategory = observation.category;
  ++next.weatherCounts[category];
  next.discoveredWeather |= 1UL << category;
  if (first || observation.temperatureMilliC > next.highestTemperatureMilliC) {
    next.highestTemperatureMilliC = observation.temperatureMilliC;
    next.highestTemperatureAt = observation.timestamp; next.highestTemperatureLocation = next.latestLocation;
  }
  if (first || observation.temperatureMilliC < next.lowestTemperatureMilliC) {
    next.lowestTemperatureMilliC = observation.temperatureMilliC;
    next.lowestTemperatureAt = observation.timestamp; next.lowestTemperatureLocation = next.latestLocation;
  }
  next.latestMetrics = WeatherMetrics{};
  for (uint8_t i = 0; i < METRIC_COUNT; ++i) {
    MetricId metric = static_cast<MetricId>(i);
    if (observation.metrics.has(metric) && validMetric(metric, observation.metrics.values[i])) {
      next.latestMetrics.validMask |= 1UL << i; next.latestMetrics.values[i] = observation.metrics.values[i];
    }
  }
  for (uint8_t i = 0; i < RECORD_COUNT; ++i) {
    MetricId metric = recordMetric(static_cast<RecordId>(i));
    if (!next.latestMetrics.has(metric)) continue;
    int32_t value = next.latestMetrics.values[uint8_t(metric)];
    auto &record = next.records[i];
    if (record.timestamp == 0 || (i == uint8_t(RecordId::LOW_PRESSURE) ? value < record.value : value > record.value))
      record = {value, observation.timestamp, next.latestLocation};
  }
  if (observation.temperatureMilliC < 0 && next.freezingObservations < UINT64_MAX) ++next.freezingObservations;
  evaluateGearUnlocks(next);
  evaluateGearVariants(next);
  uint32_t achieved = evaluateAchievements(next, observation.timestamp);
  FieldNote note;
  note.timestamp = next.latestObservationAt; note.temperatureMilliC = next.latestTemperatureMilliC;
  note.weatherCode = next.latestWeatherCode; note.category = next.latestCategory;
  note.location = next.latestLocation; note.metrics = next.latestMetrics;
  note.outcomes = observationOutcomes(buddy, next);
  appendFieldNote(next, note);
  if (!validateBuddySave(next)) { Serial.println("Observation rejected by save validation."); return false; }
  uint32_t unlocked = next.unlockedGear & ~buddy.unlockedGear;
  queueVariantUnlocks(next);
  newAchievements |= achieved;
  if (note.outcomes & NEW_WEATHER_DISCOVERY) queueSoundEvent(SoundEvent::WEATHER_DISCOVERY);
  if (note.outcomes & (NEW_HIGH_TEMP|NEW_LOW_TEMP|NEW_WIND_RECORD|NEW_GUST_RECORD|NEW_PRESSURE_RECORD|NEW_PRECIP_RECORD|NEW_HUMIDITY_RECORD))
    queueSoundEvent(SoundEvent::LIFETIME_RECORD);
  if (unlocked) queueSoundEvent(SoundEvent::GEAR_ITEM);
  buddy = next; dirty = true;
  for (uint8_t id = 1; id <= 7; ++id) {
    GearId gear = static_cast<GearId>(id);
    if (unlocked & gearFlag(gear)) Serial.printf("Field gear unlocked: %s\n", gearName(gear));
  }
  Serial.printf("Weather observation #%llu: %s, %.3f C, Central date %lu\n",
                static_cast<unsigned long long>(buddy.totalObservations), weatherCategoryName(observation.category),
                observation.temperatureMilliC / 1000.0, static_cast<unsigned long>(date));
  checkpointJournal();
  return true;
}

bool equipJournalGear(GearSlot slot, GearId gear) { return equipJournalGearVariant(slot, gear, 0); }
bool equipJournalGearVariant(GearSlot slot, GearId gear, uint8_t variant) {
  if (!available || !gearFitsSlot(gear, slot) ||
      (gear == GearId::NONE ? variant != 0 : !gearVariantUnlocked(buddy, gear, variant))) return false;
  uint8_t index = uint8_t(slot);
  if (buddy.equippedSlots[index] == gear && buddy.equippedVariants[index] == variant) return true;
  BuddySaveData next = buddy; next.equippedSlots[index] = gear; next.equippedVariants[index] = variant;
  if (!persistBuddySave(next)) return false;
  buddy = next; dirty = false; retryPending = false; return true;
}

bool exportBuddy(Print &output) {
  if (!available) { output.println("ERROR: Buddy export unavailable; original NVS data preserved."); return false; }
  bool success = serializeBuddySave(buddy, output);
  output.println();
  return success;
}

bool importBuddy(const char *json, size_t length) {
  cancelBuddyImport(); transferStatus = BuddyTransferStatus::IMPORT_FAILED;
  if (!available) { Serial.println("Import disabled: buddy storage is protected/unavailable."); return false; }
  const char *error = nullptr;
  if (!deserializeBuddySave(json, length, pendingImport, error)) {
    Serial.printf("Import rejected: %s. Existing buddy preserved.\n", error); return false;
  }
  importPending = true; importStarted = millis(); transferStatus = BuddyTransferStatus::IMPORT_READY;
  Serial.println("Import validated; press B on IMPORT BUDDY within 60 seconds to REPLACE, or C to cancel.");
  return true;
}

bool confirmBuddyImport(uint32_t checksum) {
  if (!available || !importPending || millis() - importStarted >= IMPORT_CONFIRM_MS ||
      checksum != buddySaveChecksum(pendingImport)) return false;
  if (!persistBuddySave(pendingImport)) {
    Serial.println("Import NVS commit failed; current runtime buddy preserved.");
    cancelBuddyImport(); transferStatus = BuddyTransferStatus::IMPORT_FAILED; return false;
  }
  bool activeCoordinatesChanged = locationExists(buddy, buddy.activeLocation) && locationExists(pendingImport, pendingImport.activeLocation) &&
      (buddy.locations[buddy.activeLocation].latitudeMicrodegrees != pendingImport.locations[pendingImport.activeLocation].latitudeMicrodegrees ||
       buddy.locations[buddy.activeLocation].longitudeMicrodegrees != pendingImport.locations[pendingImport.activeLocation].longitudeMicrodegrees);
  bool locationChanged = activeCoordinatesChanged || buddy.activeLocation != pendingImport.activeLocation || buddy.locationConfigured != pendingImport.locationConfigured ||
      buddy.latitudeMicrodegrees != pendingImport.latitudeMicrodegrees || buddy.longitudeMicrodegrees != pendingImport.longitudeMicrodegrees;
  buddy = pendingImport; newFieldEvents = 0; newAchievements = 0; memset(newGearVariants, 0, sizeof(newGearVariants)); dirty = false; retryPending = false;
  if (locationChanged) invalidateWeatherLocation();
  transferStatus = BuddyTransferStatus::IMPORT_COMPLETE;
  cancelBuddyImport();
  Serial.println("Buddy import committed and verified.");
  return true;
}

void cancelBuddyImport() { importPending = false; pendingImport = BuddySaveData{}; }

void updateBuddySerial() {
  if (importPending && millis() - importStarted >= IMPORT_CONFIRM_MS) {
    cancelBuddyImport(); transferStatus = BuddyTransferStatus::IMPORT_FAILED; Serial.println("Import confirmation expired; current buddy preserved.");
  }
  // Fixed storage, bounded work per loop, no readString/readBytes waits.
  unsigned processed = 0;
  while (processed++ < 64 && Serial.available() > 0) {
    int next = Serial.read();
    if (next < 0) break;
    char c = static_cast<char>(next);
    // Arduino Serial Monitor supports LF, CR, and CRLF. An empty second
    // terminator is harmless and must never execute a command twice.
    if (c == '\n' || c == '\r') {
      serialLine[serialLength] = 0;
      if (serialOverflow) { cancelBuddyImport(); transferStatus = BuddyTransferStatus::IMPORT_FAILED; Serial.println("Buddy command too long; discarded without writing."); }
      else processSerialLine();
      serialLength = 0; serialOverflow = false;
    } else if (c == '\0') {
      serialOverflow = true;
    } else if (serialLength < sizeof(serialLine) - 1) {
      serialLine[serialLength++] = c;
    } else serialOverflow = true;
  }
}

void updateJournal() { checkpointJournal(); updateBuddySerial(); }

namespace {
bool commitSettings(const BuddySaveData &next) {
  if (!available) return false;
  if (buddySaveChecksum(next) == buddySaveChecksum(buddy)) return true;
  if (!persistBuddySave(next)) return false;
  buddy = next; dirty = false; retryPending = false; return true;
}
}
bool saveBuddySound(bool enabled) {
  BuddySaveData next = buddy; next.soundEnabled = enabled; return commitSettings(next);
}
bool saveBuddyUnits(UnitsId units) {
  if (static_cast<uint8_t>(units) > 1) return false;
  BuddySaveData next = buddy; next.units = units; return commitSettings(next);
}
bool saveBuddyLocation(double latitude, double longitude) {
  // Backward-compatible USB command edits the active named site.
  uint8_t id = buddy.activeLocation;
  return locationExists(buddy, id) && upsertFieldLocation(id, buddy.locations[id].name, latitude, longitude);
}
namespace {
void mirrorActiveLocation(BuddySaveData &s) {
  const auto &site = s.locations[s.activeLocation]; s.locationConfigured = true;
  s.latitudeMicrodegrees = site.latitudeMicrodegrees; s.longitudeMicrodegrees = site.longitudeMicrodegrees;
}
}
bool upsertFieldLocation(uint8_t id, const char *name, double latitude, double longitude) {
  if (id >= FIELD_LOCATION_COUNT || !validLocationName(name) || !isfinite(latitude) || !isfinite(longitude) ||
      latitude < -90 || latitude > 90 || longitude < -180 || longitude > 180 || importPending) return false;
  BuddySaveData next = buddy; auto &site = next.locations[id];
  memset(site.name, 0, sizeof(site.name)); strcpy(site.name, name); site.used = true;
  site.latitudeMicrodegrees = lround(latitude * 1000000); site.longitudeMicrodegrees = lround(longitude * 1000000);
  if (next.activeLocation == UNKNOWN_LOCATION) next.activeLocation = id;
  bool coordinatesChanged = next.activeLocation == id &&
      (!locationExists(buddy, id) || buddy.locations[id].latitudeMicrodegrees != site.latitudeMicrodegrees || buddy.locations[id].longitudeMicrodegrees != site.longitudeMicrodegrees);
  mirrorActiveLocation(next);
  if (!commitSettings(next)) return false;
  if (coordinatesChanged) invalidateWeatherLocation();
  return true;
}
bool activateFieldLocation(uint8_t id) {
  if (!locationExists(buddy, id) || importPending) return false;
  if (buddy.activeLocation == id) return true;
  BuddySaveData next = buddy; next.activeLocation = id; mirrorActiveLocation(next);
  if (!commitSettings(next)) return false;
  invalidateWeatherLocation(); return true;
}
bool deleteFieldLocation(uint8_t id, uint8_t replacement) {
  if (!locationExists(buddy, id) || importPending ||
      (replacement != UNKNOWN_LOCATION && (!locationExists(buddy, replacement) || replacement == id)) ||
      (buddy.activeLocation == id && replacement == UNKNOWN_LOCATION)) return false;
  BuddySaveData next = buddy;
  if (next.activeLocation == id) { next.activeLocation = replacement; mirrorActiveLocation(next); }
  next.locations[id] = FieldLocation{};
  // Reusing a slot must never attribute an old record to a newly configured site.
  for (auto &note : next.fieldNotes) if (note.location == id) note.location = UNKNOWN_LOCATION;
  for (auto &record : next.records) if (record.location == id) record.location = UNKNOWN_LOCATION;
  if (next.latestLocation == id) next.latestLocation = UNKNOWN_LOCATION;
  if (next.highestTemperatureLocation == id) next.highestTemperatureLocation = UNKNOWN_LOCATION;
  if (next.lowestTemperatureLocation == id) next.lowestTemperatureLocation = UNKNOWN_LOCATION;
  bool switched = next.activeLocation != buddy.activeLocation;
  if (!commitSettings(next)) return false;
  if (switched) invalidateWeatherLocation(); return true;
}
void listFieldLocations(Print &output) {
  if (!available) { output.println("LOCATION_ERROR protected/unavailable save"); return; }
  JsonDocument doc; doc["activeLocation"] = buddy.activeLocation; doc["fieldSitesVisited"] = buddy.fieldSitesVisited;
  JsonArray sites = doc["locations"].to<JsonArray>();
  for (uint8_t i = 0; i < FIELD_LOCATION_COUNT; ++i) if (buddy.locations[i].used) {
    const auto &site = buddy.locations[i]; JsonObject item = sites.add<JsonObject>();
    item["id"] = i; item["name"] = site.name; item["latitude"] = site.latitudeMicrodegrees / 1000000.0;
    item["longitude"] = site.longitudeMicrodegrees / 1000000.0; item["visited"] = site.visited;
  }
  output.write(reinterpret_cast<const uint8_t *>("LOCATIONS "), 10); serializeJson(doc, output); output.println();
}
BuddyTransferStatus buddyTransferStatus() { return transferStatus; }
uint32_t pendingBuddyImportChecksum() { return importPending ? buddySaveChecksum(pendingImport) : 0; }
void beginBuddyTransfer(bool importing) {
  cancelBuddyImport(); buttonImportRequired = importing;
  transferStatus = BuddyTransferStatus::NONE;
}
void endBuddyTransfer() {
  cancelBuddyImport(); buttonImportRequired = false; transferStatus = BuddyTransferStatus::NONE;
}

bool latestJournalMetric(MetricId id, int32_t &value) {
  if (!available || !buddy.latestMetrics.has(id)) return false;
  value = buddy.latestMetrics.values[uint8_t(id)]; return true;
}
bool journalRecord(RecordId id, WeatherRecord &record) {
  if (!available || uint8_t(id) >= RECORD_COUNT || buddy.records[uint8_t(id)].timestamp == 0) return false;
  record = buddy.records[uint8_t(id)]; return true;
}

bool latestJournalTemperature(int32_t &milliC) {
  if (!available || buddy.totalObservations == 0) return false;
  milliC = buddy.latestTemperatureMilliC; return true;
}

bool commitFieldEvents(const BuddySaveData &next, uint32_t discoveries) {
  if (!available) return false;
  BuddySaveData progress = next; evaluateGearVariants(progress);
  uint32_t achieved = evaluateAchievements(progress, progress.latestObservationAt);
  // Alert collection follows the accepted live observation in the same session.
  if (progress.fieldNoteCount && fieldNoteAt(progress, 0)->timestamp == progress.latestObservationAt) {
    auto &note = progress.fieldNotes[(progress.fieldNoteNext + FIELD_NOTE_COUNT - 1) % FIELD_NOTE_COUNT];
    note.severeEvents |= discoveries;
    if (discoveries) note.outcomes |= NEW_SEVERE_EVENT;
    if (addedGearVariant(buddy, progress)) note.outcomes |= NEW_GEAR_VARIANT;
  }
  if (!validateBuddySave(progress)) return false;
  queueVariantUnlocks(progress);
  newAchievements |= achieved;
  if (discoveries) queueSoundEvent(SoundEvent::SEVERE_DISCOVERY);
  buddy = progress; dirty = true; newFieldEvents |= discoveries;
  checkpointJournal(); // Existing retry/before-sleep policy protects failed writes.
  return true;
}
FieldEventId takeNewFieldEvent() {
  for (uint8_t i = 0; i < FIELD_EVENT_COUNT; ++i) if (newFieldEvents & (1UL << i)) {
    newFieldEvents &= ~(1UL << i); return FieldEventId(i);
  }
  return FieldEventId::COUNT;
}
bool discoveredFieldEvent(FieldEventId id) {
  return available && uint8_t(id) < FIELD_EVENT_COUNT && buddy.fieldEvents[uint8_t(id)].count > 0;
}

bool takeNewGearVariant(GearId &gear, uint8_t &variant) {
 for (uint8_t i = 0; i < GEAR_ITEM_COUNT; ++i) for (uint8_t v = 0; v < gearVariantCount(GearId(i + 1)); ++v)
   if (newGearVariants[i] & (1UL << v)) { newGearVariants[i] &= ~(1UL << v); gear = GearId(i + 1); variant = v; return true; }
 return false;
}

AchievementId takeNewAchievement() {
  for (uint8_t i = 0; i < ACHIEVEMENT_COUNT; ++i) if (newAchievements & (1UL << i)) {
    newAchievements &= ~(1UL << i); return AchievementId(i);
  }
  return AchievementId::COUNT;
}
