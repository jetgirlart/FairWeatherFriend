#ifndef FWF_PALETTE_H
#define FWF_PALETTE_H
#include <stdint.h>

// Stable persisted IDs; never reorder.
enum class FurPaletteId : uint8_t { ORANGE = 0, CREAM = 1, GRAY = 2, BROWN = 3, BLUE = 4, COUNT = 5 };
constexpr uint8_t FUR_PALETTE_COUNT = 5;
bool validFurPalette(FurPaletteId id);
const char *furPaletteName(FurPaletteId id);

enum class SpritePixelRole : uint8_t { TRANSPARENT, OUTLINE, PRIMARY, ACCENT, DETAIL };
struct SpritePalette { uint16_t outline, primary, accent, detail; };
const SpritePalette &furPalette(FurPaletteId id);

// RGB565: a dark field, warm paper/fox, cool environment, muted clothing.
// Semantic roles run from black outline through medium/light tones to white.
constexpr uint16_t COLOR_OUTLINE = 0x0000;
constexpr uint16_t COLOR_DETAIL = 0xFFFF;
constexpr uint16_t COLOR_BACKGROUND = 0x0841;
constexpr uint16_t COLOR_TEXT = 0xEF3B;
constexpr uint16_t COLOR_WARM = 0xEDEA;
constexpr uint16_t COLOR_COOL = 0x6C98;
constexpr uint16_t COLOR_MUTED = 0x7BEF;
constexpr uint16_t COLOR_FUR = 0xED49;
// Existing rendering-only gear tints. Fur selection lives in BuddySaveData;
// SpritePalette roles can also be used by future gear masks without new art sets.
struct PetPalette {
  uint16_t gear[8] = {COLOR_TEXT, COLOR_MUTED, COLOR_TEXT, COLOR_COOL,
                       COLOR_COOL, COLOR_WARM, COLOR_MUTED, COLOR_MUTED};
};
extern PetPalette petPalette;

#endif
