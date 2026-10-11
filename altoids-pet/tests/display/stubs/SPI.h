#pragma once
#include <stdint.h>
#define SPI_MODE0 0
#define SPI_MODE3 3
#define MSBFIRST 1
struct SPISettings {
 uint32_t hz; int order,mode;
 SPISettings(uint32_t h,int o,int m):hz(h),order(o),mode(m){}
};
struct SPIClass {
 void begin(int,int,int,int);
 void beginTransaction(SPISettings);
 void endTransaction();
};
extern SPIClass SPI;
