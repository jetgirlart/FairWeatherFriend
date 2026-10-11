#ifndef FWF_BACKGROUND_H
#define FWF_BACKGROUND_H
#include "weather.h"

// Rendering-only scene choice. No stored state, network requests or new scheduler.
enum class HomeCloudLayer : uint8_t { NONE, LIGHT, HEAVY, STORM };
enum class HomePalette : uint8_t {
  CLEAR_DAY, CLEAR_NIGHT, OVERCAST_DAY, OVERCAST_NIGHT,
  RAIN_DAY, RAIN_NIGHT, STORM_DAY, STORM_NIGHT,
  SNOW_DAY, SNOW_NIGHT, FOG_DAY, FOG_NIGHT, LIGHTNING
};
struct HomeScene {
  HomePalette palette;
  HomeCloudLayer clouds;
  bool nightSky;
  bool lightning;
};
HomeScene selectHomeScene(WeatherState weather, bool daylight, unsigned frame);
uint16_t homeSkyColor(const HomeScene &scene);
void drawHomeEnvironment(const HomeScene &scene);
void drawHomeClouds(const HomeScene &scene);
void drawHomeForeground(const HomeScene &scene);
#endif
