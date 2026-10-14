// Standalone spare-board test. Never include this in FairWeather Friend firmware.
// M100: VCC -> 3V3, GND -> GND, TX -> D7 (GPIO44), RX -> D5 (GPIO6).
#include <Arduino.h>
#include <HardwareSerial.h>
#include "nmea_gga.h"
#include "ubx_power.h"

constexpr unsigned long GNSS_BAUD = 115200;
constexpr unsigned long FIX_TIMEOUT_MS = 20000;
HardwareSerial gnss(1);
gnss_test::Parser parser;
gnss_test::Fix lastFix;
bool haveGga = false, finished = false;
unsigned long acquisitionStarted = 0;
enum class TestPhase : uint8_t { FIX, PROBE, STANDBY, RESUME_SETTLE, RESUME };
TestPhase phase = TestPhase::FIX;
ubx_test::Parser ubx;
unsigned long phaseStarted = 0, wakeStarted = 0;
constexpr unsigned long PROBE_MS = 2000, STANDBY_GRACE_MS = 1000;
constexpr unsigned long QUIET_WINDOW_MS = 3000, WAKE_SETTLE_MS = 1500, WAKE_TIMEOUT_MS = 5000;
static_assert(STANDBY_GRACE_MS + QUIET_WINDOW_MS + WAKE_TIMEOUT_MS < ubx_test::STANDBY_DURATION_MS, "Observe resume before timed auto-wake");
bool standbyAck = false, standbyNak = false, standbyQuiet = false, sawVersion = false;
unsigned standbyBytes = 0, standbyNmea = 0;

void finishTest() { gnss.end(); finished = true; }
void pollVersion() { gnss.write(ubx_test::MON_VER_POLL, sizeof(ubx_test::MON_VER_POLL)); gnss.flush(); }
void beginStandbyTest() {
  // Read-only firmware query. No configuration writes or flash/BBR changes.
  phase=TestPhase::PROBE;phaseStarted=millis();pollVersion();
}
void enterStandby() {
  Serial.println("Entering GNSS standby...");
  uint8_t packet[24];ubx_test::standbyPacket(packet);
  phase=TestPhase::STANDBY;phaseStarted=millis();ubx=ubx_test::Parser{};
  standbyAck=standbyNak=standbyQuiet=false;standbyBytes=standbyNmea=0;
  gnss.write(packet,sizeof(packet));gnss.flush();
}
void wakeGnss() {
  Serial.println("Waking GNSS...");
  // The configured UART RX edge is the documented wake signal, not a UBX ACK.
  // 0xFF provides a start-bit edge. Wait for startup before MON-VER polling.
  gnss.write(uint8_t(0xFF));gnss.flush();
  parser=gnss_test::Parser{};ubx=ubx_test::Parser{};
  phase=TestPhase::RESUME_SETTLE;phaseStarted=wakeStarted=millis();
}
void observeUbx(uint8_t c) {
  if(!ubx.feed(c))return;
  if(ubx.cls==0x0A && ubx.id==0x04 && ubx.length>=40) {
    if(!sawVersion){Serial.printf("Receiver MON-VER: SW %.30s / HW %.10s\n",ubx.payload,ubx.payload+30);sawVersion=true;}
  }
  if(phase==TestPhase::STANDBY && ubx.cls==0x05 && ubx.length==2 && ubx.payload[0]==0x02 && ubx.payload[1]==0x41) {
    if(ubx.id==1 && !standbyAck){standbyAck=true;Serial.println("GNSS standby acknowledged");}
    if(ubx.id==0 && !standbyNak){standbyNak=true;Serial.println("GNSS standby rejected (UBX-ACK-NAK)");}
  }
}


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
  if (!accepted) finishTest();
}

void setup() {
  Serial.begin(115200);
  // A short bounded USB readiness window; it is outside the 20-second acquisition.
  unsigned long usbStarted = millis();
  while (!Serial && millis() - usbStarted < 2000) yield();
  gnss.setRxBufferSize(2048);
  // GPS RX connects to D5 (UART TX); GPS TX remains on D7 (UART RX).
  gnss.begin(GNSS_BAUD, SERIAL_8N1, D7, D5);
  Serial.println("Waiting for GNSS fix (20 s)...");
  acquisitionStarted = millis();
}

void loop() {
  if (finished) { yield(); return; }
  unsigned long now=millis();
  if(phase==TestPhase::FIX && now-acquisitionStarted>=FIX_TIMEOUT_MS){report(false);return;}
  for(unsigned budget=0;budget<512 && gnss.available()>0;++budget){
    int byte=gnss.read();if(byte<0)break;
    if(phase==TestPhase::STANDBY && millis()-phaseStarted>=STANDBY_GRACE_MS)++standbyBytes;
    observeUbx(uint8_t(byte));
    if(parser.feed(char(byte),lastFix)){
      haveGga=true;
      if(phase==TestPhase::FIX && lastFix.acceptable()){
        if(millis()-acquisitionStarted>=FIX_TIMEOUT_MS){report(false);return;}
        report(true);beginStandbyTest();return;
      }
      if(phase==TestPhase::STANDBY && millis()-phaseStarted>=STANDBY_GRACE_MS)++standbyNmea;
      if(phase==TestPhase::RESUME){
        Serial.println(standbyQuiet?"GNSS awake":"GNSS awake (standby was not confirmed)");
        Serial.println("NMEA resumed (checksum-valid GGA)");
        Serial.println(lastFix.acceptable()?"Fix after wake: valid":"Fix after wake: invalid; receiver is awake but reacquiring");
        finishTest();return;
      }
    }
  }
  now=millis();
  if(phase==TestPhase::PROBE && now-phaseStarted>=PROBE_MS){
    if(!sawVersion)Serial.println("MON-VER unavailable; testing documented M10 PMREQ v0");
    enterStandby();
  }else if(phase==TestPhase::STANDBY && now-phaseStarted>=STANDBY_GRACE_MS+QUIET_WINDOW_MS){
    if(!standbyAck)Serial.println("No UBX standby ACK received (silence is not an ACK)");
    standbyQuiet=!standbyNak && standbyBytes==0;
    Serial.printf("Standby observation: %u RX bytes, %u GGA sentences in 3 s\n",standbyBytes,standbyNmea);
    Serial.println(standbyQuiet?"NMEA stopped; UART silence observed (power state not measured)":"Standby not confirmed: UART output continued or command rejected");
    wakeGnss();
  }else if(phase==TestPhase::RESUME_SETTLE && now-phaseStarted>=WAKE_SETTLE_MS){
    parser=gnss_test::Parser{};pollVersion();phase=TestPhase::RESUME;
  }
  if((phase==TestPhase::RESUME || phase==TestPhase::RESUME_SETTLE) && now-wakeStarted>=WAKE_TIMEOUT_MS){
    Serial.println("GNSS wake FAILED: no checksum-valid GGA at 115200 baud");
    Serial.println("Timed standby expires automatically; RAM settings/baud may have reset");
    finishTest();
  }
  yield();
}
