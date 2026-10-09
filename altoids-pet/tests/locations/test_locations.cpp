#define FWF_RICH_WEATHER_TEST
#include "../journal/test_journal.cpp"
void invalidateWeatherLocation(){invalidations++;}
int main(){
 setenv("TZ","CST6CDT,M3.2.0/2,M11.1.0/2",1);tzset();blank();
 assert(getBuddySave().activeLocation==0 && std::string(getBuddySave().locations[0].name)=="HOME" && getBuddySave().fieldSitesVisited==0);
 assert(!upsertFieldLocation(8,"BAD",0,0) && !upsertFieldLocation(1,"",0,0) && !upsertFieldLocation(1,"abcdefghijklmnop",0,0));
 assert(!upsertFieldLocation(1,"BAD\n",0,0) && !upsertFieldLocation(1," BAD",0,0));
 assert(!upsertFieldLocation(1,"BAD",91,0) && !upsertFieldLocation(1,"BAD",0,-181) && !upsertFieldLocation(1,"BAD",NAN,0));
 assert(upsertFieldLocation(1,"FIELD CAMP",41.5,-87.5));auto writes=nvsWrites;auto before=getBuddySave().totalObservations;
 assert(upsertFieldLocation(1,"FIELD CAMP",41.5,-87.5) && nvsWrites==writes);
 assert(getBuddySave().fieldSitesVisited==0 && activateFieldLocation(1));auto invalidated=invalidations;
 assert(activateFieldLocation(1) && invalidations==invalidated && getBuddySave().totalObservations==before);
 observe();auto first=getBuddySave();
 assert(first.fieldSitesVisited==1 && first.locations[1].visited && first.latestLocation==1 && first.highestTemperatureLocation==1 && first.lowestTemperatureLocation==1);
 assert(activateFieldLocation(0));observe(0,900);assert(getBuddySave().fieldSitesVisited==2 && getBuddySave().highestTemperatureLocation==0 && getBuddySave().lowestTemperatureLocation==1);
 observe(0,900);assert(getBuddySave().fieldSitesVisited==2 && getBuddySave().highestTemperatureAt<getBuddySave().latestObservationAt);
 assert(activateFieldLocation(1));fakeEpoch+=3600;WeatherObservation rich={fakeEpoch,fahrenheitDeciToMilliC(500),0,WeatherCategory::CLEAR};
 rich.location=1;rich.metrics.validMask=31;int32_t values[]={5000,3000,4500,100000,500};for(int i=0;i<5;i++)rich.metrics.values[i]=values[i];
 assert(recordWeatherObservation(rich));for(auto &r:getBuddySave().records)assert(r.location==1);
 assert(!deleteFieldLocation(1) && !deleteFieldLocation(1,1));auto old=getBuddySave();failWrite=true;
 assert(!deleteFieldLocation(1,0) && buddySaveChecksum(old)==buddySaveChecksum(getBuddySave()));failWrite=false;
 assert(deleteFieldLocation(1,0));assert(getBuddySave().activeLocation==0 && getBuddySave().fieldSitesVisited==2);
 for(auto &r:getBuddySave().records)assert(r.location==UNKNOWN_LOCATION && r.timestamp==fakeEpoch);
 assert(getBuddySave().lowestTemperatureLocation==UNKNOWN_LOCATION && getBuddySave().latestLocation==UNKNOWN_LOCATION);
 assert(upsertFieldLocation(1,"NEW SITE",-90,180));assert(!getBuddySave().locations[1].visited);
 assert(activateFieldLocation(1));observe();assert(getBuddySave().fieldSitesVisited==3);
 initializeJournal();assert(getBuddySave().activeLocation==1 && getBuddySave().fieldSitesVisited==3);
 for(int i=2;i<8;i++)assert(upsertFieldLocation(i,"123456789012345",90,-180));
 auto json=exported();BuddySaveData restored;const char *error;assert(deserializeBuddySave(json.c_str(),json.size(),restored,error));
 assert(buddySaveChecksum(restored)==buddySaveChecksum(getBuddySave()));
 auto full=getBuddySave();
 for(auto &event:full.fieldEvents)event={UINT32_MAX,full.createdAt,full.latestObservationAt};
 for(int i=0;i<RECENT_ALERT_COUNT;i++)full.recentAlerts[i]={uint64_t(i+1),full.latestObservationAt};
 Print largest;assert(serializeBuddySave(full,largest));
 printf("Eight-site/full-event export: %zu bytes (limit %zu)\n",largest.output.size(),BUDDY_IMPORT_BYTES);
 assert(deserializeBuddySave(largest.output.c_str(),largest.output.size(),restored,error));assert(buddySaveChecksum(full)==buddySaveChecksum(restored));
 // Complete v6 envelope migration preserves every old field byte; historical record locations remain unknown.
 auto prior=getBuddySave();prior.saveVersion=6;uint8_t payload[V6_PAYLOAD_BYTES];encodeCurrent(prior,payload);encodeEvents(prior,payload+V5_PAYLOAD_BYTES);
 std::vector<uint8_t> envelope(HEADER_BYTES+V6_PAYLOAD_BYTES);uint8_t *p=envelope.data();
 put32(p,SAVE_MAGIC);put32(p,6);put32(p,V6_PAYLOAD_BYTES);put64(p,99);memcpy(envelope.data()+HEADER_BYTES,payload,V6_PAYLOAD_BYTES);
 p=envelope.data()+20;put32(p,hashBytes(payload,V6_PAYLOAD_BYTES,hashBytes(envelope.data(),20)));
 storage.clear();storage["fwf-buddy"]["save1"]=envelope;initializeJournal();assert(dirty && v6Checksum(getBuddySave())==v6Checksum(prior));
 assert(getBuddySave().locations[0].latitudeMicrodegrees==-90000000 && getBuddySave().locations[0].longitudeMicrodegrees==180000000);
 assert(getBuddySave().latestLocation==UNKNOWN_LOCATION && getBuddySave().fieldSitesVisited==0);
 JsonDocument doc;assert(!deserializeJson(doc,json));doc["saveVersion"]=6;
 for(JsonPair pair:doc["records"].as<JsonObject>())pair.value().as<JsonObject>().remove("location");
 char hash[9];snprintf(hash,9,"%08lx",(unsigned long)v6Checksum(prior));doc["checksum"]=hash;std::string legacy;serializeJson(doc,legacy);
 assert(deserializeBuddySave(legacy.c_str(),legacy.size(),restored,error) && restored.fieldSitesVisited==0);
 for(int scenario=0;scenario<9;scenario++){
  assert(!deserializeJson(doc,json));
  if(scenario==0)doc["fieldLocations"][0]["name"]="name too long here";
  if(scenario==1)doc["fieldLocations"][1]["id"]=0;
  if(scenario==2)doc["fieldLocations"][0]["latitudeMicrodegrees"]=90000001;
  if(scenario==3)doc["activeLocation"]=255;
  if(scenario==4)doc["latestLocation"]=8;
  if(scenario==5)doc["fieldSitesVisited"]=0;
  if(scenario==6)doc["records"]["strongestWind"]["location"]=8;
  if(scenario==7)doc["fieldLocations"][0]["visited"]="yes";
  if(scenario==8)doc["fieldLocations"][1]["visited"]=false;
  std::string invalid;serializeJson(doc,invalid);assert(!deserializeBuddySave(invalid.c_str(),invalid.size(),restored,error));
 }
 // HOME may retain legacy config-fallback fields; import must compare actual slot coordinates.
 blank();auto different=getBuddySave();different.locations[0].latitudeMicrodegrees=1000000;
 Print replacement;assert(serializeBuddySave(different,replacement));assert(importBuddy(replacement.output.c_str(),replacement.output.size()));
 auto priorInvalidations=invalidations;assert(confirmBuddyImport(buddySaveChecksum(different)) && invalidations==priorInvalidations+1);
 // Human-readable Serial management, including names with spaces and safe deletion.
 blank();Serial.output.clear();send("UPSERT_LOCATION 1 40 -100 TRAVEL CAMP");assert(Serial.output.find("LOCATION_OK saved")!=std::string::npos);
 send("ACTIVE_LOCATION 1");assert(getBuddySave().activeLocation==1 && getBuddySave().totalObservations==0);
 send("DELETE_LOCATION 1");assert(locationExists(getBuddySave(),1));send("DELETE_LOCATION 1 0");assert(!locationExists(getBuddySave(),1));
 Serial.output.clear();send("LIST_LOCATIONS");assert(Serial.output.find("LOCATIONS {")!=std::string::npos);
 puts("PASS: eight slots, names/coordinates, persisted switch/delete/reuse, live-only visits, all record locations/ties, v6 NVS/JSON migration, full JSON/import validation and USB commands.");
}
