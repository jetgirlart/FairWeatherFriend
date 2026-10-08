#pragma once
#include "sprites.h"

constexpr uint16_t GEAR_BITMAP_BYTES = 48 * 48 / 8;

// Gear-only layers, never copies of the base pet. Native 48x48/MSB-first format.
// Foreground bits paint white; optional mask bits erase only covered pixels.
extern const uint8_t GEAR_FIELD_CAP[GEAR_BITMAP_BYTES] PROGMEM;
extern const uint8_t GEAR_FIELD_CAP_MASK[GEAR_BITMAP_BYTES] PROGMEM;
extern const uint8_t GEAR_SUNGLASSES[GEAR_BITMAP_BYTES] PROGMEM;
extern const uint8_t GEAR_UMBRELLA[GEAR_BITMAP_BYTES] PROGMEM;
extern const uint8_t GEAR_UMBRELLA_MASK[GEAR_BITMAP_BYTES] PROGMEM;
extern const uint8_t GEAR_RAINCOAT[GEAR_BITMAP_BYTES] PROGMEM;
extern const uint8_t GEAR_RAINCOAT_MASK[GEAR_BITMAP_BYTES] PROGMEM;
extern const uint8_t GEAR_WINTER_SCARF[GEAR_BITMAP_BYTES] PROGMEM;
extern const uint8_t GEAR_WINTER_SCARF_MASK[GEAR_BITMAP_BYTES] PROGMEM;
extern const uint8_t GEAR_WINTER_COAT[GEAR_BITMAP_BYTES] PROGMEM;
extern const uint8_t GEAR_WINTER_COAT_MASK[GEAR_BITMAP_BYTES] PROGMEM;
extern const uint8_t GEAR_BOOTS[GEAR_BITMAP_BYTES] PROGMEM;
extern const uint8_t GEAR_BOOTS_MASK[GEAR_BITMAP_BYTES] PROGMEM;
