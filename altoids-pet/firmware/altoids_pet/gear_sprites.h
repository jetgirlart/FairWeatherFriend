#pragma once
#include "sprites.h"

// Gear-only layers, never copies of the base pet. Same 24x24/MSB-first format.
// Foreground bits paint white; optional mask bits erase only covered pixels.
extern const uint8_t GEAR_FIELD_CAP[KITSUNE_BITMAP_BYTES] PROGMEM;
extern const uint8_t GEAR_FIELD_CAP_MASK[KITSUNE_BITMAP_BYTES] PROGMEM;
extern const uint8_t GEAR_SUNGLASSES[KITSUNE_BITMAP_BYTES] PROGMEM;
extern const uint8_t GEAR_UMBRELLA[KITSUNE_BITMAP_BYTES] PROGMEM;
extern const uint8_t GEAR_UMBRELLA_MASK[KITSUNE_BITMAP_BYTES] PROGMEM;
extern const uint8_t GEAR_RAINCOAT[KITSUNE_BITMAP_BYTES] PROGMEM;
extern const uint8_t GEAR_RAINCOAT_MASK[KITSUNE_BITMAP_BYTES] PROGMEM;
extern const uint8_t GEAR_WINTER_SCARF[KITSUNE_BITMAP_BYTES] PROGMEM;
extern const uint8_t GEAR_WINTER_SCARF_MASK[KITSUNE_BITMAP_BYTES] PROGMEM;
extern const uint8_t GEAR_WINTER_COAT[KITSUNE_BITMAP_BYTES] PROGMEM;
extern const uint8_t GEAR_WINTER_COAT_MASK[KITSUNE_BITMAP_BYTES] PROGMEM;
extern const uint8_t GEAR_BOOTS[KITSUNE_BITMAP_BYTES] PROGMEM;
extern const uint8_t GEAR_BOOTS_MASK[KITSUNE_BITMAP_BYTES] PROGMEM;
