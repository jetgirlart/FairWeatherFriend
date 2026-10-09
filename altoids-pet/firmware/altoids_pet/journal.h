#pragma once
#include "save.h"

struct WeatherObservation {
  int64_t timestamp;
  int32_t temperatureMilliC;
  int32_t weatherCode;
  WeatherCategory category;
  WeatherMetrics metrics;
  uint8_t location = UNKNOWN_LOCATION;
};

WeatherCategory observationCategoryForCode(int32_t code);
void initializeJournal();
void updateJournal();
void checkpointJournal(bool beforeSleep = false);
const BuddySaveData &getBuddySave();
bool journalAvailable();
bool recordWeatherObservation(const WeatherObservation &observation);
bool equipJournalGear(GearSlot slot, GearId gear);
bool buddyNeedsSetup();
bool confirmBuddySetup(FurPaletteId palette);

// USB Serial foundation; staging never writes. Confirmation commits to NVS
// before replacing the runtime buddy. Import is the explicit replacement path.
bool exportBuddy(Print &output);
bool importBuddy(const char *json, size_t length);
bool confirmBuddyImport(uint32_t checksum);
void cancelBuddyImport();
void updateBuddySerial();

// Settings commit immediately, only on changed values; failed writes keep runtime intact.
bool saveBuddySound(bool enabled);
bool saveBuddyUnits(UnitsId units);
bool saveBuddyLocation(double latitude, double longitude);
enum class BuddyTransferStatus : uint8_t {
  NONE, EXPORT_COMPLETE, EXPORT_FAILED, IMPORT_READY, IMPORT_FAILED, IMPORT_COMPLETE, IMPORT_CANCELED
};
BuddyTransferStatus buddyTransferStatus();
uint32_t pendingBuddyImportChecksum();
void beginBuddyTransfer(bool importing);
void endBuddyTransfer();

// Canonical metric queries for future gear conditions; false means unavailable.
bool latestJournalMetric(MetricId id, int32_t &value);
bool journalRecord(RecordId id, WeatherRecord &record);

bool latestJournalTemperature(int32_t &milliC);

// Supplemental alert batch commits once; these queries are future gear reward hooks.
bool commitFieldEvents(const BuddySaveData &next, uint32_t discoveries);
FieldEventId takeNewFieldEvent();
bool discoveredFieldEvent(FieldEventId id);

bool upsertFieldLocation(uint8_t id, const char *name, double latitude, double longitude);
bool activateFieldLocation(uint8_t id);
bool deleteFieldLocation(uint8_t id, uint8_t replacement = UNKNOWN_LOCATION);
void listFieldLocations(Print &output);

bool equipJournalGearVariant(GearSlot slot, GearId gear, uint8_t variant);
bool takeNewGearVariant(GearId &gear, uint8_t &variant);
