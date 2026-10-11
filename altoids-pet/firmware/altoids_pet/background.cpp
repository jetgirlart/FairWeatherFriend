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
  {rgb(104,170,224),rgb(77,118,43),0,rgb(118,139,171),rgb(197,214,235),rgb(247,244,231)},
  {rgb(25,37,62),rgb(35,64,48),0,rgb(43,57,77),rgb(72,85,105),rgb(105,119,136)},
  {rgb(111,158,213),rgb(85,112,46),0,rgb(59,65,81),rgb(205,207,210),rgb(251,248,235)},
  {rgb(32,43,57),rgb(31,52,43),0,rgb(87,85,98),rgb(147,145,154),rgb(207,206,219)},
  {rgb(73,105,147),rgb(65,99,40),0,rgb(32,38,49),rgb(175,182,185),rgb(236,234,220)},
  {rgb(22,35,52),rgb(29,49,43),0,rgb(69,66,83),rgb(132,128,145),rgb(191,186,204)},
  {rgb(53,66,87),rgb(38,67,48),0,rgb(12,17,27),rgb(143,145,153),rgb(210,211,219)},
  {rgb(17,25,42),rgb(25,42,35),0,rgb(64,58,77),rgb(114,108,126),rgb(174,169,190)},
  {rgb(163,188,218),rgb(170,192,195),0,rgb(89,101,120),rgb(215,218,224),rgb(253,253,255)},
  {rgb(39,55,78),rgb(69,90,91),0,rgb(97,108,124),rgb(174,183,197),rgb(232,239,248)},
  {rgb(158,172,161),rgb(109,128,100),0,rgb(129,146,139),rgb(181,193,180),rgb(215,221,203)},
  {rgb(54,66,69),rgb(47,64,55),0,rgb(51,66,69),rgb(85,100,98),rgb(125,139,129)},
  {rgb(191,203,223),rgb(111,142,114),0,rgb(90,93,112),rgb(153,155,166),rgb(248,248,255)}
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

namespace {
uint16_t readableHomeInk(uint16_t background) {
  // A deliberately small retro text palette; no filled backing rectangles.
  unsigned red=((background >> 11) & 31)*255/31;
  unsigned green=((background >> 5) & 63)*255/63;
  unsigned blue=(background & 31)*255/31;
  unsigned brightness=(red*299+green*587+blue*114)/1000;
  return brightness >= 145 ? rgb(23,29,44) : COLOR_DETAIL;
}
}
uint16_t homeSkyInkColor(const HomeScene &scene) { return readableHomeInk(homeColors(scene).sky); }
uint16_t homeGroundInkColor(const HomeScene &scene) { return readableHomeInk(homeColors(scene).grass); }
