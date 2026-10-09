#define FWF_RICH_WEATHER_TEST
#include "../journal/test_journal.cpp"
void invalidateWeatherLocation(){invalidations++;}
#include "../../firmware/altoids_pet/field_events.cpp"
BuddySaveData milestone(WeatherCategory category,uint64_t count,bool freezing=false){
 BuddySaveData s;s.totalObservations=count;s.weatherCounts[uint8_t(category)]=count;
 s.lowestTemperatureMilliC=freezing?-1:20000;s.freezingObservations=freezing?count:0;
 evaluateGearUnlocks(s);evaluateGearVariants(s);return s;
}
void threshold(GearId item,uint8_t variant,WeatherCategory category,uint64_t count,bool freezing=false){
 auto before=milestone(category,count-1,freezing),after=milestone(category,count,freezing);
 assert(!gearVariantUnlocked(before,item,variant) && gearVariantUnlocked(after,item,variant));
}
int main(){
 setenv("TZ","CST6CDT,M3.2.0/2,M11.1.0/2",1);tzset();blank();
 threshold(GearId::FIELD_CAP,1,WeatherCategory::CLEAR,25);threshold(GearId::FIELD_CAP,2,WeatherCategory::CLEAR,100);
 threshold(GearId::SUNGLASSES,1,WeatherCategory::CLEAR,25);threshold(GearId::SUNGLASSES,2,WeatherCategory::CLEAR,100);
 threshold(GearId::UMBRELLA,1,WeatherCategory::RAIN,10);threshold(GearId::UMBRELLA,2,WeatherCategory::RAIN,25);threshold(GearId::UMBRELLA,3,WeatherCategory::RAIN,50);
 threshold(GearId::RAINCOAT,1,WeatherCategory::RAIN,25);threshold(GearId::RAINCOAT,2,WeatherCategory::RAIN,50);threshold(GearId::RAINCOAT,3,WeatherCategory::RAIN,100);
 threshold(GearId::WINTER_SCARF,1,WeatherCategory::SNOW,10);threshold(GearId::WINTER_SCARF,2,WeatherCategory::SNOW,25);
 threshold(GearId::WINTER_COAT,1,WeatherCategory::SNOW,15);threshold(GearId::WINTER_COAT,2,WeatherCategory::SNOW,30);
 threshold(GearId::BOOTS,1,WeatherCategory::CLEAR,10,true);
 auto sunny=milestone(WeatherCategory::MAINLY_CLEAR,25);assert(gearVariantUnlocked(sunny,GearId::SUNGLASSES,1));
 auto owned=milestone(WeatherCategory::RAIN,100);owned.weatherCounts[uint8_t(WeatherCategory::SNOW)]=30;owned.lowestTemperatureMilliC=-1;
 evaluateGearUnlocks(owned);
 const FieldEventId events[]={FieldEventId::FLASH_FLOOD_WARNING,FieldEventId::TORNADO_WARNING,FieldEventId::HURRICANE_WARNING,FieldEventId::BLIZZARD_WARNING,FieldEventId::EXTREME_HEAT_WARNING};
 for(auto event:events)owned.fieldEvents[uint8_t(event)].count=1;evaluateGearVariants(owned);
 assert(gearVariantUnlocked(owned,GearId::BOOTS,2) && gearVariantUnlocked(owned,GearId::RAINCOAT,4) && gearVariantUnlocked(owned,GearId::UMBRELLA,4));
 assert(gearVariantUnlocked(owned,GearId::WINTER_SCARF,3) && gearVariantUnlocked(owned,GearId::WINTER_COAT,3) && gearVariantUnlocked(owned,GearId::FIELD_CAP,3));
 // Real observation path: no extra NVS write for colors and no farming rejected observations.
 for(int i=0;i<25;i++)observe(61,700);auto saved=getBuddySave();
 assert(gearVariantUnlocked(saved,GearId::RAINCOAT,1) && gearVariantUnlocked(saved,GearId::UMBRELLA,2));
 assert(!equipJournalGearVariant(GearSlot::BODY,GearId::RAINCOAT,2));
 auto writes=nvsWrites;assert(equipJournalGearVariant(GearSlot::BODY,GearId::RAINCOAT,1) && nvsWrites==writes+1);
 writes=nvsWrites;assert(equipJournalGearVariant(GearSlot::BODY,GearId::RAINCOAT,1) && nvsWrites==writes);
 for(int i=0;i<20;i++){initializeJournal();assert(getBuddySave().equippedVariants[uint8_t(GearSlot::BODY)]==1);updateJournal();}
 assert(nvsWrites==writes);
 assert(!recordWeatherObservation({fakeEpoch,20000,61,WeatherCategory::RAIN}));assert(nvsWrites==writes);
 // Severe hook evaluated in the existing batch checkpoint, not another save.
 ActiveFieldAlert alert={FieldEventId::TORNADO_WARNING,12345};assert(collectFieldAlerts(&alert,1,fakeEpoch));assert(nvsWrites==writes+1);
 assert(gearVariantUnlocked(getBuddySave(),GearId::RAINCOAT,4));assert(equipJournalGearVariant(GearSlot::BODY,GearId::RAINCOAT,4));
 auto json=exported();BuddySaveData restored;const char *error;
 assert(deserializeBuddySave(json.c_str(),json.size(),restored,error));assert(buddySaveChecksum(restored)==buddySaveChecksum(getBuddySave()));
 auto old=getBuddySave();old.saveVersion=7;uint8_t payload[V7_PAYLOAD_BYTES];encodeCurrent(old,payload);encodeEvents(old,payload+V5_PAYLOAD_BYTES);encodeLocations(old,payload+V6_PAYLOAD_BYTES);
 std::vector<uint8_t> envelope(HEADER_BYTES+V7_PAYLOAD_BYTES);uint8_t *p=envelope.data();put32(p,SAVE_MAGIC);put32(p,7);put32(p,V7_PAYLOAD_BYTES);put64(p,100);
 memcpy(envelope.data()+HEADER_BYTES,payload,V7_PAYLOAD_BYTES);p=envelope.data()+20;put32(p,hashBytes(payload,V7_PAYLOAD_BYTES,hashBytes(envelope.data(),20)));
 storage.clear();storage["fwf-buddy"]["save0"]=envelope;initializeJournal();assert(dirty && v7Checksum(getBuddySave())==v7Checksum(old));
 assert(getBuddySave().equippedSlots[uint8_t(GearSlot::BODY)]==GearId::RAINCOAT && getBuddySave().equippedVariants[uint8_t(GearSlot::BODY)]==0);
 for(uint8_t id=1;id<=7;id++)assert(getBuddySave().unlockedVariants[id-1]==((old.unlockedGear & gearFlag(GearId(id)))?1:0));
 JsonDocument doc;assert(!deserializeJson(doc,json));doc["saveVersion"]=7;char hash[9];snprintf(hash,9,"%08lx",(unsigned long)v7Checksum(old));doc["checksum"]=hash;
 doc.remove("unlockedVariants");doc.remove("equippedVariants");doc.remove("freezingObservations");std::string legacy;serializeJson(doc,legacy);
 assert(deserializeBuddySave(legacy.c_str(),legacy.size(),restored,error) && restored.unlockedVariants[3]==1 && restored.equippedVariants[3]==0);
 for(int scenario=0;scenario<5;scenario++){
  assert(!deserializeJson(doc,json));
  if(scenario==0)doc["equippedVariants"]["BODY"]=5;
  if(scenario==1)doc["unlockedVariants"]["RAINCOAT"]=32;
  if(scenario==2)doc["unlockedVariants"]["RAINCOAT"]=0;
  if(scenario==3)doc["freezingObservations"]=UINT64_MAX;
  if(scenario==4)doc["equippedVariants"]["BODY"]="BLUE";
  std::string invalid;serializeJson(doc,invalid);assert(!deserializeBuddySave(invalid.c_str(),invalid.size(),restored,error));
 }
 // Migration uses only one provable historical freezing encounter; new accepted ones count normally.
 auto historic=milestone(WeatherCategory::SNOW,100,true);historic.freezingObservations=0;migrateVariants(historic);assert(historic.freezingObservations==1);
 puts("PASS: all tier boundaries, severe hooks, default v7 NVS/JSON migration, locked equip rejection, reboot/deep-wake reloads, no extra color checkpoints, JSON roundtrip/validation and freezing-count migration.");
}
