#define FWF_RICH_WEATHER_TEST
#include "../journal/test_journal.cpp"
void invalidateWeatherLocation(){invalidations++;}
#include "../../firmware/altoids_pet/field_events.cpp"
int main() {
 setenv("TZ","CST6CDT,M3.2.0/2,M11.1.0/2",1);tzset();blank();observe();
 for(uint8_t i=0;i<FIELD_EVENT_COUNT;++i)assert(mapNwsEvent(fieldEventName(FieldEventId(i)))==FieldEventId(i));
 assert(mapNwsEvent(nullptr)==FieldEventId::COUNT && mapNwsEvent("tornado warning")==FieldEventId::COUNT);
 assert(mapNwsEvent("Excessive Heat Warning")==FieldEventId::COUNT && mapNwsEvent("Dense Fog Advisory")==FieldEventId::COUNT);
 assert(!plausibleNwsLocation(51.5,-.1) && !plausibleNwsLocation(-33.86,151.2));
 JsonDocument doc;
 assert(!deserializeJson(doc,R"({"properties":{"forecastZone":"https://api.weather.gov/zones/forecast/TXZ001"}})"));
 assert(supportedNwsPoint(doc.as<JsonVariantConst>()));doc.clear();assert(!supportedNwsPoint(doc.as<JsonVariantConst>()));
 const char *fixture=R"({"features":[{"properties":{"event":"Tornado Watch","id":"urn:watch:1"}},{"id":"urn:warning:1","properties":{"event":"Tornado Warning"}},{"properties":{"event":"Flood Advisory","id":"urn:unknown"}},{"properties":{"event":"Ice Storm Warning"}}]})";
 assert(!deserializeJson(doc,fixture));ActiveFieldAlert alerts[MAX_ACTIVE_FIELD_ALERTS];size_t count;
 assert(parseNwsAlerts(doc.as<JsonVariantConst>(),alerts,count) && count==2);
 assert(alerts[0].event!=alerts[1].event && alerts[0].hash!=alerts[1].hash);
 assert(collectFieldAlerts(alerts,count,fakeEpoch));auto saved=getBuddySave();
 assert(saved.fieldEvents[0].count==1 && saved.fieldEvents[1].count==1 && saved.fieldEvents[2].count==0);
 assert(saved.fieldEvents[0].firstAt==fakeEpoch && saved.fieldEvents[0].latestAt==fakeEpoch);
 assert(discoveredFieldEvent(FieldEventId::TORNADO_WATCH) && !discoveredFieldEvent(FieldEventId::HURRICANE_WARNING));
 assert(takeNewFieldEvent()==FieldEventId::TORNADO_WATCH && takeNewFieldEvent()==FieldEventId::TORNADO_WARNING && takeNewFieldEvent()==FieldEventId::COUNT);
 // Duplicate in one response and across reboot only counts once. Cache remains NVS-backed.
 assert(collectFieldAlerts(alerts,count,fakeEpoch));initializeJournal();observe();assert(collectFieldAlerts(alerts,count,fakeEpoch));
 assert(getBuddySave().fieldEvents[0].count==1 && getBuddySave().fieldEvents[0].latestAt==saved.fieldEvents[0].latestAt);
 alerts[0].hash=nwsAlertHash("urn:watch:2");assert(collectFieldAlerts(alerts,count,fakeEpoch));
 assert(getBuddySave().fieldEvents[0].count==2 && getBuddySave().fieldEvents[0].firstAt==saved.fieldEvents[0].firstAt && getBuddySave().fieldEvents[0].latestAt==fakeEpoch);
 assert(takeNewFieldEvent()==FieldEventId::COUNT);
 auto json=exported();BuddySaveData restored;const char *error;
 assert(deserializeBuddySave(json.c_str(),json.size(),restored,error));assert(buddySaveChecksum(restored)==buddySaveChecksum(getBuddySave()));
 auto old=getBuddySave();old.saveVersion=5;
 uint8_t payload[V5_PAYLOAD_BYTES];encodeCurrent(old,payload);std::vector<uint8_t> envelope(HEADER_BYTES+V5_PAYLOAD_BYTES);
 uint8_t *header=envelope.data();put32(header,SAVE_MAGIC);put32(header,5);put32(header,V5_PAYLOAD_BYTES);put64(header,90);
 memcpy(envelope.data()+HEADER_BYTES,payload,V5_PAYLOAD_BYTES);header=envelope.data()+20;
 put32(header,hashBytes(payload,V5_PAYLOAD_BYTES,hashBytes(envelope.data(),20)));
 storage.clear();storage["fwf-buddy"]["save0"]=envelope;initializeJournal();
 assert(v5Checksum(getBuddySave())==v5Checksum(old));
 assert(dirty && getBuddySave().fieldEvents[0].count==0 && getBuddySave().recentAlerts[0].hash==0);
 assert(getBuddySave().totalObservations==old.totalObservations && getBuddySave().furPalette==old.furPalette && getBuddySave().highestTemperatureAt==old.highestTemperatureAt);
 checkpointJournal(true);initializeJournal();assert(!dirty);
 JsonDocument backup;assert(!deserializeJson(backup,json));backup["saveVersion"]=5;backup.remove("fieldEvents");backup.remove("recentAlerts");
 char hash[9];snprintf(hash,9,"%08lx",(unsigned long)v5Checksum(old));backup["checksum"]=hash;
 std::string oldJson;serializeJson(backup,oldJson);assert(deserializeBuddySave(oldJson.c_str(),oldJson.size(),restored,error));assert(restored.fieldEvents[0].count==0);
 for(int scenario=0;scenario<7;++scenario){
  JsonDocument bad;assert(!deserializeJson(bad,json));
  if(scenario==0)bad["fieldEvents"][0]["discovered"]=false;
  if(scenario==1)bad["fieldEvents"][0]["firstAt"]=old.latestObservationAt+1;
  if(scenario==2)bad["fieldEvents"][0]["name"]="Tornado Warning";
  if(scenario==3)bad["fieldEvents"][1]["id"]=0;
  if(scenario==4)bad["fieldEvents"][0]["count"]=-1;
  if(scenario==5)bad["recentAlerts"][0][0]="ffffffffffffffffg";
  if(scenario==6)bad["saveVersion"]=SAVE_VERSION+1;
  std::string invalid;serializeJson(bad,invalid);assert(!deserializeBuddySave(invalid.c_str(),invalid.size(),restored,error));
 }
 // Fill the bounded cache, keep current entries pinned even with new IDs first.
 blank();observe();ActiveFieldAlert many[64];
 for(int i=0;i<32;++i)many[i]={FieldEventId::TORNADO_WATCH,uint64_t(i+1)};
 assert(collectFieldAlerts(many,32,fakeEpoch));assert(getBuddySave().fieldEvents[0].count==32);
 observe();for(int i=0;i<32;++i)many[32+i]=many[i];for(int i=0;i<32;++i)many[i].hash=100+i;
 assert(collectFieldAlerts(many,64,fakeEpoch));assert(getBuddySave().fieldEvents[0].count==32);
 // All supported events and full fingerprints survive JSON without changing slots.
 auto fullest=getBuddySave();
 for(auto &event:fullest.fieldEvents)event={UINT32_MAX,fullest.createdAt,fullest.latestObservationAt};
 Print p;assert(serializeBuddySave(fullest,p));
 printf("Full event export: %zu bytes (limit %zu)\n",p.output.size(),BUDDY_IMPORT_BYTES);
 assert(deserializeBuddySave(p.output.c_str(),p.output.size(),restored,error));assert(buddySaveChecksum(fullest)==buddySaveChecksum(restored));
 puts("PASS: exact NWS mappings, independent watches/warnings, unsupported regions, IDs/fallbacks, reboot dedup, first/latest encounters, bounded pinned cache, v5 NVS/JSON migrations and validation.");
}
