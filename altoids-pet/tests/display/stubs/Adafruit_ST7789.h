#pragma once
#include "SPI.h"
#include <stdint.h>
struct Adafruit_ST7789 {
 Adafruit_ST7789(SPIClass*,int,int,int);
 void init(int,int,int);
 void setRotation(int);
 void setSPISpeed(uint32_t);
 void startWrite();
 void setAddrWindow(int,int,int,int);
 void writePixels(uint16_t*,int);
 void endWrite();
 void enableDisplay(bool);
 void enableSleep(bool);
};
