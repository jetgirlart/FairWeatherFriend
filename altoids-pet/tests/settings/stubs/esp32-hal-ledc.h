#pragma once
#include <stdint.h>
bool ledcAttach(uint8_t pin, uint32_t frequency, uint8_t resolution);
uint32_t ledcWriteTone(uint8_t pin, uint32_t frequency);
bool ledcDetach(uint8_t pin);
