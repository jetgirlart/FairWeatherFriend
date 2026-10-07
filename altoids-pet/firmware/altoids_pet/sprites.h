#pragma once

#include <Arduino.h>

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

// Crisp 2x presentation (48x48). Offsets are in physical OLED pixels.
// The base artwork stays 24x24; accessories are drawn by pet/display code.
void drawKitsuneSprite(const uint8_t *bitmap, int x, int y,
                       uint8_t earOffset = 0, bool crouching = false);
