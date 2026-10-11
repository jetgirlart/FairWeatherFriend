#include "background.h"
#include "display_surface.h"
#include "generated/background_assets.h"

namespace {
constexpr uint16_t rgb(unsigned r, unsigned g, unsigned b) {
  return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}
struct SceneColors { uint16_t sky, grass, outline, cloudLow, cloudMid, cloudHigh; };
// Background roles share source encoding with sprites, but have independent
// scene/material palettes. Foreground DETAIL is grass; cloud DETAIL is highlight.
const SceneColors sceneColors[] PROGMEM = {
  {rgb(122,184,200),rgb(74,117,60),0,rgb(133,157,165),rgb(193,210,207),rgb(239,238,222)},
  {rgb(25,37,62),rgb(35,64,48),0,rgb(43,57,77),rgb(72,85,105),rgb(105,119,136)},
  {rgb(134,154,157),rgb(69,99,62),0,rgb(114,133,140),rgb(162,180,180),rgb(205,213,204)},
  {rgb(32,43,57),rgb(31,52,43),0,rgb(42,53,68),rgb(68,81,96),rgb(100,115,126)},
  {rgb(89,123,141),rgb(49,88,66),0,rgb(68,91,110),rgb(112,137,152),rgb(157,178,184)},
  {rgb(22,35,52),rgb(29,49,43),0,rgb(28,42,59),rgb(54,71,91),rgb(83,102,119)},
  {rgb(53,66,87),rgb(38,67,48),0,rgb(43,50,66),rgb(77,85,105),rgb(116,126,143)},
  {rgb(17,25,42),rgb(25,42,35),0,rgb(22,30,45),rgb(44,53,72),rgb(72,86,105)},
  {rgb(162,183,191),rgb(132,160,151),0,rgb(119,142,157),rgb(179,198,207),rgb(235,240,235)},
  {rgb(39,55,78),rgb(69,90,91),0,rgb(42,59,83),rgb(79,97,120),rgb(133,151,172)},
  {rgb(158,172,161),rgb(109,128,100),0,rgb(129,146,139),rgb(181,193,180),rgb(215,221,203)},
  {rgb(54,66,69),rgb(47,64,55),0,rgb(51,66,69),rgb(85,100,98),rgb(125,139,129)},
  {rgb(191,203,223),rgb(111,142,114),0,rgb(144,156,177),rgb(192,204,224),rgb(241,244,255)}
};
SceneColors homeColors(const HomeScene &scene) {
  const auto &c = sceneColors[uint8_t(scene.palette)];
  return {pgm_read_word(&c.sky), pgm_read_word(&c.grass), pgm_read_word(&c.outline),
          pgm_read_word(&c.cloudLow), pgm_read_word(&c.cloudMid), pgm_read_word(&c.cloudHigh)};
}
void layer(const uint8_t *pixels, const uint16_t (&palette)[5]) {
  // Native source coordinates: no pet transform, scaling, trimming or extra canvas.
  for (int y = 0; y < 240; ++y) {
    for (int x = 0; x < 240; x += 2) {
      uint8_t packed = pgm_read_byte(pixels + (y * 240 + x) / 2);
      uint8_t left = packed >> 4, right = packed & 15;
      if (left) display.drawPixel(x, y, palette[left]);
      if (right) display.drawPixel(x + 1, y, palette[right]);
    }
  }
}
}
HomeScene selectHomeScene(WeatherState weather, bool daylight, unsigned frame) {
  HomeScene scene{daylight ? HomePalette::CLEAR_DAY : HomePalette::CLEAR_NIGHT,
                  HomeCloudLayer::NONE, !daylight, false};
  switch (weather) {
    case WEATHER_MAINLY_CLEAR:
    case WEATHER_PARTLY_CLOUDY: scene.clouds = HomeCloudLayer::LIGHT; break;
    case WEATHER_CLOUDY:
      scene.palette = daylight ? HomePalette::OVERCAST_DAY : HomePalette::OVERCAST_NIGHT;
      scene.clouds = HomeCloudLayer::HEAVY; scene.nightSky = false; break;
    case WEATHER_RAIN:
      scene.palette = daylight ? HomePalette::RAIN_DAY : HomePalette::RAIN_NIGHT;
      scene.clouds = HomeCloudLayer::HEAVY; scene.nightSky = false; break;
    case WEATHER_STORM:
      scene.palette = daylight ? HomePalette::STORM_DAY : HomePalette::STORM_NIGHT;
      scene.clouds = HomeCloudLayer::STORM; scene.nightSky = false;
      scene.lightning = frame % 24 < 2;
      if (scene.lightning) scene.palette = HomePalette::LIGHTNING;
      break;
    case WEATHER_SNOW:
      scene.palette = daylight ? HomePalette::SNOW_DAY : HomePalette::SNOW_NIGHT;
      scene.clouds = HomeCloudLayer::HEAVY; scene.nightSky = false; break;
    case WEATHER_FOG:
      scene.palette = daylight ? HomePalette::FOG_DAY : HomePalette::FOG_NIGHT;
      scene.nightSky = false; break;
    default: break;
  }
  return scene;
}
uint16_t homeSkyColor(const HomeScene &scene) { return homeColors(scene).sky; }
void drawHomeEnvironment(const HomeScene &scene) {
  const auto c = homeColors(scene);
  display.fillScreen(c.sky);
  const uint16_t palette[] = {0, c.outline, c.sky, c.cloudMid, c.grass};
  layer(BACKGROUND_ENVIRONMENT, palette);
}
void drawHomeClouds(const HomeScene &scene) {
  const auto c = homeColors(scene);
  const uint16_t palette[] = {0, c.outline, c.cloudLow, c.cloudMid, c.cloudHigh};
  switch (scene.clouds) {
    case HomeCloudLayer::LIGHT: layer(BACKGROUND_CLOUDS_LIGHT, palette); break;
    case HomeCloudLayer::HEAVY: layer(BACKGROUND_CLOUDS_HEAVY, palette); break;
    case HomeCloudLayer::STORM: layer(BACKGROUND_STORM_CLOUDS, palette); break;
    default: break;
  }
}
void drawHomeForeground(const HomeScene &scene) {
  const auto c = homeColors(scene);
  const uint16_t palette[] = {0, c.outline, c.grass, c.cloudMid, c.grass};
  layer(BACKGROUND_FOREGROUND, palette);
}
