#include "journal.h"
#include "gear.h"
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
char serialLine[4097];
size_t serialLength = 0;
bool serialOverflow = false;
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
  retryPending = false; importPending = false;
  buttonImportRequired = false; transferStatus = BuddyTransferStatus::NONE;
  serialLength = 0; serialOverflow = false;
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
      date == 0 || observation.temperatureDeciF < -2000 || observation.temperatureDeciF > 2000 ||
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
  next.latestTemperatureDeciF = observation.temperatureDeciF;
  next.latestWeatherCode = observation.weatherCode;
  next.latestCategory = observation.category;
  ++next.weatherCounts[category];
  next.discoveredWeather |= 1UL << category;
  if (first || observation.temperatureDeciF > next.highestTemperatureDeciF) {
    next.highestTemperatureDeciF = observation.temperatureDeciF;
    next.highestTemperatureAt = observation.timestamp;
  }
  if (first || observation.temperatureDeciF < next.lowestTemperatureDeciF) {
    next.lowestTemperatureDeciF = observation.temperatureDeciF;
    next.lowestTemperatureAt = observation.timestamp;
  }
  evaluateGearUnlocks(next);
  if (!validateBuddySave(next)) { Serial.println("Observation rejected by save validation."); return false; }
  uint32_t unlocked = next.unlockedGear & ~buddy.unlockedGear;
  buddy = next; dirty = true;
  for (uint8_t id = 1; id <= 7; ++id) {
    GearId gear = static_cast<GearId>(id);
    if (unlocked & gearFlag(gear)) Serial.printf("Field gear unlocked: %s\n", gearName(gear));
  }
  Serial.printf("Weather observation #%llu: %s, %.1f F, Central date %lu\n",
                static_cast<unsigned long long>(buddy.totalObservations), weatherCategoryName(observation.category),
                observation.temperatureDeciF / 10.0, static_cast<unsigned long>(date));
  checkpointJournal();
  return true;
}

bool equipJournalGear(GearSlot slot, GearId gear) {
  if (!available || !gearFitsSlot(gear, slot) ||
      (gear != GearId::NONE && !(buddy.unlockedGear & gearFlag(gear)))) return false;
  if (gear == buddy.equippedSlots[static_cast<uint8_t>(slot)]) return true;
  // Commit first so a failed selection does not replace the equipped item.
  BuddySaveData next = buddy; next.equippedSlots[static_cast<uint8_t>(slot)] = gear;
  if (!persistBuddySave(next)) return false;
  buddy = next; dirty = false; retryPending = false;
  return true;
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
  bool locationChanged = buddy.locationConfigured != pendingImport.locationConfigured ||
      buddy.latitudeMicrodegrees != pendingImport.latitudeMicrodegrees || buddy.longitudeMicrodegrees != pendingImport.longitudeMicrodegrees;
  buddy = pendingImport; dirty = false; retryPending = false;
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
  if (!isfinite(latitude) || !isfinite(longitude) || latitude < -90 || latitude > 90 || longitude < -180 || longitude > 180) return false;
  BuddySaveData next = buddy; next.locationConfigured = true;
  next.latitudeMicrodegrees = lround(latitude * 1000000); next.longitudeMicrodegrees = lround(longitude * 1000000);
  bool changed = buddySaveChecksum(next) != buddySaveChecksum(buddy);
  if (!commitSettings(next)) return false;
  if (changed) invalidateWeatherLocation();
  return true;
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
