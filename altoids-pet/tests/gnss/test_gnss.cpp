#define FWF_GNSS_INTEGRATION_TEST
#include "../field_events/test_network.cpp"
std::string gga(const char *lat="3242.61752",const char *lon="09529.78460",const char *sat="06",const char *hdop="2.26",const char *quality="1") {
 std::string body=std::string("GNGGA,120000.0,")+lat+",N,"+lon+",W,"+quality+","+sat+","+hdop+",185.0,M,0,M,,";
 unsigned hash=0;for(char c:body)hash^=unsigned(c);char tail[8];snprintf(tail,sizeof(tail),"*%02X\r\n",hash);return "$"+body+tail;
}
std::string standbyResponse(bool ack) {
 std::string body;for(uint8_t b:{uint8_t(5),uint8_t(ack?1:0),uint8_t(2),uint8_t(0),uint8_t(2),uint8_t(0x41)})body+=char(b);
 uint8_t a=0,b=0;for(unsigned char value:body){a+=value;b+=a;}
 return std::string("\xB5\x62",2)+body+char(a)+char(b);
}
void acquire(const std::string &sentence) {
 gnssOutput.clear();gnssInput=sentence;beginObservationLocation();unsigned long started=millis();
 while(!updateObservationLocation()){yield();assert(millis()-started<=GNSS_TIMEOUT_MS+GNSS_STANDBY_CHECK_MS+1);}
}
int main(){
 setenv("TZ","CST6CDT,M3.2.0/2,M11.1.0/2",1);tzset();
 prepare();auto savedSites=getBuddySave();acquire(gga());auto position=liveObservationLocation();
 assert(position.source==LOCATION_SOURCE_GNSS && position.id==GNSS_LOCATION);
 assert(gnssOutput.size()==25 && uint8_t(gnssOutput[0])==0xFF && gnssTxHeld);
 gnss_ubx::Parser powerPacket;bool packetValid=false;
 for(size_t i=1;i<gnssOutput.size();++i)if(powerPacket.feed(uint8_t(gnssOutput[i])))packetValid=true;
 assert(packetValid && powerPacket.cls==2 && powerPacket.id==0x41 && powerPacket.length==16);
 for(unsigned i=4;i<8;++i)assert(powerPacket.payload[i]==0); // indefinite, not the test's 10-second timer.
 assert(powerPacket.payload[8]==6 && powerPacket.payload[12]==8);
 assert(fabs(position.latitude-32.710292)<0.000001 && fabs(position.longitude+95.496410)<0.000001);
 httpReplies={{200,weatherReply},{200,pointReply},{200,alertReply}};assert(fetchWeather());
 assert(httpUrls[0].find("latitude=32.710292&longitude=-95.496410")!=std::string::npos);
 assert(httpUrls.back()=="https://api.weather.gov/alerts/active?point=32.710292,-95.496410");
 const auto &s=getBuddySave();assert(s.latestLocation==GNSS_LOCATION && s.highestTemperatureLocation==GNSS_LOCATION && fieldNoteAt(s,0)->location==GNSS_LOCATION);
 assert(s.fieldSitesVisited==0 && s.activeLocation==savedSites.activeLocation);
 for(unsigned i=0;i<FIELD_LOCATION_COUNT;++i){assert(!memcmp(&s.locations[i],&savedSites.locations[i],sizeof(FieldLocation)));}
 assert(!strcmp(fieldLocationName(s,GNSS_LOCATION),"GNSS"));
 auto observed=buddySaveChecksum(s);initializeJournal();assert(buddySaveChecksum(getBuddySave())==observed);
 Print json;assert(serializeBuddySave(getBuddySave(),json));BuddySaveData restored;const char *error=nullptr;
 assert(deserializeBuddySave(json.output.c_str(),json.output.size(),restored,error) && restored.latestLocation==GNSS_LOCATION);
 saveCachedData();unsigned starts=gnssStarts;auto txBefore=gnssOutput;auto releases=gnssReleaseCalls;currentWeatherLocation={};
 for(int i=0;i<100;++i)initializeWeather(true);
 assert(gnssOutput==txBefore && gnssReleaseCalls==releases && gnssTxHeld);
 prepareGnssDeepSleep();assert(gnssTxHeld && gnssOutput==txBefore && gnssDeepHolds>0);
 assert(gnssStarts==starts && currentWeatherLocation.source==LOCATION_SOURCE_GNSS && currentWeatherLocation.id==GNSS_LOCATION);
 assert(httpUrls.size()==3 && buddySaveChecksum(getBuddySave())==observed);
 // GNSS history cannot become a reused saved-site name.
 assert(upsertFieldLocation(1,"OTHER",33,-96));assert(deleteFieldLocation(0,1));assert(upsertFieldLocation(0,"REUSED",34,-97));
 assert(getBuddySave().latestLocation==GNSS_LOCATION && !strcmp(fieldLocationName(getBuddySave(),fieldNoteAt(getBuddySave(),0)->location),"GNSS"));
 // Failing each quality gate times out and selects the active site unchanged.
 for(auto bad:{gga("3242.6","09529.7","03"),gga("3242.6","09529.7","06","3.01"),gga("0000.0","00000.0"),gga("3242.6","09529.7","06","2.0","0"),std::string()}) {
  prepare();auto before=buddySaveChecksum(getBuddySave());acquire(bad);position=liveObservationLocation();
  assert(position.source==LOCATION_SOURCE_FIELD_SITE && position.id==0 && position.latitude==32 && position.longitude==-95);
  assert(buddySaveChecksum(getBuddySave())==before);
 }
 prepare();assert(saveBuddyLocation(0,0));acquire("");assert(liveObservationLocation().source==LOCATION_SOURCE_NONE);
 auto before=getBuddySave().totalObservations;httpReplies={{200,weatherReply}};assert(!fetchWeather() && httpUrls.empty() && getBuddySave().totalObservations==before);
 WiFi.mode(WIFI_OFF);gnssInput.clear();assert(!syncOnlineData());
 assert(httpUrls.empty() && WiFi.currentMode==WIFI_OFF && getBuddySave().totalObservations==before);
 // Actual cold/live coordinator: GNSS precedes weather/NWS, cached wake skips it.
 prepare();gnssInput=gga();httpReplies={{200,weatherReply},{200,pointReply},{200,alertReply}};assert(syncOnlineData());
 assert(currentWeatherLocation.source==LOCATION_SOURCE_GNSS && getBuddySave().latestLocation==GNSS_LOCATION);
 // Standby ACK is accepted only when actually received for PMREQ.
 prepare();gnssInput=gga();beginObservationLocation();fakeMillis=locationStarted+GNSS_WAKE_SETTLE_MS;
 assert(!updateObservationLocation() && standbyPending);
 gnssInput=standbyResponse(true);
 Serial.output.clear();updateObservationLocation();assert(standbyAckSeen && Serial.output.find("GNSS standby acknowledged")!=std::string::npos);
 fakeMillis=feedbackStarted+GNSS_STANDBY_CHECK_MS;assert(updateObservationLocation() && gnssTxHeld);
 prepare();gnssInput=gga();beginObservationLocation();fakeMillis=locationStarted+GNSS_WAKE_SETTLE_MS;updateObservationLocation();
 gnssInput=standbyResponse(false);
 updateObservationLocation();assert(standbyNakSeen && !standbyAckSeen);
 fakeMillis=feedbackStarted+GNSS_STANDBY_CHECK_MS;assert(updateObservationLocation());
 // No ACK plus silence is observational evidence, never a fabricated ACK.
 prepare();Serial.output.clear();acquire(gga());
 assert(Serial.output.find("0 RX bytes, 0 GGA sentences in 3 s")!=std::string::npos);
 assert(Serial.output.find("GNSS output stopped; standby inferred")!=std::string::npos);
 assert(Serial.output.find("GNSS standby acknowledged")==std::string::npos);
 // Settling traffic is excluded; continuing data during observation is detected.
 prepare();gnssInput=gga();beginObservationLocation();fakeMillis=locationStarted+GNSS_WAKE_SETTLE_MS;updateObservationLocation();
 gnssInput=gga();fakeMillis=feedbackStarted+GNSS_STANDBY_GRACE_MS-1;updateObservationLocation();
 assert(standbyRxBytes==0 && standbyGgaSentences==0);
 gnssInput=gga();fakeMillis=feedbackStarted+GNSS_STANDBY_GRACE_MS+1;updateObservationLocation();
 assert(standbyRxBytes>0 && standbyGgaSentences==1);
 Serial.output.clear();fakeMillis=feedbackStarted+GNSS_STANDBY_CHECK_MS;assert(updateObservationLocation());
 assert(Serial.output.find("receiver output continues")!=std::string::npos);
 assert(Serial.output.find("GNSS output stopped")==std::string::npos);
 // A missing wire/data stream must not be reported as confirmed standby.
 prepare();Serial.output.clear();acquire("");assert(Serial.output.find("no pre-command data")!=std::string::npos);
 // Migration from the current shipping version 10 keeps every permanent field.
 prepare();httpReplies={{200,weatherReply},{200,pointReply},{200,alertReply}};assert(fetchWeather());
 auto old=getBuddySave();std::vector<uint8_t> bytes(RECORD_BYTES);uint8_t *p=bytes.data();
 put32(p,SAVE_MAGIC);put32(p,10);put32(p,PAYLOAD_BYTES);put64(p,99);p+=4;
 encodeCurrent(old,bytes.data()+HEADER_BYTES);p=bytes.data()+HEADER_BYTES;put32(p,10);
 encodeEvents(old,bytes.data()+HEADER_BYTES+V5_PAYLOAD_BYTES);encodeLocations(old,bytes.data()+HEADER_BYTES+V6_PAYLOAD_BYTES);
 encodeVariants(old,bytes.data()+HEADER_BYTES+V7_PAYLOAD_BYTES);encodeNotes(old,bytes.data()+HEADER_BYTES+V8_PAYLOAD_BYTES);encodeAchievements(old,bytes.data()+HEADER_BYTES+V9_PAYLOAD_BYTES);
 p=bytes.data()+20;put32(p,hashBytes(bytes.data()+HEADER_BYTES,PAYLOAD_BYTES,hashBytes(bytes.data(),20)));
 storage.clear();storage["fwf-buddy"]["save1"]=bytes;initializeJournal();assert(journalAvailable() && getBuddySave().saveVersion==11 && buddySaveChecksum(getBuddySave())==buddySaveChecksum(old));
 JsonDocument doc;Print exportOld;assert(serializeBuddySave(old,exportOld));assert(!deserializeJson(doc,exportOld.output));
 doc["saveVersion"]=10;char checksum[9];snprintf(checksum,sizeof(checksum),"%08lx",(unsigned long)v10Checksum(old));doc["checksum"]=checksum;
 std::string compact;serializeJson(doc,compact);assert(deserializeBuddySave(compact.c_str(),compact.size(),restored,error));
 assert(restored.saveVersion==11 && buddySaveChecksum(restored)==buddySaveChecksum(old));
 puts("PASS: GNSS gates/timeouts, missing fallback, shared weather/NWS coordinates, cached wakes without UART, no site writes/visits, GNSS note/record NVS+JSON, reused slots, v10 binary/JSON migration.");
}
