// Standalone spare-board test. Never include this in FairWeather Friend firmware.
// M100: VCC -> 3V3, GND -> GND, TX -> D7 (GPIO44), RX unconnected.
#include <Arduino.h>
#include <HardwareSerial.h>
#include "nmea_gga.h"

constexpr unsigned long GNSS_BAUD = 115200;
constexpr unsigned long FIX_TIMEOUT_MS = 20000;
HardwareSerial gnss(1);
gnss_test::Parser parser;
gnss_test::Fix lastFix;
bool haveGga = false, finished = false;
unsigned long acquisitionStarted = 0;

void report(bool accepted) {
  Serial.println(accepted ? "GPS FIX OK" : "GPS FIX FAILED");
  Serial.println(accepted ? "Fix: valid" : "Fix: invalid");
  if (haveGga && lastFix.coordinatesValid) {
    Serial.printf("Lat: %.6f\nLon: %.6f\n", lastFix.latitude, lastFix.longitude);
  } else {
    Serial.println("Lat: unavailable\nLon: unavailable");
  }
  if (haveGga && lastFix.satellitesAvailable) Serial.printf("Satellites: %u\n", lastFix.satellites);
  else Serial.println("Satellites: unavailable");
  if (haveGga && lastFix.hdopAvailable) Serial.printf("HDOP: %.2f\n", lastFix.hdop);
  else Serial.println("HDOP: unavailable");
  if (haveGga && lastFix.altitudeAvailable) Serial.printf("Altitude: %.1f m\n", lastFix.altitude);
  else Serial.println("Altitude: unavailable");
  Serial.flush();
  gnss.end(); finished = true;
}

void setup() {
  Serial.begin(115200);
  // A short bounded USB readiness window; it is outside the 20-second acquisition.
  unsigned long usbStarted = millis();
  while (!Serial && millis() - usbStarted < 2000) yield();
  gnss.setRxBufferSize(2048);
  // Custom RX prevents the core from selecting default UART pins. No UART TX.
  gnss.begin(GNSS_BAUD, SERIAL_8N1, D7, -1);
  Serial.println("Waiting for GNSS fix (20 s)...");
  acquisitionStarted = millis();
}

void loop() {
  if (finished) { yield(); return; }
  if (millis() - acquisitionStarted >= FIX_TIMEOUT_MS) { report(false); return; }
  // Bounded work per loop, no delays/readString/readBytes and no raw NMEA dump.
  for (unsigned budget = 0; budget < 512 && gnss.available() > 0; ++budget) {
    int byte = gnss.read();
    if (byte < 0) break;
    if (parser.feed(char(byte), lastFix)) {
      haveGga = true;
      if (lastFix.acceptable()) {
        report(millis() - acquisitionStarted < FIX_TIMEOUT_MS); return;
      }
    }
  }
  yield();
}
