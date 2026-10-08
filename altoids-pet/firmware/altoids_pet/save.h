#pragma once

#include <Arduino.h>
#include <stdint.h>

constexpr uint32_t SAVE_VERSION = 1;
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

struct BuddySaveData {
  uint32_t saveVersion = SAVE_VERSION;
  int64_t createdAt = 0; // FIELD RESEARCH BEGAN; 0 until the existing clock is valid.
  uint64_t totalObservations = 0;
  uint32_t uniqueDaysObserved = 0;
  int64_t latestObservationAt = 0;
  uint32_t lastObservedDate = 0; // Central local YYYYMMDD, not UTC day boundaries.
  int32_t latestTemperatureDeciF = 0; // Tenths Fahrenheit, independent of UI units.
  int32_t latestWeatherCode = -1;
  WeatherCategory latestCategory = WeatherCategory::UNKNOWN;
  int32_t highestTemperatureDeciF = 0;
  int64_t highestTemperatureAt = 0;
  int32_t lowestTemperatureDeciF = 0;
  int64_t lowestTemperatureAt = 0;
  uint64_t weatherCounts[WEATHER_CATEGORY_COUNT] = {};
  uint32_t discoveredWeather = 0; // Bit n corresponds to WeatherCategory n.
  uint32_t unlockedGear = 0;     // Bit (GearId - 1), NONE has no bit.
  GearId equippedGear = GearId::NONE;
};

// Only permanent fields are serialized, explicitly as little-endian values.
// Never persist this native C++ struct/padding or animation/cache structures.
enum class SaveLoadResult { LOADED, NEW_BUDDY, MIGRATED_PET, PROTECTED, UNAVAILABLE };
SaveLoadResult loadBuddySave(BuddySaveData &data);
bool buddySaveWritable();
bool persistBuddySave(const BuddySaveData &data);
bool validateBuddySave(const BuddySaveData &data);
uint32_t buddySaveChecksum(const BuddySaveData &data);
const char *weatherCategoryName(WeatherCategory category);
bool serializeBuddySave(const BuddySaveData &data, Print &output);
bool deserializeBuddySave(const char *json, size_t length, BuddySaveData &data,
                          const char *&error);
