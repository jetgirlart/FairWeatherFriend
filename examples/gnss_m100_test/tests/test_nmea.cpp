#include "../nmea_gga.h"
#include <assert.h>
#include <stdio.h>
#include <string>
using namespace gnss_test;
std::string sentence(const std::string &body) {
  unsigned sum=0;for(char c:body)sum^=unsigned(c);char suffix[8];snprintf(suffix,sizeof(suffix),"*%02X\r\n",sum);
  return "$"+body+suffix;
}
bool feed(Parser &parser,const std::string &s,Fix &fix) {
  bool parsed=false;for(char c:s)if(parser.feed(c,fix))parsed=true;return parsed;
}
std::string gga(const char *lat="3242.61098",const char *ns="N",const char *lon="09529.76630",const char *ew="W",
                const char *quality="1",const char *sat="10",const char *hdop="1.03",const char *alt="162.4",const char *unit="M") {
  return sentence(std::string("GNGGA,120000.00,")+lat+","+ns+","+lon+","+ew+","+quality+","+sat+","+hdop+","+alt+","+unit+",0.0,M,,");
}
int main() {
  Parser p;Fix f;
  assert(feed(p,gga(),f) && f.acceptable());
  assert(fabs(f.latitude-32.710183)<0.000001 && fabs(f.longitude+95.496105)<0.000001);
  assert(f.satellites==10 && f.hdop==1.03 && f.altitude==162.4);
  for(const char *talker:{"GP","GN","GL","GA","GB","BD"}){
    auto s=gga();s=sentence(std::string(talker)+s.substr(3,s.find('*')-3));assert(feed(p,s,f) && f.acceptable());
  }
  for(const char *q:{"0","3","6","7","8","9","","-1","nan"})assert(feed(p,gga("3242.61098","N","09529.76630","W",q),f) && !f.acceptable());
  for(const char *q:{"1","2","4","5"})assert(feed(p,gga("3242.61098","N","09529.76630","W",q,"04","3.0"),f) && f.acceptable());
  for(const char *sat:{"0","3","","999","x"})assert(feed(p,gga("3242.61098","N","09529.76630","W","1",sat),f) && !f.acceptable());
  for(const char *h:{"3.01","0","-1","","nan","inf","1.2.3"})assert(feed(p,gga("3242.61098","N","09529.76630","W","1","10",h),f) && !f.acceptable());
  for(const char *lat:{"9060.0","9000.01","3260.0","9100.0","nan","","32.7"})assert(feed(p,gga(lat),f) && !f.acceptable());
  for(const char *lon:{"18100.0","18000.1","09560.0","-09500.0","","nan"})assert(feed(p,gga("3242.61098","N",lon),f) && !f.acceptable());
  assert(feed(p,gga("0000.0","N","00000.0","E"),f) && !f.acceptable());
  assert(feed(p,gga("0000.0","N","09500.0","W"),f) && f.acceptable());
  assert(feed(p,gga("9000.0","S","18000.0","W"),f) && f.acceptable() && f.latitude==-90 && f.longitude==-180);
  assert(feed(p,gga("3242.61098","X"),f) && !f.acceptable());
  assert(feed(p,gga("3242.61098","N","09529.76630","W","1","10","1.03","-15.2"),f) && f.acceptable() && f.altitude==-15.2);
  for(const char *alt:{"","nan","inf","broken"})assert(feed(p,gga("3242.61098","N","09529.76630","W","1","10","1.03",alt),f) && f.acceptable() && !f.altitudeAvailable);
  assert(feed(p,gga("3242.61098","N","09529.76630","W","1","10","1.03","162.4","F"),f) && !f.altitudeAvailable);
  auto good=gga();auto bad=good;bad[20]^=1;assert(!feed(p,bad,f));
  assert(!feed(p,"$GNGGA,missing checksum\r\n",f));
  assert(!feed(p,sentence("GNRMC,120000.00,A,3242.61098,N,09529.76630,W,0,0,131026,,,A"),f));
  assert(!feed(p,"$"+std::string(200,'A')+"\r\n",f));assert(feed(p,gga(),f) && f.acceptable());
  assert(feed(p,"garbage$partial"+gga(),f) && f.acceptable());
  puts("PASS: checksum/talkers, coordinates/hemispheres, quality, satellites, HDOP limits, optional altitude, malformed/noisy/overflow recovery.");
}
