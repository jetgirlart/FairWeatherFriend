#include "achievements.h"
#ifdef ARDUINO
#define ACHIEVEMENT_FLASH PROGMEM
#else
#define ACHIEVEMENT_FLASH
#endif
namespace {
enum class Source : uint8_t { OBSERVATIONS, VARIETY, HIGH_TEMPERATURE, LOW_TEMPERATURE, SITES, SEVERE, EVENT };
struct AchievementDefinition {
  char name[24]; char description[80]; Source source; int32_t threshold;
};
// ESP32 flash is memory mapped; strings and rules share one immutable table.
const AchievementDefinition definitions[ACHIEVEMENT_COUNT] ACHIEVEMENT_FLASH = {
 {"FIRST NOTES", "Record your first live weather observation.", Source::OBSERVATIONS, 1},
 {"FIELD 10", "Record 10 live weather observations.", Source::OBSERVATIONS, 10},
 {"FIELD 50", "Record 50 live weather observations.", Source::OBSERVATIONS, 50},
 {"FIELD 100", "Record 100 live weather observations.", Source::OBSERVATIONS, 100},
 {"WEATHER 4", "Discover four ordinary weather categories.", Source::VARIETY, 4},
 {"WEATHER 6", "Discover six ordinary weather categories.", Source::VARIETY, 6},
 {"ALL CORE WEATHER", "Discover all eight ordinary weather categories.", Source::VARIETY, 8},
 {"CENTURY", "Observe 100 F or warmer (37.778 C).", Source::HIGH_TEMPERATURE, 37778},
 {"FREEZING", "Observe 32 F / 0 C or colder.", Source::LOW_TEMPERATURE, 0},
 {"DEEP FREEZE", "Observe 0 F or colder (-17.778 C).", Source::LOW_TEMPERATURE, -17778},
 {"THREE FIELD SITES", "Record live weather at three configured field sites.", Source::SITES, 3},
 {"FIVE FIELD SITES", "Record live weather at five configured field sites.", Source::SITES, 5},
 {"EIGHT FIELD SITES", "Record live weather at eight configured field sites.", Source::SITES, 8},
 {"FIRST SEVERE", "Witness your first supported severe-weather event.", Source::SEVERE, 1},
 {"TORNADO WITNESS", "Witness a Tornado Warning during a live check.", Source::EVENT, int32_t(FieldEventId::TORNADO_WARNING)},
 {"HURRICANE WITNESS", "Witness a Hurricane Warning during a live check.", Source::EVENT, int32_t(FieldEventId::HURRICANE_WARNING)},
 {"BLIZZARD WITNESS", "Witness a Blizzard Warning during a live check.", Source::EVENT, int32_t(FieldEventId::BLIZZARD_WARNING)},
 {"FLASH FLOOD WITNESS", "Witness a Flash Flood Warning during a live check.", Source::EVENT, int32_t(FieldEventId::FLASH_FLOOD_WARNING)}
};
}
const char *achievementName(AchievementId id) {
  return uint8_t(id) < ACHIEVEMENT_COUNT ? definitions[uint8_t(id)].name : "???";
}
const char *achievementDescription(AchievementId id) {
  return uint8_t(id) < ACHIEVEMENT_COUNT ? definitions[uint8_t(id)].description : "";
}
uint32_t eligibleAchievements(const BuddySaveData &s) {
  if (!s.totalObservations) return 0;
  unsigned variety = 0, severe = 0;
  for (uint8_t i = 0; i < WEATHER_CATEGORY_COUNT; ++i) if (s.discoveredWeather & (1UL << i)) ++variety;
  for (const auto &e : s.fieldEvents) if (e.count) ++severe;
  uint32_t mask = 0;
  for (uint8_t i = 0; i < ACHIEVEMENT_COUNT; ++i) {
    const auto &d = definitions[i]; bool earned = false;
    switch (d.source) {
      case Source::OBSERVATIONS: earned = s.totalObservations >= uint32_t(d.threshold); break;
      case Source::VARIETY: earned = variety >= unsigned(d.threshold); break;
      case Source::HIGH_TEMPERATURE: earned = s.highestTemperatureMilliC >= d.threshold; break;
      case Source::LOW_TEMPERATURE: earned = s.lowestTemperatureMilliC <= d.threshold; break;
      case Source::SITES: earned = s.fieldSitesVisited >= uint32_t(d.threshold); break;
      case Source::SEVERE: earned = severe >= unsigned(d.threshold); break;
      case Source::EVENT: earned = s.fieldEvents[d.threshold].count > 0; break;
    }
    if (earned) mask |= 1UL << i;
  }
  return mask;
}
uint32_t evaluateAchievements(BuddySaveData &s, int64_t timestamp) {
  if (timestamp <= 0 || timestamp < s.createdAt || timestamp > s.latestObservationAt) return 0;
  uint32_t added = eligibleAchievements(s) & ~s.unlockedAchievements;
  for (uint8_t i = 0; i < ACHIEVEMENT_COUNT; ++i) if (added & (1UL << i)) s.achievementUnlockedAt[i] = timestamp;
  s.unlockedAchievements |= added;
  return added;
}
void reconstructAchievements(BuddySaveData &s) {
  evaluateAchievements(s, s.latestObservationAt);
}
