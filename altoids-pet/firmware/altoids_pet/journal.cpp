#include "journal.h"
#include "gear.h"
#include "weather.h"
#include <string.h>
#include <stdlib.h>

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

uint32_t localDate(int64_t timestamp) {
  time_t epoch = static_cast<time_t>(timestamp);
  struct tm local = {};
  if (timestamp <= 0 || !localtime_r(&epoch, &local) || local.tm_year < 70 || local.tm_year > 8099) return 0;
  return uint32_t(local.tm_year + 1900) * 10000 + (local.tm_mon + 1) * 100 + local.tm_mday;
}
void processSerialLine() {
  if (strcmp(serialLine, "EXPORT_BUDDY") == 0) {
    exportBuddy(Serial);
  } else if (strncmp(serialLine, "IMPORT_BUDDY ", 13) == 0) {
    importBuddy(serialLine + 13, serialLength - 13);
  } else if (strncmp(serialLine, "CONFIRM_IMPORT ", 15) == 0) {
    const char *text = serialLine + 15;
    bool valid = strlen(text) == 8;
    for (size_t i = 0; valid && i < 8; ++i) {
      char c = text[i];
      valid = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }
    if (!valid || !confirmBuddyImport(strtoul(text, nullptr, 16))) {
      Serial.println("Import not confirmed; current buddy preserved.");
    }
  } else if (strcmp(serialLine, "CANCEL_IMPORT") == 0) {
    cancelBuddyImport(); Serial.println("Import canceled.");
  } else if (serialLength != 0) {
    Serial.println("Buddy commands: EXPORT_BUDDY, IMPORT_BUDDY <one-line JSON>, CONFIRM_IMPORT <checksum>, CANCEL_IMPORT");
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
  serialLength = 0; serialOverflow = false;
  const char *source = result == SaveLoadResult::LOADED ? "NVS" :
                       result == SaveLoadResult::MIGRATED_SAVE ? "older NVS (palette migration queued)" :
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
  checkpointJournal();
  if (dirty) { output.println("ERROR: Export deferred until pending progress is saved."); return false; }
  bool success = serializeBuddySave(buddy, output);
  output.println();
  return success;
}

bool importBuddy(const char *json, size_t length) {
  cancelBuddyImport();
  if (!available) { Serial.println("Import disabled: buddy storage is protected/unavailable."); return false; }
  const char *error = nullptr;
  if (!deserializeBuddySave(json, length, pendingImport, error)) {
    Serial.printf("Import rejected: %s. Existing buddy preserved.\n", error); return false;
  }
  importPending = true; importStarted = millis();
  Serial.printf("Import validated; will REPLACE the current buddy. Within 60 seconds send CONFIRM_IMPORT %08lx, or CANCEL_IMPORT.\n",
                static_cast<unsigned long>(buddySaveChecksum(pendingImport)));
  return true;
}

bool confirmBuddyImport(uint32_t checksum) {
  if (!available || !importPending || millis() - importStarted >= IMPORT_CONFIRM_MS ||
      checksum != buddySaveChecksum(pendingImport)) return false;
  if (!persistBuddySave(pendingImport)) {
    Serial.println("Import NVS commit failed; current runtime buddy preserved.");
    cancelBuddyImport(); return false;
  }
  buddy = pendingImport; dirty = false; retryPending = false;
  cancelBuddyImport();
  Serial.println("Buddy import committed and verified.");
  return true;
}

void cancelBuddyImport() { importPending = false; pendingImport = BuddySaveData{}; }

void updateBuddySerial() {
  if (importPending && millis() - importStarted >= IMPORT_CONFIRM_MS) {
    cancelBuddyImport(); Serial.println("Import confirmation expired; current buddy preserved.");
  }
  // Fixed storage, bounded work per loop, no readString/readBytes waits.
  unsigned processed = 0;
  while (processed++ < 64 && Serial.available() > 0) {
    int next = Serial.read();
    if (next < 0) break;
    char c = static_cast<char>(next);
    if (c == '\r') continue;
    if (c == '\n') {
      serialLine[serialLength] = 0;
      if (serialOverflow) { cancelBuddyImport(); Serial.println("Buddy command too long; discarded without writing."); }
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
