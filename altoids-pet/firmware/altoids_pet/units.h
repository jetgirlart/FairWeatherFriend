#pragma once
#include "journal.h"
#include "measurement_units.h"
#include <stdio.h>
inline int32_t displayTemperatureDeci(int32_t milliC) {
  return getBuddySave().units == UnitsId::US ? milliCToFahrenheitDeci(milliC) : roundedDivide(milliC, 100);
}
inline void formatFixed(int32_t value, unsigned places, const char *unit, char *text, size_t length) {
  int64_t magnitude = value < 0 ? -int64_t(value) : value;
  int32_t scale = places == 2 ? 100 : places == 1 ? 10 : 1;
  if (places) snprintf(text, length, "%s%lld.%0*lld %s", value < 0 ? "-" : "", (long long)(magnitude / scale),
                       int(places), (long long)(magnitude % scale), unit);
  else snprintf(text, length, "%ld %s", (long)value, unit);
}
inline void formatBuddyTemperature(int32_t milliC, char *text, size_t length, bool fractional = true) {
  int32_t value = displayTemperatureDeci(milliC);
  // Round whole-degree displays directly, avoiding a second rounding step.
  int32_t whole = getBuddySave().units == UnitsId::US ? roundedDivide(int64_t(milliC) * 9 + 160000, 5000) : roundedDivide(milliC, 1000);
  formatFixed(fractional ? value : whole, fractional ? 1 : 0,
              getBuddySave().units == UnitsId::US ? "F" : "C", text, length);
}
inline void formatBuddyMetric(MetricId id, int32_t value, char *text, size_t length) {
  bool us = getBuddySave().units == UnitsId::US;
  const char *unit = "%"; unsigned places = 1; int32_t displayed = roundedDivide(value, 10);
  switch (id) {
    case MetricId::HUMIDITY: break;
    case MetricId::WIND: case MetricId::GUST:
      unit = us ? "mph" : "km/h"; displayed = us ? roundedDivide(int64_t(value) * 100000, 1609344) : roundedDivide(value, 10); break;
    case MetricId::PRESSURE:
      unit = us ? "inHg" : "hPa"; places = us ? 2 : 1;
      displayed = us ? roundedDivide(int64_t(value) * 100000, 3386389) : roundedDivide(value, 10); break;
    case MetricId::PRECIPITATION:
      unit = us ? "in" : "mm"; places = 2;
      displayed = us ? roundedDivide(int64_t(value) * 10, 254) : value; break;
    default: snprintf(text, length, "UNAVAILABLE"); return;
  }
  formatFixed(displayed, places, unit, text, length);
}
