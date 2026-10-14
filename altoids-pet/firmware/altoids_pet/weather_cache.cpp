#include "weather.h"
#include "journal.h"
#include <math.h>
#include <esp_attr.h>

ObservationLocation currentWeatherLocation;
RTC_DATA_ATTR uint8_t cachedObservationId = UNKNOWN_LOCATION;
RTC_DATA_ATTR uint8_t cachedActiveSite = UNKNOWN_LOCATION;
RTC_DATA_ATTR int32_t cachedSiteLatitude = 0, cachedSiteLongitude = 0;
RTC_DATA_ATTR int32_t cachedLatitudeMicrodegrees = 0;
RTC_DATA_ATTR int32_t cachedLongitudeMicrodegrees = 0;
RTC_DATA_ATTR bool cacheLocationKnown = false;
RTC_DATA_ATTR bool cachedWeatherValid = false;
RTC_DATA_ATTR bool cachedSunTimesValid = false;

RTC_DATA_ATTR int32_t cachedTemperatureMilliC = 0;
RTC_DATA_ATTR int cachedTemperatureF = 0;
RTC_DATA_ATTR int cachedWeatherCode = -1;

RTC_DATA_ATTR int cachedSunriseHour = 7;
RTC_DATA_ATTR int cachedSunriseMinute = 0;

RTC_DATA_ATTR int cachedSunsetHour = 19;
RTC_DATA_ATTR int cachedSunsetMinute = 0;

RTC_DATA_ATTR time_t lastOnlineSync = 0;

void invalidateWeatherLocation() {
  cachedWeatherValid = cachedSunTimesValid = weatherValid = sunTimesValid = false;
  cacheLocationKnown = false; lastOnlineSync = 0; currentWeatherLocation = {};
  weatherState = WEATHER_UNKNOWN;
  Serial.println("Location changed; weather/sun cache stale. Next normal sync uses saved coordinates.");
}
void saveCachedData() {

  if (!weatherValid) {
    return;
  }

  cachedObservationId = currentWeatherLocation.id;
  cachedActiveSite = getBuddySave().activeLocation;
  cachedSiteLatitude = lround(configuredLatitude() * 1000000);
  cachedSiteLongitude = lround(configuredLongitude() * 1000000);
  cachedLatitudeMicrodegrees = lround(currentWeatherLocation.latitude * 1000000);
  cachedLongitudeMicrodegrees = lround(currentWeatherLocation.longitude * 1000000);
  cacheLocationKnown = true;
  cachedWeatherValid =
    true;

  cachedSunTimesValid =
    sunTimesValid;

  cachedTemperatureMilliC = temperatureMilliC;
  cachedTemperatureF =
    temperatureF;

  cachedWeatherCode =
    weatherCode;

  cachedSunriseHour =
    sunriseHour;

  cachedSunriseMinute =
    sunriseMinute;

  cachedSunsetHour =
    sunsetHour;

  cachedSunsetMinute =
    sunsetMinute;

  lastOnlineSync =
    time(nullptr);

  Serial.println(
    "Weather cached."
  );
}

void restoreCachedData() {
  if (cachedWeatherValid && (!cacheLocationKnown ||
      cachedActiveSite != getBuddySave().activeLocation ||
      cachedSiteLatitude != lround(configuredLatitude() * 1000000) ||
      cachedSiteLongitude != lround(configuredLongitude() * 1000000))) {
    invalidateWeatherLocation();
  }

  if (!cachedWeatherValid) {
    return;
  }

  currentWeatherLocation = {cachedLatitudeMicrodegrees / 1000000.0, cachedLongitudeMicrodegrees / 1000000.0, cachedObservationId, locationSource(cachedObservationId)};
  temperatureMilliC = cachedTemperatureMilliC;
  temperatureF =
    cachedTemperatureF;

  weatherCode =
    cachedWeatherCode;

  weatherState =
    mapWeatherCode(
      weatherCode
    );

  sunriseHour =
    cachedSunriseHour;

  sunriseMinute =
    cachedSunriseMinute;

  sunsetHour =
    cachedSunsetHour;

  sunsetMinute =
    cachedSunsetMinute;

  weatherValid =
    true;

  sunTimesValid =
    cachedSunTimesValid;

  Serial.println(
    "Weather restored from RTC memory."
  );
}

