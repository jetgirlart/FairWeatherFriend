#ifndef FWF_HARDWARE_H
#define FWF_HARDWARE_H
#include <Arduino.h>

// XIAO ESP32-S3: module SCL/SDA labels mean SPI clock/data, not I2C.
constexpr int TFT_WIDTH = 240;
constexpr int TFT_HEIGHT = 240;
constexpr uint8_t TFT_ROTATION = 0;
constexpr int TFT_CS = D7;
constexpr int TFT_DC = D6;
constexpr int TFT_RST = D4;
constexpr int TFT_BL = D9;
constexpr int TFT_SCK = D8;
constexpr int TFT_MOSI = D10;
constexpr uint32_t TFT_SPI_HZ = 40000000;
// D5 is reserved for the future Hall sensor. Do not initialize it or SPI MISO.

#endif
