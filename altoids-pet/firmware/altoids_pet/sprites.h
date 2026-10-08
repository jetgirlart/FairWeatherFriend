#pragma once

#include <Arduino.h>
#include "palette.h"

// Adafruit_GFX bitmap layout: three MSB-first bytes per row, 24 rows.
constexpr uint8_t KITSUNE_WIDTH = 24;
constexpr uint8_t KITSUNE_HEIGHT = 24;
constexpr uint8_t KITSUNE_BITMAP_BYTES = 72;

extern const uint8_t KITSUNE_IDLE[KITSUNE_BITMAP_BYTES] PROGMEM;
extern const uint8_t KITSUNE_BLINK[KITSUNE_BITMAP_BYTES] PROGMEM;
extern const uint8_t KITSUNE_LOOK_LEFT[KITSUNE_BITMAP_BYTES] PROGMEM;
extern const uint8_t KITSUNE_LOOK_RIGHT[KITSUNE_BITMAP_BYTES] PROGMEM;
extern const uint8_t KITSUNE_HAPPY[KITSUNE_BITMAP_BYTES] PROGMEM;
extern const uint8_t KITSUNE_EXCITED[KITSUNE_BITMAP_BYTES] PROGMEM;
extern const uint8_t KITSUNE_SLEEPY[KITSUNE_BITMAP_BYTES] PROGMEM;
extern const uint8_t KITSUNE_SLEEP[KITSUNE_BITMAP_BYTES] PROGMEM;
extern const uint8_t KITSUNE_BOUNCE[KITSUNE_BITMAP_BYTES] PROGMEM;
extern const uint8_t KITSUNE_LOOK_UP[KITSUNE_BITMAP_BYTES] PROGMEM;

// Local 2x masks retain existing animation offsets; DisplaySurface maps them
// to pixel-perfect 3x TFT art. Color is supplied at render time; masks use background.
void drawKitsuneSprite(const uint8_t *bitmap, int x, int y,
                       uint8_t earOffset = 0, bool crouching = false, uint16_t color = COLOR_FUR);

// Shared role renderer: RAM role map, 24x24, reusable for future gear variants.
void drawRoleSprite(const uint8_t *roles, int x, int y, const SpritePalette &palette,
                    uint8_t earOffset = 0, bool crouching = false);
void buildKitsuneRoles(const uint8_t *bitmap, uint8_t *roles);
void drawColoredKitsune(const uint8_t *bitmap, int x, int y, FurPaletteId palette,
                        uint8_t earOffset = 0, bool crouching = false);
