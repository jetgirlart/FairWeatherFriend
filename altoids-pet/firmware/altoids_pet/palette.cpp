#include "palette.h"
namespace {
// Outline/primary/accent/detail. All sets retain high-contrast facial details.
const SpritePalette palettes[FUR_PALETTE_COUNT] = {
  {COLOR_FUR, 0xCBA7, 0xF73A, 0x1082}, // ORANGE: original warm kitsune color.
  {0xC5D4, 0xEF19, 0xFFBC, 0x18C3}, // CREAM
  {0xAD55, 0x7BEF, 0xDEDB, 0x1082}, // GRAY
  {0xBC0B, 0x8265, 0xDE35, 0x1082}, // BROWN
  {0x7D9C, 0x43F2, 0xC6FA, 0x1082}  // BLUE
};
}
bool validFurPalette(FurPaletteId id) { return static_cast<uint8_t>(id) < FUR_PALETTE_COUNT; }
const char *furPaletteName(FurPaletteId id) {
  const char *names[] = {"ORANGE", "CREAM", "GRAY", "BROWN", "BLUE"};
  return validFurPalette(id) ? names[static_cast<uint8_t>(id)] : "UNKNOWN";
}
const SpritePalette &furPalette(FurPaletteId id) {
  return palettes[validFurPalette(id) ? static_cast<uint8_t>(id) : 0];
}
