#include "gear_variants.h"
#include "gear.h"
#include <string.h>

#ifdef ARDUINO
#define GEAR_FLASH PROGMEM
#else
#define GEAR_FLASH
#endif
namespace {
const char *names[GEAR_ITEM_COUNT][MAX_GEAR_VARIANTS] = {
 {"ORIGINAL", "GREEN", "NAVY", "HEAT", nullptr},
 {"ORIGINAL", "BLUE", "RED", "HEAT", nullptr},
 {"ORIGINAL", "BLUE", "RED", "RAINBOW", "SOUWESTER"},
 {"ORIGINAL", "BLUE", "RED", "STORM", "STORM CHASER"},
 {"ORIGINAL", "BLUE", "PATTERNED", "BLIZZARD", nullptr},
 {"ORIGINAL", "GREEN", "WHITE", "BLIZZARD", nullptr},
 {"ORIGINAL", "YELLOW", "WATERPROOF", nullptr, nullptr}
};
enum class Milestone : uint8_t { TOTAL, CLEAR, RAIN, SNOW, FREEZING, EVENT };
struct VariantRule { GearId item; uint8_t variant; Milestone source; uint64_t threshold; FieldEventId event = FieldEventId::COUNT; };
// All thresholds live here. Base ownership still follows the original gear rules.
const VariantRule variantRules[] = {
 {GearId::FIELD_CAP,1,Milestone::TOTAL,25}, {GearId::FIELD_CAP,2,Milestone::TOTAL,100},
 {GearId::SUNGLASSES,1,Milestone::CLEAR,25}, {GearId::SUNGLASSES,2,Milestone::CLEAR,100},
 {GearId::UMBRELLA,1,Milestone::RAIN,10}, {GearId::UMBRELLA,2,Milestone::RAIN,25}, {GearId::UMBRELLA,3,Milestone::RAIN,50},
 {GearId::RAINCOAT,1,Milestone::RAIN,25}, {GearId::RAINCOAT,2,Milestone::RAIN,50}, {GearId::RAINCOAT,3,Milestone::RAIN,100},
 {GearId::WINTER_SCARF,1,Milestone::SNOW,10}, {GearId::WINTER_SCARF,2,Milestone::SNOW,25},
 {GearId::WINTER_COAT,1,Milestone::SNOW,15}, {GearId::WINTER_COAT,2,Milestone::SNOW,30},
 {GearId::BOOTS,1,Milestone::FREEZING,10},
 {GearId::BOOTS,2,Milestone::EVENT,1,FieldEventId::FLASH_FLOOD_WARNING},
 {GearId::RAINCOAT,4,Milestone::EVENT,1,FieldEventId::TORNADO_WARNING},
 {GearId::UMBRELLA,4,Milestone::EVENT,1,FieldEventId::HURRICANE_WARNING},
 {GearId::WINTER_SCARF,3,Milestone::EVENT,1,FieldEventId::BLIZZARD_WARNING},
 {GearId::WINTER_COAT,3,Milestone::EVENT,1,FieldEventId::BLIZZARD_WARNING},
 {GearId::FIELD_CAP,3,Milestone::EVENT,1,FieldEventId::EXTREME_HEAT_WARNING},
 {GearId::SUNGLASSES,3,Milestone::EVENT,1,FieldEventId::EXTREME_HEAT_WARNING}
};
// Medium primary / light secondary tones; outline and white detail are shared.
// Gear primaries are distinct from all five buddy fur primaries.
// Stable variant IDs/names and unlock rules are unchanged.
const uint16_t colors[GEAR_ITEM_COUNT][MAX_GEAR_VARIANTS][2] GEAR_FLASH = {
 {{0x6B4D,0xCE79},{0x65C8,0xC6B6},{0x4295,0x9D7C},{0xCDE0,0xFFB5},{0,0}},
 {{0x6B4D,0xDEDB},{0x4C9F,0xCEFF},{0xD945,0xFEB8},{0xCBA0,0xFFB5},{0,0}},
 {{0x2390,0xC6FA},{0x4C9F,0xCEFF},{0xD945,0xFEB8},{0xD945,0xC776},{0xCDE0,0xFFB5}},
 {{0x2390,0xC6FA},{0x4C9F,0xCEFF},{0xD945,0xFEB8},{0x5AD6,0xBE7D},{0x4295,0xEF95}},
 {{0xBC0B,0xF73A},{0x4C9F,0xCEFF},{0xD945,0xFEB8},{0x7C97,0xDEFF},{0,0}},
 {{0x6B4D,0xDEDB},{0x65C8,0xC6B6},{0xAD55,0xEF7D},{0x7C97,0xDEFF},{0,0}},
 {{0x6B4D,0xCE79},{0xCDE0,0xFFB5},{0x3BA9,0xBEBB},{0,0},{0,0}}
};
}
uint8_t gearVariantCount(GearId gear) {
 uint8_t id=uint8_t(gear); if(id<1 || id>GEAR_ITEM_COUNT)return 0;
 uint8_t count=0;while(count<MAX_GEAR_VARIANTS && names[id-1][count])++count;return count;
}
const char *gearVariantName(GearId gear,uint8_t variant) {
 return variant<gearVariantCount(gear)?names[uint8_t(gear)-1][variant]:"UNKNOWN";
}
bool gearVariantUnlocked(const BuddySaveData &data,GearId gear,uint8_t variant) {
 return variant<gearVariantCount(gear) && (data.unlockedGear & gearFlag(gear)) &&
        (data.unlockedVariants[uint8_t(gear)-1] & (1UL<<variant));
}
void initializeGearVariants(BuddySaveData &data) {
 for(uint8_t id=1;id<=GEAR_ITEM_COUNT;++id)if(data.unlockedGear & gearFlag(GearId(id)))data.unlockedVariants[id-1]|=1;
}
void evaluateGearVariants(BuddySaveData &data) {
 initializeGearVariants(data);
 for(const auto &rule:variantRules){
  if(!(data.unlockedGear & gearFlag(rule.item)))continue;
  uint64_t count=0;
  switch(rule.source){
   case Milestone::TOTAL:count=data.totalObservations;break;
   // Preserve the existing sunny category pair; saturate rather than overflow.
   case Milestone::CLEAR:count=data.weatherCounts[0];count=UINT64_MAX-count<data.weatherCounts[1]?UINT64_MAX:count+data.weatherCounts[1];break;
   case Milestone::RAIN:count=data.weatherCounts[uint8_t(WeatherCategory::RAIN)];break;
   case Milestone::SNOW:count=data.weatherCounts[uint8_t(WeatherCategory::SNOW)];break;
   case Milestone::FREEZING:count=data.freezingObservations;break;
   case Milestone::EVENT:count=data.fieldEvents[uint8_t(rule.event)].count;break;
  }
  if(count>=rule.threshold)data.unlockedVariants[uint8_t(rule.item)-1]|=1UL<<rule.variant;
 }
}
SpritePalette gearVariantPalette(GearId gear,uint8_t variant) {
 if(variant>=gearVariantCount(gear))return {COLOR_OUTLINE,0x6B4D,0xDEDB,COLOR_DETAIL};
 #ifdef ARDUINO
 uint16_t primary=pgm_read_word(&colors[uint8_t(gear)-1][variant][0]);
 uint16_t accent=pgm_read_word(&colors[uint8_t(gear)-1][variant][1]);
 #else
 uint16_t primary=colors[uint8_t(gear)-1][variant][0], accent=colors[uint8_t(gear)-1][variant][1];
 #endif
 return {COLOR_OUTLINE,primary,accent,COLOR_DETAIL};
}
bool patternedGearVariant(GearId gear,uint8_t variant){return variant>=2 &&
 (gear==GearId::WINTER_SCARF || variant>=3 || (gear==GearId::BOOTS && variant==2));}
