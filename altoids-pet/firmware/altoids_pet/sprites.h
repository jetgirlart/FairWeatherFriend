#ifndef FWF_SPRITES_H
#define FWF_SPRITES_H
#include <Arduino.h>
#include "palette.h"

constexpr uint8_t KITSUNE_WIDTH = 48;
constexpr uint8_t KITSUNE_HEIGHT = 48;
constexpr uint16_t KITSUNE_PIXEL_COUNT = KITSUNE_WIDTH * KITSUNE_HEIGHT;
constexpr uint16_t KITSUNE_FRAME_BYTES = KITSUNE_PIXEL_COUNT / 2;
// Native role maps: two pixels per byte, high nibble first, PROGMEM.
// 0 transparent, 1 outline, 2 primary, 3 accent, 4 facial/detail.
extern const uint8_t KITSUNE_IDLE[KITSUNE_FRAME_BYTES] PROGMEM;
extern const uint8_t KITSUNE_BLINK[KITSUNE_FRAME_BYTES] PROGMEM;
extern const uint8_t KITSUNE_LOOK_LEFT[KITSUNE_FRAME_BYTES] PROGMEM;
extern const uint8_t KITSUNE_LOOK_RIGHT[KITSUNE_FRAME_BYTES] PROGMEM;
extern const uint8_t KITSUNE_HAPPY[KITSUNE_FRAME_BYTES] PROGMEM;
extern const uint8_t KITSUNE_EXCITED[KITSUNE_FRAME_BYTES] PROGMEM;
extern const uint8_t KITSUNE_SLEEPY[KITSUNE_FRAME_BYTES] PROGMEM;
extern const uint8_t KITSUNE_SLEEP[KITSUNE_FRAME_BYTES] PROGMEM;
extern const uint8_t KITSUNE_BOUNCE[KITSUNE_FRAME_BYTES] PROGMEM;
extern const uint8_t KITSUNE_LOOK_UP[KITSUNE_FRAME_BYTES] PROGMEM;
extern const uint8_t KITSUNE_FOCUS[KITSUNE_FRAME_BYTES] PROGMEM;

// UI supplies the physical origin; the existing pet animation coordinates
// remain local. No display-driver or scheduler changes are required.
void setSpriteOrigin(int x, int y);
void drawKitsuneSprite(const uint8_t *bitmap, int x, int y,
                      uint8_t earOffset = 0, bool crouching = false,
                      uint16_t color = COLOR_FUR, int pixelOffsetY = 0);
void buildKitsuneRoles(const uint8_t *frame, uint8_t *roles);
void drawRoleSprite(const uint8_t *roles, int x, int y, const SpritePalette &palette,
                    uint8_t earOffset = 0, bool crouching = false);
void drawColoredKitsune(const uint8_t *frame, int x, int y, FurPaletteId palette,
                        uint8_t earOffset = 0, bool crouching = false);

void drawPaletteGearSprite(const uint8_t *bitmap, int x, int y, const SpritePalette &palette, bool patterned, bool crouch, int offsetY = 0);

// Generic packed-role renderer for source buddy/gear PNGs. SECONDARY maps to
// palette.accent; transparency writes nothing. Gear callers pass gearVariantPalette.
void drawPackedPaletteSprite(const uint8_t *frame, int x, int y, const SpritePalette &palette,
                             uint8_t earOffset = 0, bool crouching = false, int pixelOffsetY = 0);

#endif // FWF_SPRITES_H
