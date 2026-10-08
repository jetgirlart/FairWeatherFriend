#ifndef FWF_PALETTE_H
#define FWF_PALETTE_H
#include <stdint.h>

// RGB565: a dark field, warm paper/fox, cool environment, muted clothing.
constexpr uint16_t COLOR_BACKGROUND = 0x0841;
constexpr uint16_t COLOR_TEXT = 0xEF3B;
constexpr uint16_t COLOR_WARM = 0xEDEA;
constexpr uint16_t COLOR_COOL = 0x6C98;
constexpr uint16_t COLOR_MUTED = 0x7BEF;
constexpr uint16_t COLOR_FUR = 0xED49;
// Rendering-only runtime colors. No save fields or duplicate sprite sheets.
struct PetPalette {
  uint16_t fur = COLOR_FUR;
  uint16_t gear[8] = {COLOR_TEXT, COLOR_MUTED, COLOR_TEXT, COLOR_COOL,
                       COLOR_COOL, COLOR_WARM, COLOR_MUTED, COLOR_MUTED};
};
extern PetPalette petPalette;

#endif
