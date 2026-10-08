#pragma once
#include "journal.h"
#include <stdio.h>
// Permanent observations remain tenths Fahrenheit; conversion is display-only.
inline int32_t displayTemperatureDeci(int32_t deciF) {
  if (getBuddySave().units == UnitsId::US) return deciF;
  int64_t numerator = (int64_t(deciF) - 320) * 5;
  return int32_t((numerator + (numerator < 0 ? -4 : 4)) / 9);
}
inline void formatBuddyTemperature(int32_t deciF, char *text, size_t length, bool fractional = true) {
  int64_t value = displayTemperatureDeci(deciF);
  char unit = getBuddySave().units == UnitsId::METRIC ? 'C' : 'F';
  if (!fractional) {
    int64_t rounded = (value + (value < 0 ? -5 : 5)) / 10;
    snprintf(text, length, "%lld %c", (long long)rounded, unit); return;
  }
  int64_t magnitude = value < 0 ? -value : value;
  snprintf(text, length, "%s%lld.%lld %c", value < 0 ? "-" : "",
           (long long)(magnitude / 10), (long long)(magnitude % 10), unit);
}
