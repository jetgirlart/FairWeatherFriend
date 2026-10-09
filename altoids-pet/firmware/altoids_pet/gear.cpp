#include "gear.h"
#include "journal.h"

namespace {
struct GearRule {
  GearId gear;
  WeatherCategory category;
  uint64_t minimumCount;
  bool belowFreezing;
};
// UNKNOWN means total observations; gear has no stat bonuses or care mechanics.
const GearRule rules[] = {
  {GearId::FIELD_CAP, WeatherCategory::UNKNOWN, 1, false},
  {GearId::SUNGLASSES, WeatherCategory::CLEAR, 1, false},
  {GearId::SUNGLASSES, WeatherCategory::MAINLY_CLEAR, 1, false},
  {GearId::UMBRELLA, WeatherCategory::RAIN, 1, false},
  {GearId::RAINCOAT, WeatherCategory::RAIN, 10, false},
  {GearId::WINTER_SCARF, WeatherCategory::SNOW, 1, false},
  {GearId::WINTER_COAT, WeatherCategory::SNOW, 5, false},
  {GearId::BOOTS, WeatherCategory::UNKNOWN, 1, true}
};
}

uint32_t gearFlag(GearId gear) {
  uint8_t id = static_cast<uint8_t>(gear);
  return id >= 1 && id <= 7 ? 1UL << (id - 1) : 0;
}

const char *gearName(GearId gear) {
  const char *names[] = {"NONE", "FIELD_CAP", "SUNGLASSES", "UMBRELLA", "RAINCOAT",
                         "WINTER_SCARF", "WINTER_COAT", "BOOTS"};
  uint8_t id = static_cast<uint8_t>(gear);
  return id <= 7 ? names[id] : "UNKNOWN";
}

uint32_t eligibleGear(const BuddySaveData &data) {
  uint32_t flags = 0;
  for (const GearRule &rule : rules) {
    uint64_t count = rule.category == WeatherCategory::UNKNOWN ? data.totalObservations :
                     data.weatherCounts[static_cast<uint8_t>(rule.category)];
    if (count >= rule.minimumCount && (!rule.belowFreezing || data.lowestTemperatureMilliC < 0)) {
      flags |= gearFlag(rule.gear);
    }
  }
  return flags;
}

void evaluateGearUnlocks(BuddySaveData &data) {
  data.unlockedGear |= eligibleGear(data);
}

GearSlot gearSlot(GearId gear) {
  switch (gear) {
    case GearId::FIELD_CAP: return GearSlot::HEAD;
    case GearId::SUNGLASSES: return GearSlot::FACE;
    case GearId::WINTER_SCARF: return GearSlot::NECK;
    case GearId::RAINCOAT: case GearId::WINTER_COAT: return GearSlot::BODY;
    case GearId::BOOTS: return GearSlot::FEET;
    case GearId::UMBRELLA: return GearSlot::PROP;
    default: return GearSlot::COUNT;
  }
}
const char *gearSlotName(GearSlot slot) {
  const char *names[] = {"HEAD", "FACE", "NECK", "BODY", "FEET", "PROP"};
  uint8_t id = static_cast<uint8_t>(slot);
  return id < GEAR_SLOT_COUNT ? names[id] : "UNKNOWN";
}
bool gearFitsSlot(GearId gear, GearSlot slot) {
  return static_cast<uint8_t>(slot) < GEAR_SLOT_COUNT &&
         (gear == GearId::NONE || gearSlot(gear) == slot);
}
bool equipGear(GearSlot slot, GearId gear) {
  return equipJournalGear(slot, gear);
}
