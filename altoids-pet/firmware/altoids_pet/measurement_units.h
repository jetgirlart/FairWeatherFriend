#pragma once
#include <stdint.h>
inline int32_t roundedDivide(int64_t value, int64_t divisor) {
  return int32_t((value + (value < 0 ? -divisor / 2 : divisor / 2)) / divisor);
}
inline int32_t fahrenheitDeciToMilliC(int32_t value) { return roundedDivide((int64_t(value) - 320) * 500, 9); }
inline int32_t milliCToFahrenheitDeci(int32_t value) { return roundedDivide(int64_t(value) * 9 + 160000, 500); }
