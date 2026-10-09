#include "field_events.h"
#include "field_locations.h"
#include "weather.h"
#include "journal.h"
#include "weather_observation.h"
#include "measurement_units.h"
#include <config.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>
#include <esp_attr.h>

// ==================================================
// CLOCK
// ==================================================

bool timeValid = false;

int currentHour = 0;
int currentMinute = 0;

int currentYear = 0;
int currentMonth = 0;
int currentDay = 0;

int lastDisplayedMinute = -1;

unsigned long lastClockCheck = 0;

// ==================================================
// SUNRISE / SUNSET
// ==================================================

bool sunTimesValid = false;

int sunriseHour = 7;
int sunriseMinute = 0;

int sunsetHour = 19;
int sunsetMinute = 0;

// ==================================================
// WEATHER
// ==================================================

WeatherState weatherState = WEATHER_UNKNOWN;

bool weatherValid = false;

int temperatureF = 0;
int32_t temperatureMilliC = 0;
int weatherCode = -1;

// ==================================================
// MOON
// ==================================================

MoonPhase currentMoonPhase = MOON_NEW;

// ==================================================
// RTC MEMORY
// Survives ESP32 deep sleep.
// ==================================================

// ==================================================
// TIMEZONE
// ==================================================

void configureTimezone() {
  setenv("TZ", TIMEZONE, 1);
  tzset();
}

// ==================================================
// WEATHER MAPPING
// ==================================================

WeatherState mapWeatherCode(int code) {

  if (code == 0)
    return WEATHER_CLEAR;

  if (code == 1)
    return WEATHER_MAINLY_CLEAR;

  if (code == 2)
    return WEATHER_PARTLY_CLOUDY;

  if (code == 3)
    return WEATHER_CLOUDY;

  if (code == 45 || code == 48)
    return WEATHER_FOG;

  if (code >= 51 && code <= 67)
    return WEATHER_RAIN;

  if (code >= 71 && code <= 77)
    return WEATHER_SNOW;

  if (code >= 80 && code <= 82)
    return WEATHER_RAIN;

  if (code == 85 || code == 86)
    return WEATHER_SNOW;

  if (code >= 95)
    return WEATHER_STORM;

  return WEATHER_UNKNOWN;
}

const char* weatherName() {

  bool daytime = isDaylight();

  switch (weatherState) {

    case WEATHER_CLEAR:
      return daytime ? "SUNNY" : "STARRY";

    case WEATHER_MAINLY_CLEAR:
      return daytime ? "FAIR" : "CLEAR";

    case WEATHER_PARTLY_CLOUDY:
      return "PART CLOUD";

    case WEATHER_CLOUDY:
      return "CLOUDY";

    case WEATHER_RAIN:
      return "RAIN";

    case WEATHER_STORM:
      return "STORM";

    case WEATHER_SNOW:
      return "SNOW";

    case WEATHER_FOG:
      return "FOG";

    default:
      return "UNKNOWN";
  }
}

// ==================================================
// MOON PHASE
// ==================================================

MoonPhase calculateMoonPhase() {

  if (!timeValid) {
    return MOON_NEW;
  }

  // Known new moon near Jan 6, 2000.
  const time_t knownNewMoon = 947182440;

  const double lunarCycle =
    29.530588853;

  time_t now =
    time(nullptr);

  double daysSince =
    difftime(now, knownNewMoon) /
    86400.0;

  double moonAge =
    fmod(daysSince, lunarCycle);

  if (moonAge < 0) {
    moonAge += lunarCycle;
  }

  int phase =
    (int)floor(
      (moonAge / lunarCycle) *
      8.0 +
      0.5
    ) % 8;

  return (MoonPhase)phase;
}

const char* moonPhaseName() {

  switch (currentMoonPhase) {

    case MOON_NEW:
      return "NEW MOON";

    case MOON_WAXING_CRESCENT:
      return "WAX CRES";

    case MOON_FIRST_QUARTER:
      return "FIRST QTR";

    case MOON_WAXING_GIBBOUS:
      return "WAX GIB";

    case MOON_FULL:
      return "FULL MOON";

    case MOON_WANING_GIBBOUS:
      return "WANE GIB";

    case MOON_LAST_QUARTER:
      return "LAST QTR";

    case MOON_WANING_CRESCENT:
      return "WANE CRES";
  }

  return "";
}

// ==================================================
// CLOCK UPDATE
// ==================================================

bool updateClock() {

  struct tm timeInfo;

  if (
    !getLocalTime(
      &timeInfo,
      1000
    )
  ) {

    return false;
  }

  currentHour =
    timeInfo.tm_hour;

  currentMinute =
    timeInfo.tm_min;

  currentYear =
    timeInfo.tm_year + 1900;

  currentMonth =
    timeInfo.tm_mon + 1;

  currentDay =
    timeInfo.tm_mday;

  timeValid =
    true;

  currentMoonPhase =
    calculateMoonPhase();

  return true;
}

