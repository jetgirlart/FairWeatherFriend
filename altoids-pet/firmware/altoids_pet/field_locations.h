#pragma once
#include "save.h"
#include <string.h>

inline bool validLocationName(const char *name) {
  if (!name) return false;
  size_t length = strnlen(name, LOCATION_NAME_BYTES);
  if (!length || length >= LOCATION_NAME_BYTES || name[0] == ' ' || name[length - 1] == ' ') return false;
  for (size_t i = 0; i < length; ++i) if (name[i] < 32 || name[i] > 126) return false;
  return true;
}
inline bool locationExists(const BuddySaveData &s, uint8_t id) {
  return id < FIELD_LOCATION_COUNT && s.locations[id].used;
}
enum LocationSource : uint8_t { LOCATION_SOURCE_NONE, LOCATION_SOURCE_FIELD_SITE, LOCATION_SOURCE_GNSS };
inline LocationSource locationSource(uint8_t id) {
  return id == GNSS_LOCATION ? LOCATION_SOURCE_GNSS : id < FIELD_LOCATION_COUNT ? LOCATION_SOURCE_FIELD_SITE : LOCATION_SOURCE_NONE;
}
struct ObservationLocation {
  double latitude = 0, longitude = 0;
  uint8_t id = UNKNOWN_LOCATION;
  LocationSource source = LOCATION_SOURCE_NONE;
};
inline ObservationLocation savedObservationLocation(const BuddySaveData &s) {
  if (!locationExists(s, s.activeLocation)) return {};
  const auto &site = s.locations[s.activeLocation];
  double latitude = site.latitudeMicrodegrees / 1000000.0, longitude = site.longitudeMicrodegrees / 1000000.0;
  if ((latitude == 0 && longitude == 0) || latitude < -90 || latitude > 90 || longitude < -180 || longitude > 180) return {};
  return {latitude, longitude, s.activeLocation, LOCATION_SOURCE_FIELD_SITE};
}
inline const char *fieldLocationName(const BuddySaveData &s, uint8_t id) {
  if (id == GNSS_LOCATION) return "GNSS";
  return locationExists(s, id) ? s.locations[id].name : "UNKNOWN";
}
inline void initializeHomeLocation(BuddySaveData &s, double latitude, double longitude) {
  if (s.activeLocation != UNKNOWN_LOCATION) return;
  auto &home = s.locations[0]; home.used = true;
  strcpy(home.name, "HOME");
  home.latitudeMicrodegrees = int32_t(latitude * 1000000.0 + (latitude >= 0 ? .5 : -.5));
  home.longitudeMicrodegrees = int32_t(longitude * 1000000.0 + (longitude >= 0 ? .5 : -.5));
  s.activeLocation = 0;
}
