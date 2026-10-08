#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <display_surface.h>

enum ScreenMode {
  HOME,
  MENU,
  WEATHER_SCREEN,
  TIMER_SETUP,
  FOCUS_SCREEN,
  TIMER_DONE,
  JOURNAL_SCREEN,
  RECORDS_SCREEN,
  GEAR_SCREEN
};

extern ScreenMode currentScreen;
extern const char* menuItems[];
extern const int menuCount;
extern int menuIndex;

void drawTime();
void drawHome();
void drawWeatherScreen();
void drawMenu();

void initializeDisplayBus();
void initializeDisplay();
void handleButtons(bool aPressed, bool bPressed, bool cPressed);

void drawTimerSetup(uint32_t minutes);
void drawFocusTimer(uint32_t seconds);
void drawTimerDone(uint32_t frame);

// Future menu destinations; existing WEATHER/TIMER slots and buttons stay put.
enum class BuddyMenuPage : uint8_t { JOURNAL, RECORDS, GEAR, TIMER, SETTINGS };
enum class BuddySetting : uint8_t {
  EXPORT_BUDDY, IMPORT_BUDDY, LOCATION, UNITS, SOUND, DISPLAY_OPTIONS, ABOUT
};
