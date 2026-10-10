#define FWF_RICH_WEATHER_TEST
#include "../journal/test_journal.cpp"
void invalidateWeatherLocation(){invalidations++;}
#include "../../firmware/altoids_pet/field_events.cpp"
int main(){
 setenv("TZ","CST6CDT,M3.2.0/2,M11.1.0/2",1);tzset();blank();
 assert(getBuddySave().fieldNoteCount==0 && !fieldNoteAt(getBuddySave(),0));
 observe();auto first=*fieldNoteAt(getBuddySave(),0);
 assert(first.timestamp==fakeEpoch && first.location==0 && first.metrics.validMask==0);
 assert(first.outcomes==(NEW_WEATHER_DISCOVERY|NEW_HIGH_TEMP|NEW_LOW_TEMP|NEW_GEAR_UNLOCK));
 unsigned writes=nvsWrites;
 assert(!recordWeatherObservation({fakeEpoch+59,20000,0,WeatherCategory::CLEAR}));
 assert(!recordWeatherObservation({fakeEpoch+60,20000,61,WeatherCategory::CLEAR}));
 assert(getBuddySave().fieldNoteCount==1 && nvsWrites==writes);
 // Cached-style boots and updates do not create history or flash writes.
 for(int i=0;i<100;i++){initializeJournal();updateJournal();}
 assert(getBuddySave().fieldNoteCount==1 && nvsWrites==writes);
 for(int i=0;i<20;i++)observe(61,700+i);
 auto s=getBuddySave();assert(s.fieldNoteCount==16 && s.fieldNoteNext==5);
 for(int age=0;age<16;age++)assert(fieldNoteAt(s,age)->timestamp==fakeEpoch-age*3600);
 assert(!fieldNoteAt(s,16));assert(nvsWrites==writes+20);
 WeatherObservation rich={fakeEpoch+3600,30000,61,WeatherCategory::RAIN};
 rich.metrics.validMask=31;int32_t values[]={8100,2200,4400,101300,200};
 for(int i=0;i<5;i++)rich.metrics.values[i]=values[i];fakeEpoch=rich.timestamp;
 assert(recordWeatherObservation(rich));auto note=*fieldNoteAt(getBuddySave(),0);
 assert(note.metrics.validMask==31 && note.outcomes==(NEW_HIGH_TEMP|NEW_WIND_RECORD|NEW_GUST_RECORD|NEW_HUMIDITY_RECORD|NEW_PRESSURE_RECORD|NEW_PRECIP_RECORD));
 ActiveFieldAlert alert={FieldEventId::TORNADO_WARNING,123456};writes=nvsWrites;
 assert(collectFieldAlerts(&alert,1,fakeEpoch));assert(nvsWrites==writes+1 && getBuddySave().fieldNoteCount==16);
 note=*fieldNoteAt(getBuddySave(),0);assert(note.severeEvents==(1UL<<uint8_t(FieldEventId::TORNADO_WARNING)));
 assert((note.outcomes & (NEW_SEVERE_EVENT|NEW_GEAR_VARIANT))==(NEW_SEVERE_EVENT|NEW_GEAR_VARIANT));
 // Newly discovered alert only, never a new history entry or a duplicate discovery flag.
 observe(61,700);assert(collectFieldAlerts(&alert,1,fakeEpoch));assert(!fieldNoteAt(getBuddySave(),0)->severeEvents);
 s=getBuddySave();auto json=exported();BuddySaveData restored;const char *error;
 assert(deserializeBuddySave(json.c_str(),json.size(),restored,error));assert(buddySaveChecksum(restored)==buddySaveChecksum(s));
 printf("FieldNote RAM=%zu; binary=%zu; payload=%zu; checkpoint=%zu; pretty JSON=%zu\n",sizeof(FieldNote),FIELD_NOTE_BYTES,PAYLOAD_BYTES,RECORD_BYTES,json.size());
 auto empty=s;empty.fieldNoteCount=empty.fieldNoteNext=0;for(auto &n:empty.fieldNotes)n=FieldNote{};
 Print emptyJSON;assert(serializeBuddySave(empty,emptyJSON));
 printf("Sixteen-note JSON increase: %zu bytes\n",json.size()-emptyJSON.output.size());
 // Fully populated optional metrics, all configured sites/events and large counters.
 for(int i=0;i<16;i++){rich.timestamp=fakeEpoch+3600;fakeEpoch=rich.timestamp;assert(recordWeatherObservation(rich));}
 auto largest=getBuddySave();
 for(int i=0;i<8;i++){auto &site=largest.locations[i];site.used=true;strcpy(site.name,"123456789012345");}
 for(auto &event:largest.fieldEvents)event={UINT32_MAX,largest.latestObservationAt,largest.latestObservationAt};
 for(int i=0;i<32;i++)largest.recentAlerts[i]={uint64_t(i+1),largest.latestObservationAt};
 for(auto &n:largest.fieldNotes){n.severeEvents=0;n.outcomes &= ~NEW_SEVERE_EVENT;}
 auto &newest=largest.fieldNotes[(largest.fieldNoteNext+15)%16];newest.severeEvents=0x7fff;newest.outcomes|=NEW_SEVERE_EVENT;
 Print fullJSON;assert(serializeBuddySave(largest,fullJSON));
 printf("Full metrics/sites/events export: %zu bytes\n",fullJSON.output.size());
 assert(fullJSON.output.size()<BUDDY_IMPORT_BYTES);
 assert(deserializeBuddySave(fullJSON.output.c_str(),fullJSON.output.size(),restored,error));
 assert(buddySaveChecksum(restored)==buddySaveChecksum(largest));
 // Restore the actual session so the subsequent import-rejection and migration fixtures agree.
 assert(importBuddy(json.c_str(),json.size()) && confirmBuddyImport(buddySaveChecksum(s)));fakeEpoch=s.latestObservationAt;
 assert(FIELD_NOTE_BYTES==56 && V9_PAYLOAD_BYTES==2340 && PAYLOAD_BYTES==2488 && RECORD_BYTES==2512);
 assert(json.size()<BUDDY_IMPORT_BYTES);initializeJournal();assert(buddySaveChecksum(getBuddySave())==buddySaveChecksum(s));
 for(int scenario=0;scenario<9;scenario++){
  auto bad=s;auto &n=bad.fieldNotes[(bad.fieldNoteNext+15)%16];
  if(scenario==0)n.category=WeatherCategory::UNKNOWN;
  if(scenario==1)n.metrics.validMask=32;
  if(scenario==2)n.timestamp=0;
  if(scenario==3)n.location=8;
  if(scenario==4)n.outcomes=1UL<<11;
  if(scenario==5)n.severeEvents=1UL<<15;
  if(scenario==6)n.outcomes|=NEW_GUST_RECORD; // missing metric
  if(scenario==7)n.severeEvents=1; // no discovery flag
  if(scenario==8)n.weatherCode=-1;
  assert(!validateBuddySave(bad));
 }
 // Every malformed history is rejected before the current buddy can be replaced.
 for(int scenario=0;scenario<15;scenario++){
  JsonDocument doc;assert(!deserializeJson(doc,json));auto n=doc["fieldNotes"][0];
  if(scenario==0)n["category"]=255;
  if(scenario==1)n["validMetrics"]=32;
  if(scenario==2)n["timestamp"]=0;
  if(scenario==3)n["location"]=8;
  if(scenario==4)n["location"]=1; // unused slot
  if(scenario==5)n["outcomes"]=1UL<<11;
  if(scenario==6)n["severeEvents"]=1UL<<15;
  if(scenario==7)n["severeEvents"]=1; // missing outcome flag / undiscovered event
  if(scenario==8)doc["fieldNotes"].add(doc["fieldNotes"][0]);
  if(scenario==9)doc["fieldNoteNext"]=16;
  if(scenario==10)n["metrics"]["humidityCentiPercent"]=10001;
  if(scenario==11)n["timestamp"]=fakeEpoch+1;
  if(scenario==12)doc["fieldNotes"][1]["timestamp"]=fakeEpoch;
  if(scenario==13)n["temperatureMilliC"]=100001;
  if(scenario==14)n["weatherCode"]=-1;
  std::string bad;serializeJson(doc,bad);assert(!importBuddy(bad.c_str(),bad.size()));
  assert(buddySaveChecksum(getBuddySave())==buddySaveChecksum(s));
 }
 assert(upsertFieldLocation(1,"TRAVEL",40,-90));assert(deleteFieldLocation(0,1));
 for(int age=0;age<16;age++)assert(fieldNoteAt(getBuddySave(),age)->location==UNKNOWN_LOCATION);
 assert(upsertFieldLocation(0,"NEW HOME",30,-80));
 for(int age=0;age<16;age++)assert(fieldNoteAt(getBuddySave(),age)->location==UNKNOWN_LOCATION);
 // Freeze a real v8 payload and envelope; preserve every v8 field, initialize notes empty.
 auto old=getBuddySave();old.saveVersion=8;uint8_t payload[V8_PAYLOAD_BYTES];
 encodeCurrent(old,payload);encodeEvents(old,payload+V5_PAYLOAD_BYTES);encodeLocations(old,payload+V6_PAYLOAD_BYTES);encodeVariants(old,payload+V7_PAYLOAD_BYTES);
 std::vector<uint8_t> envelope(HEADER_BYTES+V8_PAYLOAD_BYTES);uint8_t *p=envelope.data();
 put32(p,SAVE_MAGIC);put32(p,8);put32(p,V8_PAYLOAD_BYTES);put64(p,100);
 memcpy(envelope.data()+24,payload,sizeof(payload));put32(p,hashBytes(payload,sizeof(payload),hashBytes(envelope.data(),20)));
 storage.clear();storage["fwf-buddy"]["save0"]=envelope;initializeJournal();
 assert(dirty && getBuddySave().fieldNoteCount==0 && getBuddySave().fieldNoteNext==0);
 assert(v8Checksum(getBuddySave())==v8Checksum(old));checkpointJournal();initializeJournal();assert(!dirty);
 // Older supported JSON imports empty notes even if the newer optional fields are present.
 JsonDocument doc;assert(!deserializeJson(doc,json));doc["saveVersion"]=8;
 char hash[9];snprintf(hash,9,"%08lx",(unsigned long)v8Checksum(s));doc["checksum"]=hash;
 doc.remove("fieldNotes");doc.remove("fieldNoteNext");std::string legacy;serializeJson(doc,legacy);
 assert(deserializeBuddySave(legacy.c_str(),legacy.size(),restored,error));
 assert(restored.fieldNoteCount==0 && v8Checksum(restored)==v8Checksum(s));
 puts("PASS: field notes insertion/order/rollover, guards/cached boots, optional metrics, outcomes/severe annotation, NVS reload/migration, JSON safety and deleted-location reuse.");
}
