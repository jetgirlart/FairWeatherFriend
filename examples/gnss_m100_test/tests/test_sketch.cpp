#include "Arduino.h"
#include <assert.h>
#include <limits.h>
unsigned long fakeClock=0;
UsbSerial Serial;
#include "../gnss_m100_test.ino"
std::string nmea(const char *body){unsigned sum=0;for(const char *p=body;*p;++p)sum^=unsigned(*p);char suffix[8];snprintf(suffix,sizeof(suffix),"*%02X\r\n",sum);return std::string("$")+body+suffix;}
std::string frame(unsigned cls,unsigned id,const std::string &payload){
 std::string body;body+=char(cls);body+=char(id);body+=char(payload.size());body+=char(payload.size()>>8);body+=payload;
 uint8_t a=0,b=0;for(unsigned char c:body){a+=c;b+=a;}return std::string("\xB5\x62",2)+body+char(a)+char(b);
}
const char *fix="GNGGA,120000.00,3242.61098,N,09529.76630,W,1,10,1.03,162.4,M,0.0,M,,";
void start(unsigned long now=0){fakeClock=now;Serial.output.clear();gnss.input.clear();gnss.output.clear();gnss.ends=0;parser=gnss_test::Parser{};ubx=ubx_test::Parser{};lastFix=gnss_test::Fix{};haveGga=finished=false;sawVersion=standbyAck=standbyNak=standbyQuiet=false;phase=TestPhase::FIX;setup();}
void toStandby(){gnss.input=nmea(fix);loop();assert(phase==TestPhase::PROBE && !finished);fakeClock+=PROBE_MS;loop();assert(phase==TestPhase::STANDBY);}
void wakePhase(){fakeClock=phaseStarted+STANDBY_GRACE_MS+QUIET_WINDOW_MS;loop();assert(phase==TestPhase::RESUME_SETTLE);fakeClock+=WAKE_SETTLE_MS;loop();assert(phase==TestPhase::RESUME);}
int main(){
 start();assert(gnss.uart==1 && gnss.rx==44 && gnss.tx==6 && gnss.baud==115200 && gnss.rxSize==2048);
 gnss.input=nmea(fix);loop();assert(!finished && phase==TestPhase::PROBE && gnss.ends==0);
 assert(Serial.output.find("GPS FIX OK\nFix: valid")!=std::string::npos && Serial.output.find("$GN")==std::string::npos);
 assert(gnss.output==std::string((const char*)ubx_test::MON_VER_POLL,8));
 std::string version(40,'\0');version.replace(0,8,"SPG 5.10");gnss.input=frame(0x0A,4,version);loop();assert(sawVersion);
 fakeClock=PROBE_MS;loop();uint8_t packet[24];ubx_test::standbyPacket(packet);
 assert(gnss.output.substr(8)==std::string((const char*)packet,24));
 assert(packet[6]==0 && packet[10]==0x10 && packet[11]==0x27 && packet[14]==6 && packet[18]==8);
 ubx_test::Parser verifier;bool parsed=false;for(uint8_t c:packet)if(verifier.feed(c))parsed=true;
 assert(parsed && verifier.cls==2 && verifier.id==0x41 && verifier.length==16);
 gnss.input=frame(5,1,std::string("\x02\x41",2));loop();assert(standbyAck);
 wakePhase();assert(standbyQuiet && Serial.output.find("GNSS standby acknowledged")!=std::string::npos);
 assert(gnss.output[32]==char(0xFF));gnss.input=nmea(fix);loop();assert(finished && gnss.ends==1);
 assert(Serial.output.find("GNSS awake\n")!=std::string::npos && Serial.output.find("NMEA resumed")!=std::string::npos);
 auto report=Serial.output;for(int i=0;i<100;++i){fakeClock+=1000;loop();}assert(Serial.output==report);
 // No ACK must never produce an acknowledgment message, even when output stops.
 start();toStandby();wakePhase();assert(!standbyAck && standbyQuiet && Serial.output.find("No UBX standby ACK received")!=std::string::npos);
 assert(Serial.output.find("GNSS standby acknowledged")==std::string::npos);
 fakeClock=wakeStarted+WAKE_TIMEOUT_MS;loop();assert(finished && Serial.output.find("GNSS wake FAILED")!=std::string::npos);
 // Incorrect checksum, wrong command ACK, NAK, and continuing NMEA are distinct.
 start();toStandby();auto bad=frame(5,1,std::string("\x02\x41",2));bad.back()^=1;
 gnss.input=bad+frame(5,1,std::string("\x06\x01",2));loop();assert(!standbyAck);
 gnss.input=frame(5,0,std::string("\x02\x41",2));loop();assert(standbyNak);
 fakeClock=phaseStarted+STANDBY_GRACE_MS+1;gnss.input=nmea(fix);loop();assert(standbyBytes>0 && standbyNmea==1);
 wakePhase();assert(!standbyQuiet);gnss.input=nmea(fix);loop();assert(finished && Serial.output.find("standby was not confirmed")!=std::string::npos);
 // Fix failure never sends a standby command; the 20-second gate is unchanged.
 start();fakeClock=20000;loop();assert(finished && gnss.output.empty() && Serial.output.find("GPS FIX FAILED")!=std::string::npos);
 start(ULONG_MAX-10000);fakeClock=acquisitionStarted+19999;loop();assert(!finished);
 fakeClock=acquisitionStarted+20000;loop();assert(finished);
 puts("PASS: D7 RX/D5 TX, unchanged fix gate, PMREQ bytes/checksum, MON-VER, genuine ACK/NAK filtering, silence vs ACK, UART wake/GGA resume, failure/rollover/one-shot output.");
}
