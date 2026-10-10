#include "palette.h"
namespace {
// OUTLINE < PRIMARY < SECONDARY/ACCENT < DETAIL brightness in every palette.
const SpritePalette palettes[FUR_PALETTE_COUNT] = {
  {COLOR_OUTLINE, 0xCBA7, 0xF73A, COLOR_DETAIL}, // ORANGE
  {COLOR_OUTLINE, 0xCE13, 0xFFBC, COLOR_DETAIL}, // CREAM
  {COLOR_OUTLINE, 0x7BEF, 0xDEDB, COLOR_DETAIL}, // GRAY
  {COLOR_OUTLINE, 0x8265, 0xDE35, COLOR_DETAIL}, // BROWN
  {COLOR_OUTLINE, 0x43F2, 0xC6FA, COLOR_DETAIL}  // BLUE
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
