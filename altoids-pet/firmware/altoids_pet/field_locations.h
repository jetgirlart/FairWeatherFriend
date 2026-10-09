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
inline const char *fieldLocationName(const BuddySaveData &s, uint8_t id) {
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
