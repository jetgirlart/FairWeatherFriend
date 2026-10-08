#include "display_surface.h"
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <driver/gpio.h>

namespace {
Adafruit_ST7789 tft(&SPI, TFT_CS, TFT_DC, TFT_RST);
bool panelReady = false;
bool backlightOn = false;
// floor(3*v/2), including negative offsets used by accessories/weather poses.
int scalePet(int value) { return value >= 0 ? value * 3 / 2 : -((-value * 3 + 1) / 2); }
}
PetPalette petPalette;
DisplaySurface display;
DisplaySurface::DisplaySurface() : Adafruit_GFX(TFT_WIDTH, TFT_HEIGHT), canvas(TFT_WIDTH, TFT_HEIGHT) {
  setTextWrap(false);
}
void DisplaySurface::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if (!petTransform) { canvas.drawPixel(x, y, color); return; }
  int left = scalePet(x), top = scalePet(y);
  canvas.fillRect(petX + left, petY + top,
                  scalePet(x + 1) - left, scalePet(y + 1) - top, color);
}
void DisplaySurface::clearDisplay() {
  // Compose in RAM. Never send this intermediate background to the TFT.
  canvas.fillScreen(COLOR_BACKGROUND);
}
void DisplaySurface::display() {
  if (!panelReady || !ready()) return;
  uint16_t *pixels = canvas.getBuffer();
  // Hash completed tiles instead of retaining a second 115200-byte RGB frame.
  // No transfer occurs for unchanged tiles, even if a scheduler recomposes RAM.
  for (int ty = 0; ty < TFT_HEIGHT; ty += TILE) {
    for (int tx = 0; tx < TFT_WIDTH; tx += TILE) {
      int w = min(TILE, TFT_WIDTH - tx), h = min(TILE, TFT_HEIGHT - ty);
      uint32_t hash = 2166136261UL;
      for (int y = ty; y < ty + h; ++y) for (int x = tx; x < tx + w; ++x) {
        uint16_t pixel = pixels[y * TFT_WIDTH + x];
        hash = (hash ^ (pixel & 255)) * 16777619UL;
        hash = (hash ^ (pixel >> 8)) * 16777619UL;
      }
      int index = (ty / TILE) * COLUMNS + tx / TILE;
      if (sentValid && sentHashes[index] == hash) continue;
      tft.startWrite();
      tft.setAddrWindow(tx, ty, w, h);
      for (int y = ty; y < ty + h; ++y) tft.writePixels(pixels + y * TFT_WIDTH + tx, w);
      tft.endWrite();
      sentHashes[index] = hash;
    }
  }
  sentValid = true;
  // First visible frame is the restored home/timer screen, not an empty boot.
  if (!backlightOn) { digitalWrite(TFT_BL, HIGH); backlightOn = true; }
}
void initializeDisplayBus() {
  digitalWrite(TFT_BL, LOW);
  pinMode(TFT_BL, OUTPUT);
  gpio_deep_sleep_hold_dis();
  gpio_hold_dis(static_cast<gpio_num_t>(TFT_BL));
  digitalWrite(TFT_BL, LOW);
  backlightOn = false;
  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
}
void initializeDisplay() {
  if (!display.ready()) {
    Serial.println("ERROR: TFT framebuffer allocation failed.");
    while (true) yield();
  }
  tft.init(TFT_WIDTH, TFT_HEIGHT, SPI_MODE0);
  tft.setRotation(TFT_ROTATION);
  tft.setSPISpeed(TFT_SPI_HZ);
  panelReady = true;
  display.invalidate();
  display.clearDisplay();
}
void sleepDisplay() {
  digitalWrite(TFT_BL, LOW);
  backlightOn = false;
  // Hold the output LOW through deep sleep rather than leaving BLK floating.
  gpio_hold_en(static_cast<gpio_num_t>(TFT_BL));
  gpio_deep_sleep_hold_en();
  tft.enableDisplay(false);
  tft.enableSleep(true);
  panelReady = false;
  display.invalidate();
}
