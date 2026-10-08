#pragma once

#include <Arduino.h>
#include <time.h>

enum WeatherState {
  WEATHER_UNKNOWN,
  WEATHER_CLEAR,
  WEATHER_MAINLY_CLEAR,
  WEATHER_PARTLY_CLOUDY,
  WEATHER_CLOUDY,
  WEATHER_RAIN,
  WEATHER_STORM,
  WEATHER_SNOW,
  WEATHER_FOG
};

enum MoonPhase {
  MOON_NEW,
  MOON_WAXING_CRESCENT,
  MOON_FIRST_QUARTER,
  MOON_WAXING_GIBBOUS,
  MOON_FULL,
  MOON_WANING_GIBBOUS,
  MOON_LAST_QUARTER,
  MOON_WANING_CRESCENT
};

extern bool timeValid;
extern int currentHour;
extern int currentMinute;
extern int currentYear;
extern int currentMonth;
extern int currentDay;
extern int lastDisplayedMinute;
extern unsigned long lastClockCheck;

extern bool sunTimesValid;
extern int sunriseHour;
extern int sunriseMinute;
extern int sunsetHour;
extern int sunsetMinute;

extern WeatherState weatherState;
extern bool weatherValid;
extern int temperatureF;
extern int weatherCode;
extern MoonPhase currentMoonPhase;

// Defined with RTC_DATA_ATTR in weather.cpp; retained across deep sleep.
extern bool cachedWeatherValid;
extern time_t lastOnlineSync;

void configureTimezone();
bool isDaylight();
WeatherState mapWeatherCode(int code);
const char* weatherName();
MoonPhase calculateMoonPhase();
const char* moonPhaseName();
bool updateClock();
bool parseHourMinute(const char* text, int &hour, int &minute);
void saveCachedData();
void restoreCachedData();
bool fetchWeather();
bool syncOnlineData();

void initializeWeather(bool wokeFromButton);
void checkClock();

// Invalidate only on an explicit saved location change; next normal sync uses it.
void invalidateWeatherLocation();
double configuredLatitude();
double configuredLongitude();
