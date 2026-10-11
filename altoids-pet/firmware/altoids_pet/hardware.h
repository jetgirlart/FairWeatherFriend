#ifndef FWF_HARDWARE_H
#define FWF_HARDWARE_H
#include <Arduino.h>

// XIAO ESP32-S3: module SCL/SDA labels mean SPI clock/data, not I2C.
constexpr int TFT_WIDTH = 240;
constexpr int TFT_HEIGHT = 240;
constexpr uint8_t TFT_ROTATION = 0;
// Only peripheral on this SPI bus: module CS is wired to GND.
constexpr int TFT_CS = -1;
constexpr int TFT_DC = D6;
// Retain dedicated hardware reset for reliable panel initialization/recovery.
constexpr int TFT_RST = D4;
constexpr int TFT_BL = D9;
constexpr int TFT_SCK = D8;
constexpr int TFT_MOSI = D10;
constexpr uint32_t TFT_SPI_HZ = 40000000;
// With grounded CS, keep the clock idle HIGH (mode 3). Restore mode 0 if
// reverting to the original D7-controlled CS wiring.
// ESP32 SPI_MODE3 = 3; keep SPI library types out of the shared pin header.
constexpr uint8_t TFT_SPI_MODE = 3;
// D7 is free for future GNSS. D4 remains the TFT reset output.
// D5 is reserved for the future Hall sensor. Do not initialize it or SPI MISO.

#endif
