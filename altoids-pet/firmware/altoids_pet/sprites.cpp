#include "sprites.h"
#include "display.h"

namespace {
int originX = 0, originY = 0;
// Preserve the old movement amplitudes while the native art now renders at 2x.
int localPixel(int value) { return value >= 0 ? value * 3 / 2 : -((-value * 3 + 1) / 2); }
void pixel(int x, int y, int sx, int sy, uint16_t color, uint8_t ear, bool crouch, int offsetY = 0) {
  int left = originX + localPixel(x) + sx * 2;
  if (sy < 14 && sx < 19) left += localPixel(ear);
  int top = originY + localPixel(y) + offsetY;
  uint16_t *buffer = display.getBuffer();
  for (int dy = 0; dy < 2; ++dy) {
    int row = sy * 2 + dy;
    if (crouch && (row == 69 || row == 73 || row == 77 || row == 81)) continue;
    int Y = top + row - (crouch ? (row > 69) + (row > 73) + (row > 77) + (row > 81) : 0);
    for (int dx = 0; dx < 2; ++dx) {
      int X = left + dx;
      if (buffer && X >= 0 && X < TFT_WIDTH && Y >= 0 && Y < TFT_HEIGHT) buffer[Y * TFT_WIDTH + X] = color;
    }
  }
}
}
void setSpriteOrigin(int x, int y) { originX = x; originY = y; }
void drawKitsuneSprite(const uint8_t *bitmap, int x, int y, uint8_t ear, bool crouch,
                      uint16_t color, int offsetY) {
  // Separate 48x48 MSB-first gear masks. Write only set pixels into the same
  // framebuffer; the existing display() transfers the completed frame once.
  for (int sy = 0; sy < 48; ++sy) for (int sx = 0; sx < 48; ++sx) {
    if (pgm_read_byte(bitmap + sy * 6 + sx / 8) & (0x80 >> (sx % 8)))
      pixel(x, y, sx, sy, color, ear, crouch, offsetY);
  }
}
void buildKitsuneRoles(const uint8_t *frame, uint8_t *roles) {
  for (int i = 0; i < KITSUNE_PIXEL_COUNT; ++i) {
    uint8_t packed = pgm_read_byte(frame + i / 2);
    roles[i] = i % 2 ? packed & 15 : packed >> 4;
  }
}
void drawRoleSprite(const uint8_t *roles, int x, int y, const SpritePalette &palette, uint8_t ear, bool crouch) {
  const uint16_t colors[] = {0, palette.outline, palette.primary, palette.accent, palette.detail};
  for (int i = 0; i < KITSUNE_PIXEL_COUNT; ++i) {
    uint8_t role = roles[i];
    if (role > 0 && role <= 4) pixel(x, y, i % 48, i / 48, colors[role], ear, crouch);
  }
}
void drawPackedPaletteSprite(const uint8_t *frame, int x, int y, const SpritePalette &palette, uint8_t ear, bool crouch, int offsetY) {
  const uint16_t colors[] = {0, palette.outline, palette.primary, palette.accent, palette.detail};
  for (int i = 0; i < KITSUNE_PIXEL_COUNT; ++i) {
    uint8_t packed = pgm_read_byte(frame + i / 2);
    uint8_t role = i % 2 ? packed & 15 : packed >> 4;
    if (role > 0 && role <= 4) pixel(x, y, i % 48, i / 48, colors[role], ear, crouch, offsetY);
  }
}
void drawColoredKitsune(const uint8_t *frame, int x, int y, FurPaletteId palette, uint8_t ear, bool crouch) {
  drawPackedPaletteSprite(frame, x, y, furPalette(palette), ear, crouch);
}

void drawPaletteGearSprite(const uint8_t *bitmap, int x, int y, const SpritePalette &palette, bool patterned, bool crouch, int offsetY) {
  for (int sy = 0; sy < 48; ++sy) for (int sx = 0; sx < 48; ++sx)
    if (pgm_read_byte(bitmap + sy * 6 + sx / 8) & (0x80 >> (sx % 8))) {
      // Existing one-bit art is PRIMARY. Special colorways alternate ACCENT in
      // chunky source-pixel blocks, without extra masks or base-pet copies.
      uint16_t color = patterned && ((sx / 4 + sy / 4) & 1) ? palette.accent : palette.primary;
      pixel(x, y, sx, sy, color, 0, crouch, offsetY);
    }
}
