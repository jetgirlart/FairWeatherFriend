#pragma once
#include "../../journal/stubs/Arduino.h"
#define PROGMEM
inline uint8_t pgm_read_byte(const uint8_t *p) { return *p; }
