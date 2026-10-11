#include "palette.h"
namespace {
// OUTLINE < PRIMARY < SECONDARY/ACCENT < DETAIL brightness in every palette.
const SpritePalette palettes[FUR_PALETTE_COUNT] = {
  {COLOR_OUTLINE, 0xD384, 0xFED3, COLOR_DETAIL}, // ORANGE
  {COLOR_OUTLINE, 0xBD0B, 0xFF98, COLOR_DETAIL}, // CREAM
  {COLOR_OUTLINE, 0x6B8F, 0xDF1D, COLOR_DETAIL}, // GRAY
  {COLOR_OUTLINE, 0x8A65, 0xE5F2, COLOR_DETAIL}, // BROWN
  {COLOR_OUTLINE, 0x3296, 0xAE7F, COLOR_DETAIL}  // BLUE
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
