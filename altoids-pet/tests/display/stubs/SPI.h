#pragma once
#define SPI_MODE0 0
struct SPIClass { void begin(int,int,int,int); };
extern SPIClass SPI;