// ==================================================
// OPEN-METEO TIME PARSER
// ==================================================

bool parseHourMinute(
  const char* text,
  int &hour,
  int &minute
) {

  if (!text) {
    return false;
  }

  const char* timePart =
    strchr(
      text,
      'T'
    );

  if (!timePart) {
    return false;
  }

  timePart++;

  if (
    sscanf(
      timePart,
      "%d:%d",
      &hour,
      &minute
    ) == 2
  ) {

    return true;
  }

  return false;
}

// ==================================================
// CACHE WEATHER
// ==================================================

double configuredLatitude() {
  const auto &s = getBuddySave(); if (locationExists(s, s.activeLocation)) return s.locations[s.activeLocation].latitudeMicrodegrees / 1000000.0; return s.locationConfigured ? s.latitudeMicrodegrees / 1000000.0 : LATITUDE;
}
double configuredLongitude() {
  const auto &s = getBuddySave(); if (locationExists(s, s.activeLocation)) return s.locations[s.activeLocation].longitudeMicrodegrees / 1000000.0; return s.locationConfigured ? s.longitudeMicrodegrees / 1000000.0 : LONGITUDE;
}
// ==================================================
// FETCH WEATHER
// ==================================================

bool fetchWeather() {

  Serial.println(
    "Fetching weather..."
  );

  WiFiClientSecure client;

  // Prototype only.
  client.setInsecure();

  HTTPClient http;

  String url =
    "https://api.open-meteo.com/v1/forecast"
    "?latitude=" +
    String(configuredLatitude(), 6) +
    "&longitude=" +
    String(configuredLongitude(), 6) +
    "&current=temperature_2m,relative_humidity_2m,wind_speed_10m,wind_gusts_10m,surface_pressure,precipitation,weather_code"
    "&daily=sunrise,sunset"
    "&temperature_unit=celsius&wind_speed_unit=kmh&precipitation_unit=mm"
    "&timezone=auto"
    "&forecast_days=1";

  if (
    !http.begin(
      client,
      url
    )
  ) {

    Serial.println(
      "HTTP setup failed."
    );

    return false;
  }

  int httpCode =
    http.GET();

  if (
    httpCode != 200
  ) {

    Serial.print(
      "Weather HTTP error: "
    );

    Serial.println(
      httpCode
    );

    http.end();

    return false;
  }

  String payload =
    http.getString();

  http.end();

  JsonDocument doc;

  DeserializationError error =
    deserializeJson(
      doc,
      payload
    );

  if (error) {

    Serial.print(
      "JSON error: "
    );

    Serial.println(
      error.c_str()
    );

    return false;
  }

  WeatherObservation observation;
  if (!parseLiveObservation(doc["current"], static_cast<int64_t>(time(nullptr)), observation)) {
    Serial.println("Live weather rejected: invalid required temperature/code.");
    return false;
  }
  observation.location = getBuddySave().activeLocation;
  weatherCode = observation.weatherCode;
  temperatureMilliC = observation.temperatureMilliC;
  temperatureF = round(observation.temperatureMilliC * 9.0 / 5000.0 + 32);
  logLiveObservation(observation);

  weatherState =
    mapWeatherCode(
      weatherCode
    );

  weatherValid =
    true;

  const char* sunriseText =
    doc["daily"]
       ["sunrise"][0];

  const char* sunsetText =
    doc["daily"]
       ["sunset"][0];

  bool sunriseOK =
    parseHourMinute(
      sunriseText,
      sunriseHour,
      sunriseMinute
    );

  bool sunsetOK =
    parseHourMinute(
      sunsetText,
      sunsetHour,
      sunsetMinute
    );

  sunTimesValid =
    sunriseOK &&
    sunsetOK;

  if (
    sunTimesValid
  ) {

    Serial.printf(
      "Sunrise: %02d:%02d\n",
      sunriseHour,
      sunriseMinute
    );

    Serial.printf(
      "Sunset: %02d:%02d\n",
      sunsetHour,
      sunsetMinute
    );
  }

  // Only this successful LIVE fetch can create journal progress. RTC restore
  // and failed fetches never call it. Keep existing fetch/cache/UI semantics.
  if (timeValid) {
    if (recordWeatherObservation(observation)) {
      doc.clear(); payload = String(); // Release normal response before supplemental fetch.
      checkLiveFieldEvents(configuredLatitude(), configuredLongitude(), observation.timestamp);
    }
  }
  else Serial.println("Live observation skipped: invalid clock.");

  return true;
}

// ==================================================
// ONLINE SYNC
// ==================================================

