#include "field_events.h"
#include "version.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

namespace {
const String NWS_USER_AGENT = String("FairWeatherFriend/") + FW_VERSION +
    " (https://github.com/jetgirlart/FairWeatherFriend)";
constexpr const char *NWS_ACCEPT = "application/geo+json";
constexpr int MAX_NWS_RESPONSE_BYTES = 98304;

class LimitedNwsStream : public Stream {
  Stream &source;
  size_t remaining = MAX_NWS_RESPONSE_BYTES;
public:
  explicit LimitedNwsStream(Stream &stream) : source(stream) { setTimeout(4000); }
  int available() override { return remaining ? source.available() : 0; }
  int read() override {
    if (!remaining) return -1;
    int value = source.read(); if (value >= 0) --remaining; return value;
  }
  int peek() override { return remaining ? source.peek() : -1; }
  void flush() override {}
  size_t write(uint8_t) override { return 0; }
};
bool requestNws(const String &url, JsonDocument &result, JsonDocument &filter) {
  WiFiClientSecure client;
  client.setInsecure(); // Same prototype TLS policy as the existing weather client.
  HTTPClient http;
  http.useHTTP10(true); // Direct stream parsing: avoid HTTP chunk framing.
  http.setConnectTimeout(3000); http.setTimeout(4000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(client, url)) return false;
  http.addHeader("User-Agent", NWS_USER_AGENT); http.addHeader("Accept", NWS_ACCEPT);
  int code = http.GET();
  if (code != 200 || http.getSize() > MAX_NWS_RESPONSE_BYTES) {
    Serial.printf("NWS supplemental check skipped: HTTP %d/response size.\n", code); http.end(); return false;
  }
  // Filter discards descriptions, geometry, instructions, etc. before storage.
  LimitedNwsStream stream(http.getStream());
  auto error = deserializeJson(result, stream, DeserializationOption::Filter(filter),
                               DeserializationOption::NestingLimit(12));
  http.end();
  if (error || result.overflowed()) { Serial.println("NWS supplemental check skipped: invalid response."); return false; }
  return true;
}
}
void checkLiveFieldEvents(double latitude, double longitude, int64_t observedAt) {
  if (!plausibleNwsLocation(latitude, longitude)) { Serial.println("NWS skipped: outside supported U.S. region."); return; }
  String point = String(latitude, 6) + "," + String(longitude, 6);
  {
    JsonDocument location, filter;
    filter["properties"]["forecastZone"] = true;
    if (!requestNws("https://api.weather.gov/points/" + point, location, filter)) return;
    if (!supportedNwsPoint(location.as<JsonVariantConst>())) { Serial.println("NWS skipped: no supported U.S. forecast zone."); return; }
  }
  JsonDocument response, filter;
  filter["features"][0]["id"] = true;
  filter["features"][0]["properties"]["id"] = true;
  filter["features"][0]["properties"]["event"] = true;
  if (!requestNws("https://api.weather.gov/alerts/active?point=" + point, response, filter)) return;
  ActiveFieldAlert alerts[MAX_ACTIVE_FIELD_ALERTS]; size_t count = 0;
  if (!parseNwsAlerts(response.as<JsonVariantConst>(), alerts, count)) {
    Serial.println("NWS supplemental check skipped: invalid/oversized active alert list."); return;
  }
  response.clear(); filter.clear(); // No API payload retained during NVS checkpoint.
  collectFieldAlerts(alerts, count, observedAt);
}
