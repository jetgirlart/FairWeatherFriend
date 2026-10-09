#pragma once
#include "journal.h"
#include <ArduinoJson.h>
// Required temperature/code; missing/null/invalid optional fields remain unset.
bool parseLiveObservation(JsonVariantConst current, int64_t timestamp, WeatherObservation &out);
void logLiveObservation(const WeatherObservation &observation);
