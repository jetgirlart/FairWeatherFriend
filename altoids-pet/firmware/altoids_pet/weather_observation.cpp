#include "weather_observation.h"
#include <math.h>

bool parseLiveObservation(JsonVariantConst current, int64_t timestamp, WeatherObservation &out) {
  out = WeatherObservation{};
  auto temperature = current["temperature_2m"];
  if (!temperature.is<double>() || temperature.is<bool>() || !current["weather_code"].is<int32_t>()) return false;
  double value = temperature.as<double>();
  if (!isfinite(value) || value < -130 || value > 100) return false;
  out.timestamp = timestamp; out.temperatureMilliC = lround(value * 1000);
  out.weatherCode = current["weather_code"].as<int32_t>();
  out.category = observationCategoryForCode(out.weatherCode);
  const char *fields[] = {"relative_humidity_2m", "wind_speed_10m", "wind_gusts_10m", "surface_pressure", "precipitation"};
  for (uint8_t i = 0; i < METRIC_COUNT; ++i) {
    auto optional = current[fields[i]];
    if (!optional.is<double>() || optional.is<bool>()) continue;
    double number = optional.as<double>();
    // Bound before conversion to avoid overflow from a corrupt provider value.
    if (!isfinite(number) || number < 0 || number > 2000) continue;
    int32_t scaled = lround(number * 100);
    if (!validMetric(static_cast<MetricId>(i), scaled)) continue;
    out.metrics.validMask |= 1UL << i; out.metrics.values[i] = scaled;
  }
  return true;
}
void logLiveObservation(const WeatherObservation &observation) {
  Serial.printf("Observation:\nTemp: %.3f C\n", observation.temperatureMilliC / 1000.0);
  const char *labels[] = {"Humidity", "Wind", "Gust", "Pressure", "Precip"};
  const char *units[] = {"%", "km/h", "km/h", "hPa", "mm"};
  for (uint8_t i = 0; i < METRIC_COUNT; ++i) {
    if (observation.metrics.has(static_cast<MetricId>(i)))
      Serial.printf("%s: %.2f %s\n", labels[i], observation.metrics.values[i] / 100.0, units[i]);
    else Serial.printf("%s: unavailable\n", labels[i]);
  }
  Serial.printf("Weather: %s (code %ld)\n", weatherCategoryName(observation.category), (long)observation.weatherCode);
}
