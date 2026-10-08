#pragma once
#include "save.h"

uint32_t gearFlag(GearId gear);
const char *gearName(GearId gear);
uint32_t eligibleGear(const BuddySaveData &data);
void evaluateGearUnlocks(BuddySaveData &data);
// Selection hook for the future GEAR UI. Returns false for locked gear.
bool equipGear(GearId gear);
