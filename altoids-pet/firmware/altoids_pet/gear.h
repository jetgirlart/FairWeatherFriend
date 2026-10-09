#pragma once
#include "save.h"

uint32_t gearFlag(GearId gear);
const char *gearName(GearId gear);
uint32_t eligibleGear(const BuddySaveData &data);
void evaluateGearUnlocks(BuddySaveData &data);
GearSlot gearSlot(GearId gear);
const char *gearSlotName(GearSlot slot);
bool gearFitsSlot(GearId gear, GearSlot slot);
// NONE removes only the specified slot. Locked or incompatible gear is rejected.
bool equipGear(GearSlot slot, GearId gear);

bool equipGearVariant(GearSlot slot, GearId gear, uint8_t variant);
