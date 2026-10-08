#pragma once
#include <Arduino.h>
#include "save.h"

// Coordinates are the transformed base bitmap's top-left, in OLED pixels.
// Pure framebuffer composition: no persistence, timing, or display transfer.
void drawEquippedGear(int x, int y, bool crouching, bool liftedFeet,
                      GearId foregroundAccessory = GearId::NONE);
