#include "Arduino.h"
#include <assert.h>
#include <limits.h>
unsigned long fakeClock=0;
UsbSerial Serial;
#include "../gnss_m100_test.ino"
std::string nmea(const char *body){unsigned sum=0;for(const char *p=body;*p;++p)sum^=unsigned(*p);char suffix[8];snprintf(suffix,sizeof(suffix),"*%02X\r\n",sum);return std::string("$")+body+suffix;}
void start(unsigned long now=0){fakeClock=now;Serial.output.clear();gnss.input.clear();gnss.ends=0;parser=gnss_test::Parser{};lastFix=gnss_test::Fix{};haveGga=finished=false;setup();}
int main(){
 start();assert(gnss.uart==1 && gnss.rx==44 && gnss.tx==-1 && gnss.baud==115200 && gnss.rxSize==2048);
 gnss.input=nmea("GNGGA,120000.00,3242.61098,N,09529.76630,W,1,10,1.03,162.4,M,0.0,M,,");loop();
 assert(finished && gnss.ends==1 && Serial.output.find("GPS FIX OK\nFix: valid")!=std::string::npos);
 assert(Serial.output.find("Lat: 32.710183\nLon: -95.496105")!=std::string::npos);
 assert(Serial.output.find("$GN")==std::string::npos);
 auto result=Serial.output;for(int i=0;i<100;++i){fakeClock+=1000;loop();}assert(Serial.output==result && gnss.ends==1);
 start();gnss.input=nmea("GNGGA,120000.00,3242.61098,N,09529.76630,W,1,03,3.01,,M,0.0,M,,");loop();assert(!finished);
 fakeClock=19999;loop();assert(!finished && Serial.output.find("GPS FIX FAILED")==std::string::npos);
 fakeClock=20000;loop();assert(finished && gnss.ends==1 && Serial.output.find("GPS FIX FAILED\nFix: invalid")!=std::string::npos);
 assert(Serial.output.find("Satellites: 3")!=std::string::npos && Serial.output.find("Altitude: unavailable")!=std::string::npos);
 result=Serial.output;loop();loop();assert(Serial.output==result);
 start(ULONG_MAX-10000);fakeClock=acquisitionStarted+19999;loop();assert(!finished);
 fakeClock=acquisitionStarted+20000;loop();assert(finished && Serial.output.find("Lat: unavailable")!=std::string::npos);
 puts("PASS: RX-only D7 UART, one-shot success/failure, exact 20-second deadline, last-fix diagnostics, no NMEA dump, timeout rollover.");
}