bool syncOnlineData() {

  Serial.println();

  Serial.println(
    "Starting Wi-Fi..."
  );

  WiFi.mode(
    WIFI_STA
  );

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  unsigned long start =
    millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - start < 20000
  ) {

    Serial.print(
      "."
    );

    delay(
      500
    );
  }

  Serial.println();

  if (
    WiFi.status() !=
    WL_CONNECTED
  ) {

    Serial.println(
      "Wi-Fi FAILED."
    );

    WiFi.disconnect(
      true
    );

    WiFi.mode(
      WIFI_OFF
    );

    return false;
  }

  Serial.println(
    "Wi-Fi connected."
  );

  Serial.print(
    "RSSI: "
  );

  Serial.print(
    WiFi.RSSI()
  );

  Serial.println(
    " dBm"
  );

  // ----------------------------------------------
  // NTP TIME
  // ----------------------------------------------

  // NTP maintains the UTC epoch. configTime() also overwrites TZ, so
  // restore the Central POSIX rule afterward, before reading local time.
  configTime(
    0,
    0,
    "pool.ntp.org",
    "time.google.com",
    "time.nist.gov"
  );

  configureTimezone();

  Serial.println(
    "Getting time..."
  );

  struct tm timeInfo;

  int attempts = 0;

  while (
    !getLocalTime(
      &timeInfo,
      1000
    ) &&
    attempts < 15
  ) {

    attempts++;

    Serial.print(
      "."
    );
  }

  Serial.println();

  if (
    attempts < 15
  ) {

    updateClock();

    Serial.printf(
      "Local time: %02d:%02d\n",
      currentHour,
      currentMinute
    );

    Serial.printf(
      "Date: %04d-%02d-%02d\n",
      currentYear,
      currentMonth,
      currentDay
    );

    Serial.print(
      "Moon: "
    );

    Serial.println(
      moonPhaseName()
    );

  } else {

    Serial.println(
      "Time sync failed."
    );
  }

  // ----------------------------------------------
  // WEATHER
  // ----------------------------------------------

  bool weatherOK =
    fetchWeather();

  if (
    weatherOK
  ) {

    saveCachedData();
  }

  // ----------------------------------------------
  // WIFI OFF
  // ----------------------------------------------

  WiFi.disconnect(
    true
  );

  WiFi.mode(
    WIFI_OFF
  );

  Serial.println(
    "Wi-Fi off."
  );

  return true;
}

bool isDaylight() {
  if (!timeValid) {
    return true;
  }

  if (!sunTimesValid) {
    return
      currentHour >= 7 &&
      currentHour < 19;
  }

  int nowMinutes =
    currentHour * 60 +
    currentMinute;

  int sunriseMinutes =
    sunriseHour * 60 +
    sunriseMinute;

  int sunsetMinutes =
    sunsetHour * 60 +
    sunsetMinute;

  return
    nowMinutes >= sunriseMinutes &&
    nowMinutes < sunsetMinutes;
}

void initializeWeather(bool wokeFromButton) {
  // ----------------------------------------------
  // COLD BOOT
  // ----------------------------------------------

  if (
    !wokeFromButton
  ) {

    Serial.println(
      "Cold boot."
    );

    syncOnlineData();
  }

  // ----------------------------------------------
  // DEEP SLEEP WAKE
  // ----------------------------------------------

  else {

    Serial.println(
      "Woke from button."
    );

    // Timezone has already been restored above.

    if (
      updateClock()
    ) {

      Serial.println(
        "Clock restored."
      );

      Serial.printf(
        "Local time: %02d:%02d\n",
        currentHour,
        currentMinute
      );

    } else {

      Serial.println(
        "Clock unavailable."
      );
    }

    // Restore last weather immediately.

    restoreCachedData();

    time_t now =
      time(nullptr);

    bool weatherStale =
      false;

    if (
      !cachedWeatherValid
    ) {

      weatherStale =
        true;

    } else if (
      lastOnlineSync > 0 &&
      now > lastOnlineSync &&
      (
        now -
        lastOnlineSync
      ) >
      WEATHER_REFRESH_SECONDS
    ) {

      weatherStale =
        true;
    }

    if (
      weatherStale
    ) {

      Serial.println(
        "Weather data stale. Refreshing..."
      );

      syncOnlineData();

    } else {

      Serial.println(
        "Using cached weather."
      );

      Serial.println(
        "Wi-Fi stays off."
      );
    }
  }
}

void checkClock() {
  // ----------------------------------------------
  // CLOCK CHECK
  //
  // Check every 5 seconds. The home animation scheduler renders changes
  // in its next complete frame, without a separate minute-change redraw.
  // ----------------------------------------------

  if (
    timeValid &&
    millis() -
      lastClockCheck >
      5000
  ) {

    lastClockCheck =
      millis();

    if (
      updateClock()
    ) {

      if (
        currentMinute !=
        lastDisplayedMinute
      ) {

        lastDisplayedMinute =
          currentMinute;

        // The existing 250 ms home animation frame draws the new clock.
        // Avoid a second full OLED transfer at every minute boundary.
      }
    }
  }
}
