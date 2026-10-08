#pragma once
#include "../../journal/stubs/Arduino.h"
#define PROGMEM
inline uint8_t pgm_read_byte(const uint8_t *p) { return *p; }

#define D4 5
#define D5 6
#define D6 43
#define D7 44
#define D8 7
#define D9 8
#define D10 9
