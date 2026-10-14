#include "gnss.h"
#include "gnss_nmea.h"
#include "gnss_ubx.h"
#include "journal.h"
#include <HardwareSerial.h>
#include <driver/gpio.h>
#ifdef ARDUINO
#include "hardware.h"
static_assert(D7 == GNSS_RX_GPIO && D5 == GNSS_TX_GPIO && TFT_RST == D4, "Preserve GNSS D7/D5 and TFT reset D4");
#endif
namespace {
HardwareSerial gnssUart(1);
gnss_test::Parser gnssParser;
ObservationLocation selected;
bool locationReady = false;
unsigned long locationStarted = 0;
unsigned lastRemaining = 999;
unsigned long feedbackStarted = 0;
gnss_ubx::Parser standbyReply;
bool receiverAwakeReported = false;
bool standbyPending = false, standbyAckSeen = false, standbyNakSeen = false;
void holdGnssTx() {
  // Capture the UART's idle-HIGH output before end() disconnects its pin.
  gpio_hold_en(static_cast<gpio_num_t>(GNSS_TX_GPIO));
}
void requestGnssStandby() {
  uint8_t packet[24];gnss_ubx::standbyPacket(packet);
  standbyReply = gnss_ubx::Parser{};standbyAckSeen = standbyNakSeen = false;
  gnssUart.write(packet,sizeof(packet));gnssUart.flush();
  standbyPending = true;
  Serial.println("Entering GNSS standby...");
}
void updateGnssStandby() {
  for(unsigned budget=0;budget<512 && gnssUart.available();++budget) {
    if(!standbyReply.feed(uint8_t(gnssUart.read())))continue;
    if(standbyReply.cls!=5 || standbyReply.length!=2 || standbyReply.payload[0]!=2 || standbyReply.payload[1]!=0x41)continue;
    if(standbyReply.id==1 && !standbyAckSeen){standbyAckSeen=true;Serial.println("GNSS standby acknowledged");}
    if(standbyReply.id==0 && !standbyNakSeen){standbyNakSeen=true;Serial.println("GNSS standby rejected; module may remain awake");}
  }
  if(millis()-feedbackStarted>=GNSS_STANDBY_CHECK_MS) {
    if(!standbyAckSeen && !standbyNakSeen)Serial.println("GNSS standby requested; no UBX ACK received (power state unverified)");
    gnssUart.flush();holdGnssTx();gnssUart.end();standbyPending=false;
  }
}
}
void beginObservationLocation() {
  selected = {}; locationReady = false; gnssParser = gnss_test::Parser{};
  gpio_hold_dis(static_cast<gpio_num_t>(GNSS_TX_GPIO));
  standbyPending = false; receiverAwakeReported = false;
  gnssUart.setRxBufferSize(2048);
  gnssUart.begin(GNSS_BAUD, SERIAL_8N1, GNSS_RX_GPIO, GNSS_TX_GPIO);
  Serial.println("Waking GNSS...");
  gnssUart.write(uint8_t(0xFF));gnssUart.flush();
  locationStarted = millis(); lastRemaining = 999;
  Serial.println("GNSS: acquiring...");
}
ObservationLocation liveObservationLocation() {
  return locationReady ? selected : savedObservationLocation(getBuddySave());
}
bool updateObservationLocation() {
  if (locationReady) {
    if(standbyPending)updateGnssStandby();
    return !standbyPending && millis()-feedbackStarted>=GNSS_RESULT_DISPLAY_MS;
  }
  unsigned long elapsed = millis() - locationStarted;
  if (elapsed < GNSS_TIMEOUT_MS) {
    unsigned remaining = (GNSS_TIMEOUT_MS - elapsed + 999) / 1000;
    if (remaining != lastRemaining) { lastRemaining = remaining; drawLocationAcquisitionStatus("GETTING LOCATION...", remaining); }
    if(elapsed < GNSS_WAKE_SETTLE_MS)return false;
    for (unsigned budget = 0; budget < 512 && gnssUart.available(); ++budget) {
      gnss_test::Fix fix;
      bool parsed = gnssParser.feed(char(gnssUart.read()), fix);
      if(parsed && !receiverAwakeReported) { receiverAwakeReported = true; Serial.println("GNSS awake"); }
      if (parsed && fix.acceptable(GNSS_MIN_SATELLITES, GNSS_MAX_HDOP) && millis() - locationStarted < GNSS_TIMEOUT_MS) {
        selected = {fix.latitude, fix.longitude, GNSS_LOCATION, LOCATION_SOURCE_GNSS};
        locationReady = true; feedbackStarted = millis();
        Serial.printf("GNSS FIX OK\nLat: %.6f\nLon: %.6f\nSatellites: %u\nHDOP: %.2f\nLocation source: GNSS\n", fix.latitude, fix.longitude, fix.satellites, fix.hdop);
        drawLocationAcquisitionStatus("LOCATION FOUND", 0); requestGnssStandby(); return false;
      }
    }
    return false;
  }
  locationReady = true; feedbackStarted = millis(); selected = savedObservationLocation(getBuddySave());
  Serial.println("GNSS FIX FAILED");
  if (selected.source != LOCATION_SOURCE_NONE) {
    const char *name = fieldLocationName(getBuddySave(), selected.id);
    Serial.printf("Falling back to field site: %s\nLocation source: FIELD SITE (%s)\n", name, name);
    drawLocationAcquisitionStatus("USING SAVED SITE", 0);
  } else {
    Serial.println("Location source: NONE; live observation skipped.");
    drawLocationAcquisitionStatus("LOCATION NOT SET", 0);
  }
  requestGnssStandby();
  return false;
}

void prepareGnssDeepSleep() {
  // D5 is already held idle HIGH after any completed live session. Cached boots
  // leave that hold untouched. This enables its retention through deep sleep.
  gpio_deep_sleep_hold_en();
}
