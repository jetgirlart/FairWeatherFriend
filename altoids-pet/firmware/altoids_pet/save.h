#pragma once

#include <Arduino.h>
#include <stdint.h>
#include "palette.h"

constexpr uint32_t SAVE_VERSION = 7;
constexpr uint8_t WEATHER_CATEGORY_COUNT = 8;

// Stable save IDs, independent of renderer/weather enums. Append in a new version.
enum class WeatherCategory : uint8_t {
  CLEAR, MAINLY_CLEAR, PARTLY_CLOUDY, CLOUDY, RAIN, STORM, SNOW, FOG,
  UNKNOWN = 255
};
enum class GearId : uint8_t {
  NONE = 0, FIELD_CAP = 1, SUNGLASSES = 2, UMBRELLA = 3, RAINCOAT = 4,
  WINTER_SCARF = 5, WINTER_COAT = 6, BOOTS = 7
};

enum class GearSlot : uint8_t { HEAD, FACE, NECK, BODY, FEET, PROP, COUNT };
constexpr uint8_t GEAR_SLOT_COUNT = static_cast<uint8_t>(GearSlot::COUNT);

enum class UnitsId : uint8_t { US = 0, METRIC = 1 };

enum class MetricId : uint8_t { HUMIDITY, WIND, GUST, PRESSURE, PRECIPITATION, COUNT };
constexpr uint8_t METRIC_COUNT = 5;
enum class RecordId : uint8_t { WIND, GUST, HUMIDITY, LOW_PRESSURE, HIGH_PRESSURE, PRECIPITATION, COUNT };
constexpr uint8_t RECORD_COUNT = 6;
struct WeatherMetrics {
  uint32_t validMask = 0;
  int32_t values[METRIC_COUNT] = {}; // Hundredths: percent, km/h, km/h, hPa, mm.
  bool has(MetricId id) const { return uint8_t(id) < METRIC_COUNT && (validMask & (1UL << uint8_t(id))); }
};
constexpr uint8_t FIELD_LOCATION_COUNT = 8;
constexpr uint8_t LOCATION_NAME_BYTES = 16;
constexpr uint8_t UNKNOWN_LOCATION = 255;
struct FieldLocation {
  char name[LOCATION_NAME_BYTES] = {};
  int32_t latitudeMicrodegrees = 0, longitudeMicrodegrees = 0;
  bool used = false, visited = false;
};
struct WeatherRecord { int32_t value = 0; int64_t timestamp = 0; uint8_t location = UNKNOWN_LOCATION; }; // timestamp 0 = unset.
const char *metricName(MetricId id);
const char *recordName(RecordId id);
MetricId recordMetric(RecordId id);
bool validMetric(MetricId id, int32_t value);

// Stable collectible IDs; append only in a versioned migration.
enum class FieldEventId : uint8_t {
  TORNADO_WATCH, TORNADO_WARNING, SEVERE_THUNDERSTORM_WATCH,
  SEVERE_THUNDERSTORM_WARNING, FLASH_FLOOD_WARNING, FLOOD_WARNING,
  HURRICANE_WATCH, HURRICANE_WARNING, TROPICAL_STORM_WATCH,
  TROPICAL_STORM_WARNING, WINTER_STORM_WARNING, BLIZZARD_WARNING,
  ICE_STORM_WARNING, EXTREME_HEAT_WARNING, EXTREME_COLD_WARNING, COUNT
};
constexpr uint8_t FIELD_EVENT_COUNT = uint8_t(FieldEventId::COUNT);
constexpr uint8_t RECENT_ALERT_COUNT = 32;
constexpr size_t BUDDY_IMPORT_BYTES = 12288;
struct FieldEventRecord {
  uint32_t count = 0; // count > 0 is the canonical discovered flag.
  int64_t firstAt = 0, latestAt = 0;
};
struct RecentAlert { uint64_t hash = 0; int64_t seenAt = 0; };
const char *fieldEventName(FieldEventId id); // Stable export name / official NWS event.

struct BuddySaveData {
  uint32_t saveVersion = SAVE_VERSION;
  int64_t createdAt = 0; // FIELD RESEARCH BEGAN; 0 until the existing clock is valid.
  uint64_t totalObservations = 0;
  uint32_t uniqueDaysObserved = 0;
  int64_t latestObservationAt = 0;
  uint32_t lastObservedDate = 0; // Central local YYYYMMDD, not UTC day boundaries.
  int32_t latestTemperatureMilliC = 0; // Thousandths Celsius; metric canonical storage.
  int32_t latestWeatherCode = -1;
  WeatherCategory latestCategory = WeatherCategory::UNKNOWN;
  int32_t highestTemperatureMilliC = 0;
  int64_t highestTemperatureAt = 0;
  int32_t lowestTemperatureMilliC = 0;
  int64_t lowestTemperatureAt = 0;
  WeatherMetrics latestMetrics;
  WeatherRecord records[RECORD_COUNT] = {};
  uint64_t weatherCounts[WEATHER_CATEGORY_COUNT] = {};
  uint32_t discoveredWeather = 0; // Bit n corresponds to WeatherCategory n.
  uint32_t unlockedGear = 0;     // Bit (GearId - 1), NONE has no bit.
  GearId equippedSlots[GEAR_SLOT_COUNT] = {};
  FurPaletteId furPalette = FurPaletteId::ORANGE;
  bool soundEnabled = true;
  UnitsId units = UnitsId::US;
  bool locationConfigured = false;
  int32_t latitudeMicrodegrees = 0;
  int32_t longitudeMicrodegrees = 0;
  FieldEventRecord fieldEvents[FIELD_EVENT_COUNT] = {};
  RecentAlert recentAlerts[RECENT_ALERT_COUNT] = {};
  FieldLocation locations[FIELD_LOCATION_COUNT] = {};
  uint8_t activeLocation = UNKNOWN_LOCATION, latestLocation = UNKNOWN_LOCATION;
  uint8_t highestTemperatureLocation = UNKNOWN_LOCATION, lowestTemperatureLocation = UNKNOWN_LOCATION;
  uint64_t fieldSitesVisited = 0; // Lifetime configured sites visited, including deleted sites.
  bool setupComplete = false; // Only genuinely new buddies need first-run setup.
};

// Only permanent fields are serialized, explicitly as little-endian values.
// Never persist this native C++ struct/padding or animation/cache structures.
enum class SaveLoadResult { LOADED, MIGRATED_SAVE, NEW_BUDDY, MIGRATED_PET, PROTECTED, UNAVAILABLE };
SaveLoadResult loadBuddySave(BuddySaveData &data);
bool buddySaveWritable();
bool persistBuddySave(const BuddySaveData &data);
bool validateBuddySave(const BuddySaveData &data);
uint32_t buddySaveChecksum(const BuddySaveData &data);
const char *weatherCategoryName(WeatherCategory category);
bool serializeBuddySave(const BuddySaveData &data, Print &output);
bool deserializeBuddySave(const char *json, size_t length, BuddySaveData &data,
                          const char *&error);

void initializeDefaultFieldLocation(BuddySaveData &data);
