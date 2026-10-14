#include "gnss.h"
#include "gnss_nmea.h"
#include "journal.h"
#include <HardwareSerial.h>
#ifdef ARDUINO
#include "hardware.h"
static_assert(D7 == GNSS_RX_GPIO, "GNSS must use D7");
#endif
namespace {
HardwareSerial gnssUart(1);
gnss_test::Parser gnssParser;
ObservationLocation selected;
bool locationReady = false;
unsigned long locationStarted = 0;
unsigned lastRemaining = 999;
unsigned long feedbackStarted = 0;
}
void beginObservationLocation() {
  selected = {}; locationReady = false; gnssParser = gnss_test::Parser{};
  gnssUart.setRxBufferSize(2048);
  gnssUart.begin(GNSS_BAUD, SERIAL_8N1, GNSS_RX_GPIO, -1);
  locationStarted = millis(); lastRemaining = 999;
  Serial.println("GNSS: acquiring...");
}
ObservationLocation liveObservationLocation() {
  return locationReady ? selected : savedObservationLocation(getBuddySave());
}
bool updateObservationLocation() {
  if (locationReady) return millis() - feedbackStarted >= GNSS_RESULT_DISPLAY_MS;
  unsigned long elapsed = millis() - locationStarted;
  if (elapsed < GNSS_TIMEOUT_MS) {
    unsigned remaining = (GNSS_TIMEOUT_MS - elapsed + 999) / 1000;
    if (remaining != lastRemaining) { lastRemaining = remaining; drawLocationAcquisitionStatus("GETTING LOCATION...", remaining); }
    for (unsigned budget = 0; budget < 512 && gnssUart.available(); ++budget) {
      gnss_test::Fix fix;
      if (gnssParser.feed(char(gnssUart.read()), fix) && fix.acceptable(GNSS_MIN_SATELLITES, GNSS_MAX_HDOP) && millis() - locationStarted < GNSS_TIMEOUT_MS) {
        selected = {fix.latitude, fix.longitude, GNSS_LOCATION, LOCATION_SOURCE_GNSS};
        gnssUart.end(); locationReady = true; feedbackStarted = millis();
        Serial.printf("GNSS FIX OK\nLat: %.6f\nLon: %.6f\nSatellites: %u\nHDOP: %.2f\nLocation source: GNSS\n", fix.latitude, fix.longitude, fix.satellites, fix.hdop);
        drawLocationAcquisitionStatus("LOCATION FOUND", 0); return false;
      }
    }
    return false;
  }
  gnssUart.end(); locationReady = true; feedbackStarted = millis(); selected = savedObservationLocation(getBuddySave());
  Serial.println("GNSS FIX FAILED");
  if (selected.source != LOCATION_SOURCE_NONE) {
    const char *name = fieldLocationName(getBuddySave(), selected.id);
    Serial.printf("Falling back to field site: %s\nLocation source: FIELD SITE (%s)\n", name, name);
    drawLocationAcquisitionStatus("USING SAVED SITE", 0);
  } else {
    Serial.println("Location source: NONE; live observation skipped.");
    drawLocationAcquisitionStatus("LOCATION NOT SET", 0);
  }
  return false;
}
