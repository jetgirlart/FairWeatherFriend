#pragma once

#include <Arduino.h>
#include <stdint.h>
#include "palette.h"

constexpr uint32_t SAVE_VERSION = 5;
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
struct WeatherRecord { int32_t value = 0; int64_t timestamp = 0; }; // timestamp 0 = unset.
const char *metricName(MetricId id);
const char *recordName(RecordId id);
MetricId recordMetric(RecordId id);
bool validMetric(MetricId id, int32_t value);

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
