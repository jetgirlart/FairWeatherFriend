#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

enum ScreenMode {
  HOME,
  MENU,
  WEATHER_SCREEN,
  TIMER_SETUP,
  FOCUS_SCREEN,
  TIMER_DONE
};

extern Adafruit_SH1107 &display;
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
