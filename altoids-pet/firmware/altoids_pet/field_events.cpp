#include "field_events.h"
#include "journal.h"
#include <string.h>
#include <math.h>

FieldEventId mapNwsEvent(const char *name) {
  if (name) for (uint8_t i = 0; i < FIELD_EVENT_COUNT; ++i)
    if (strcmp(name, fieldEventName(FieldEventId(i))) == 0) return FieldEventId(i);
  return FieldEventId::COUNT;
}
uint64_t nwsAlertHash(const char *id) {
  if (!id || !*id || strlen(id) > 512) return 0;
  uint64_t hash = 14695981039346656037ULL;
  for (const unsigned char *p = reinterpret_cast<const unsigned char *>(id); *p; ++p)
    hash = (hash ^ *p) * 1099511628211ULL;
  return hash ? hash : 1;
}
bool plausibleNwsLocation(double lat, double lon) {
  if (!isfinite(lat) || !isfinite(lon)) return false;
  // Broad prefilter only, NOT a country-boundary test. /points confirms coverage.
  return (lat >= 17 && lat <= 72 && lon >= -180 && lon <= -64) ||
         (lat >= 50 && lat <= 72 && lon >= 170 && lon <= 180) ||
         (lat >= 13 && lat <= 21 && lon >= 144 && lon <= 146) ||
         (lat >= -15 && lat <= -10 && lon >= -172 && lon <= -168);
}
bool supportedNwsPoint(JsonVariantConst point) {
  const char *zone = point["properties"]["forecastZone"].as<const char *>();
  return zone && strncmp(zone, "https://api.weather.gov/zones/forecast/", strlen("https://api.weather.gov/zones/forecast/")) == 0 && strlen(zone) > strlen("https://api.weather.gov/zones/forecast/");
}
bool parseNwsAlerts(JsonVariantConst response, ActiveFieldAlert *alerts, size_t &count) {
  count = 0;
  JsonArrayConst features = response["features"].as<JsonArrayConst>();
  if (features.isNull() || features.size() > MAX_ACTIVE_FIELD_ALERTS) return false;
  Serial.printf("NWS alerts: %u active\n", unsigned(features.size()));
  for (JsonVariantConst feature : features) {
    const char *name = feature["properties"]["event"].as<const char *>();
    FieldEventId event = mapNwsEvent(name);
    if (event == FieldEventId::COUNT) { if (name) Serial.printf("NWS event ignored: %.48s\n", name); continue; }
    const char *id = feature["properties"]["id"].as<const char *>();
    if (!id || !*id) id = feature["id"].as<const char *>();
    uint64_t hash = nwsAlertHash(id);
    if (!hash) { Serial.println("NWS event skipped: missing stable alert ID."); continue; }
    alerts[count++] = {event, hash};
  }
  return true;
}
bool collectFieldAlerts(const ActiveFieldAlert *alerts, size_t count, int64_t observedAt) {
  if (!journalAvailable() || count > MAX_ACTIVE_FIELD_ALERTS || (count && !alerts) ||
      observedAt <= 0 || observedAt != getBuddySave().latestObservationAt) return false;
  BuddySaveData next = getBuddySave();
  uint32_t discoveries = 0; bool changed = false;
  // Pin every cached ID present in this snapshot before choosing replacement
  // slots. Response ordering cannot evict another currently active cached ID.
  bool pinned[RECENT_ALERT_COUNT] = {};
  for (uint8_t i = 0; i < RECENT_ALERT_COUNT; ++i)
    for (size_t j = 0; j < count; ++j) if (next.recentAlerts[i].hash && next.recentAlerts[i].hash == alerts[j].hash) {
      pinned[i] = true;
      if (next.recentAlerts[i].seenAt != observedAt) { next.recentAlerts[i].seenAt = observedAt; changed = true; }
    }
  for (size_t j = 0; j < count; ++j) {
    const auto &alert = alerts[j];
    if (uint8_t(alert.event) >= FIELD_EVENT_COUNT || !alert.hash) continue;
    bool known = false;
    for (const auto &cached : next.recentAlerts) if (cached.hash == alert.hash) { known = true; break; }
    if (known) { Serial.printf("Existing alert skipped: %08lx\n", (unsigned long)(alert.hash & 0xffffffffUL)); continue; }
    int slot = -1;
    for (uint8_t i = 0; i < RECENT_ALERT_COUNT; ++i) if (!pinned[i] &&
        (slot < 0 || next.recentAlerts[i].seenAt < next.recentAlerts[slot].seenAt)) slot = i;
    auto &record = next.fieldEvents[uint8_t(alert.event)];
    if (slot < 0 || record.count == UINT32_MAX) { Serial.println("NWS event skipped: bounded cache/counter full."); continue; }
    next.recentAlerts[slot] = {alert.hash, observedAt}; pinned[slot] = true;
    bool discovered = !record.count;
    if (discovered) { record.firstAt = observedAt; discoveries |= 1UL << uint8_t(alert.event); }
    ++record.count; record.latestAt = observedAt; changed = true;
    Serial.printf("%s: %s\n", discovered ? "New field event" : "Field event encounter", fieldEventName(alert.event));
  }
  // Future reward logic can query discoveredFieldEvent(); ordinary gear rules
  // remain unchanged. One checkpoint for the whole accepted alert snapshot.
  return !changed || commitFieldEvents(next, discoveries);
}
