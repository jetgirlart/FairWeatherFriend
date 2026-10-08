#pragma once
#include "save.h"

struct WeatherObservation {
  int64_t timestamp;
  int32_t temperatureDeciF;
  int32_t weatherCode;
  WeatherCategory category;
};

WeatherCategory observationCategoryForCode(int32_t code);
void initializeJournal();
void updateJournal();
void checkpointJournal(bool beforeSleep = false);
const BuddySaveData &getBuddySave();
bool journalAvailable();
bool recordWeatherObservation(const WeatherObservation &observation);
bool equipJournalGear(GearId gear);

// USB Serial foundation; staging never writes. Confirmation commits to NVS
// before replacing the runtime buddy. Import is the explicit replacement path.
bool exportBuddy(Print &output);
bool importBuddy(const char *json, size_t length);
bool confirmBuddyImport(uint32_t checksum);
void cancelBuddyImport();
void updateBuddySerial();
