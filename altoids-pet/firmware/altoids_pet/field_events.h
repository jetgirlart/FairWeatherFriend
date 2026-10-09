#pragma once
#include "save.h"
#include <ArduinoJson.h>

constexpr uint8_t MAX_ACTIVE_FIELD_ALERTS = 64;
struct ActiveFieldAlert { FieldEventId event; uint64_t hash; };
FieldEventId mapNwsEvent(const char *name);
uint64_t nwsAlertHash(const char *id);
bool plausibleNwsLocation(double latitude, double longitude);
bool supportedNwsPoint(JsonVariantConst point);
bool parseNwsAlerts(JsonVariantConst response, ActiveFieldAlert *alerts, size_t &count);
bool collectFieldAlerts(const ActiveFieldAlert *alerts, size_t count, int64_t observedAt);
// Called only by an accepted live weather fetch. Never from a cache or loop.
void checkLiveFieldEvents(double latitude, double longitude, int64_t observedAt);
