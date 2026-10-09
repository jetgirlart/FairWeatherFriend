#pragma once
#include "save.h"

// Variant IDs are local to each item. Never reorder; 0 is the original artwork tint.
constexpr uint8_t GEAR_ITEM_COUNT = 7;
constexpr uint8_t MAX_GEAR_VARIANTS = 5;
uint8_t gearVariantCount(GearId gear);
const char *gearVariantName(GearId gear, uint8_t variant);
bool gearVariantUnlocked(const BuddySaveData &data, GearId gear, uint8_t variant);
void initializeGearVariants(BuddySaveData &data);
void evaluateGearVariants(BuddySaveData &data);
SpritePalette gearVariantPalette(GearId gear, uint8_t variant);
bool patternedGearVariant(GearId gear, uint8_t variant);
