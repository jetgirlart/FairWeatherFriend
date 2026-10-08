#ifndef FWF_DISPLAY_SURFACE_H
#define FWF_DISPLAY_SURFACE_H
#include <Adafruit_GFX.h>
#include "hardware.h"
#include "palette.h"

// UI coordinates are physical TFT pixels. During pet composition only, retain
// the existing animation's local 2x geometry and map it to crisp 3x source art.
class DisplaySurface : public Adafruit_GFX {
public:
  DisplaySurface();
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void clearDisplay();
  void display();
  bool ready() const { return canvas.getBuffer() != nullptr; }
  uint16_t *getBuffer() { return canvas.getBuffer(); }
  void beginPet(int x, int y) { petX = x; petY = y; petTransform = true; }
  void endPet() { petTransform = false; }
  void invalidate() { sentValid = false; }
private:
  GFXcanvas16 canvas;
  static constexpr int TILE = 16;
  static constexpr int COLUMNS = (TFT_WIDTH + TILE - 1) / TILE;
  static constexpr int ROWS = (TFT_HEIGHT + TILE - 1) / TILE;
  uint32_t sentHashes[COLUMNS * ROWS] = {};
  bool sentValid = false;
  bool petTransform = false;
  int petX = 0, petY = 0;
};

extern DisplaySurface display;
void initializeDisplayBus();
void initializeDisplay();
void sleepDisplay();

#endif
