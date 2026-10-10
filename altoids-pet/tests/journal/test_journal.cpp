#include <assert.h>
#include <map>
#include <vector>
#include <deque>
#include <string>
#include "Arduino.h"
#include "Preferences.h"
#include "weather.h"
SerialType Serial;
#include "palette.h"
PetPalette petPalette;
unsigned long fakeMillis=0;
#ifndef FWF_NETWORK_TEST
bool timeValid=true;
#endif
unsigned invalidations=0;
#ifndef FWF_RICH_WEATHER_TEST
void invalidateWeatherLocation(){invalidations++;}
#endif
time_t fakeEpoch=1800000000;
extern "C" time_t time(time_t *p) { if(p)*p=fakeEpoch; return fakeEpoch; }
std::map<std::string,Entries> storage;
unsigned nvsWrites=0;
bool failOpen=false,failWrite=false,corruptWrite=false;
// Weather interface fake: the real mapping is preserved by the refactor.
#ifndef FWF_NETWORK_TEST
WeatherState mapWeatherCode(int code) {
 switch(code) {
  case 0:return WEATHER_CLEAR; case 1:return WEATHER_MAINLY_CLEAR;
  case 2:return WEATHER_PARTLY_CLOUDY; case 3:return WEATHER_CLOUDY;
  case 45:return WEATHER_FOG; case 61:return WEATHER_RAIN;
  case 71:return WEATHER_SNOW; case 95:return WEATHER_STORM;
  default:return WEATHER_UNKNOWN;
 }
}
#endif
#include "../../firmware/altoids_pet/achievements.cpp"
#include "../../firmware/altoids_pet/sound.h"
std::vector<SoundEvent> queuedSoundEvents;
void queueSoundEvent(SoundEvent event){queuedSoundEvents.push_back(event);}
#include "../../firmware/altoids_pet/save.cpp"
#include "../../firmware/altoids_pet/journal.cpp"
#include "../../firmware/altoids_pet/gear.cpp"
#include "../../firmware/altoids_pet/gear_variants.cpp"
void blank() { storage.clear();nvsWrites=0;failOpen=failWrite=corruptWrite=false;fakeMillis=0;fakeEpoch=1800000000;timeValid=true;Serial.input.clear();initializeJournal(); }
void observe(int code=0,int temperature=700,int seconds=3600) {
 fakeEpoch+=seconds;fakeMillis+=seconds*1000;
 assert(recordWeatherObservation({fakeEpoch,fahrenheitDeciToMilliC(temperature),code,observationCategoryForCode(code)}));
}
std::string exported() { Print p;assert(exportBuddy(p));return p.output; }
void send(const std::string &line) { for(char c:line)Serial.input.push_back(c);Serial.input.push_back('\n');while(Serial.available())updateBuddySerial(); }
// Frozen v1 field order: independent fixture for the installed single-item save.
std::vector<uint8_t> oldPayload(const BuddySaveData &s, GearId gear) {
 std::vector<uint8_t> bytes(148);uint8_t *p=bytes.data();
 put32(p,1);put64(p,s.createdAt);put64(p,s.totalObservations);put32(p,s.uniqueDaysObserved);
 put64(p,s.latestObservationAt);put32(p,s.lastObservedDate);put32(p,s.totalObservations ? milliCToFahrenheitDeci(s.latestTemperatureMilliC) : 0);
 put32(p,s.latestWeatherCode);put32(p,static_cast<uint8_t>(s.latestCategory));
 put32(p,s.totalObservations ? milliCToFahrenheitDeci(s.highestTemperatureMilliC) : 0);put64(p,s.highestTemperatureAt);
 put32(p,s.totalObservations ? milliCToFahrenheitDeci(s.lowestTemperatureMilliC) : 0);put64(p,s.lowestTemperatureAt);
 for(auto count:s.weatherCounts)put64(p,count);
 put32(p,s.discoveredWeather);put32(p,s.unlockedGear);put32(p,static_cast<uint8_t>(gear));
 assert(p==bytes.data()+148);return bytes;
}
std::vector<uint8_t> oldRecord(const BuddySaveData &s, GearId gear, uint64_t gen) {
 auto payload=oldPayload(s,gear);std::vector<uint8_t> bytes(172);uint8_t *p=bytes.data();
 put32(p,SAVE_MAGIC);put32(p,1);put32(p,148);put64(p,gen);
 memcpy(bytes.data()+24,payload.data(),148);
 put32(p,hashBytes(payload.data(),148,hashBytes(bytes.data(),20)));return bytes;
}
void legacyTemperatures(JsonDocument &doc, const BuddySaveData &data) {
 doc["latestTemperatureDeciF"]=milliCToFahrenheitDeci(data.latestTemperatureMilliC);
 doc["highestTemperatureDeciF"]=milliCToFahrenheitDeci(data.highestTemperatureMilliC);
 doc["lowestTemperatureDeciF"]=milliCToFahrenheitDeci(data.lowestTemperatureMilliC);
 doc.remove("latestTemperatureMilliC");doc.remove("highestTemperatureMilliC");doc.remove("lowestTemperatureMilliC");
 doc.remove("latestMetrics");doc.remove("records");
}
void clearVariantHistory(BuddySaveData &s) {
 s.fieldNoteCount=s.fieldNoteNext=0;for(auto &note:s.fieldNotes)note=FieldNote{};
 s.freezingObservations=s.totalObservations && s.lowestTemperatureMilliC<0?1:0;
 for(uint8_t id=1;id<=7;id++)s.unlockedVariants[id-1]=(s.unlockedGear & gearFlag(GearId(id)))?1:0;
 for(auto &v:s.equippedVariants)v=0;
}
void reconstructTestAchievements(BuddySaveData &s) {
 s.unlockedAchievements=0;for(auto &at:s.achievementUnlockedAt)at=0;reconstructAchievements(s);
}
void clearLocationHistory(BuddySaveData &s) {
 clearVariantHistory(s);
 s.fieldSitesVisited=0;s.latestLocation=s.highestTemperatureLocation=s.lowestTemperatureLocation=UNKNOWN_LOCATION;
 for(auto &site:s.locations)site.visited=false;
 for(auto &record:s.records)record.location=UNKNOWN_LOCATION;
 reconstructTestAchievements(s);
}
#ifndef FWF_RICH_WEATHER_TEST
int main() {
 setenv("TZ","CST6CDT,M3.2.0/2,M11.1.0/2",1);tzset();
 blank(); checkpointJournal();assert(nvsWrites==1 && getBuddySave().createdAt==fakeEpoch);
 assert(buddyNeedsSetup()); initializeJournal(); assert(buddyNeedsSetup());
 unsigned setupWrites=nvsWrites;failWrite=true;assert(!confirmBuddySetup(FurPaletteId::BLUE));
 assert(buddyNeedsSetup() && getBuddySave().furPalette==FurPaletteId::ORANGE && nvsWrites==setupWrites);
 failWrite=false;assert(!confirmBuddySetup(static_cast<FurPaletteId>(5)));
 assert(confirmBuddySetup(FurPaletteId::BLUE) && nvsWrites==setupWrites+1);
 initializeJournal();assert(!buddyNeedsSetup() && getBuddySave().furPalette==FurPaletteId::BLUE);
 setupWrites=nvsWrites;assert(!confirmBuddySetup(FurPaletteId::CREAM) && nvsWrites==setupWrites);
 int64_t birth=getBuddySave().createdAt;
 observe();assert(getBuddySave().totalObservations==1 && getBuddySave().uniqueDaysObserved==1);
 assert(getBuddySave().unlockedGear==(gearFlag(GearId::FIELD_CAP)|gearFlag(GearId::SUNGLASSES)));
 assert(getBuddySave().highestTemperatureAt==fakeEpoch && getBuddySave().lowestTemperatureAt==fakeEpoch);
 unsigned writes=nvsWrites;
 for(int i=0;i<50;i++)updateJournal();assert(nvsWrites==writes);
 // Deep-sleep/cold reboot loads permanent progress; reopening or same live timestamp does not award.
 initializeJournal();assert(getBuddySave().createdAt==birth && getBuddySave().totalObservations==1);
 assert(!recordWeatherObservation({fakeEpoch+59,fahrenheitDeciToMilliC(999),0,WeatherCategory::CLEAR}));assert(nvsWrites==writes);
 assert(!recordWeatherObservation({fakeEpoch-10,fahrenheitDeciToMilliC(999),0,WeatherCategory::CLEAR}));
 assert(!recordWeatherObservation({fakeEpoch+60,fahrenheitDeciToMilliC(999),0,WeatherCategory::SNOW}));
 observe(61,650);assert(getBuddySave().uniqueDaysObserved==1 && getBuddySave().lowestTemperatureMilliC==fahrenheitDeciToMilliC(650));
 int64_t lowAt=getBuddySave().lowestTemperatureAt;
 observe(1,800,86400);assert(getBuddySave().uniqueDaysObserved==2 && getBuddySave().highestTemperatureMilliC==fahrenheitDeciToMilliC(800));
 observe(3,650);assert(getBuddySave().lowestTemperatureAt==lowAt);
 observe(95,550);assert(getBuddySave().weatherCounts[5]==1); // Ties retain first record date.
 for(int i=1;i<10;i++)observe(61,600);
 assert(getBuddySave().weatherCounts[4]==10 && (getBuddySave().unlockedGear&gearFlag(GearId::RAINCOAT)));
 for(int i=0;i<5;i++)observe(71,310);
 assert(getBuddySave().weatherCounts[6]==5 && (getBuddySave().unlockedGear&gearFlag(GearId::WINTER_COAT)));
 assert(getBuddySave().unlockedGear&gearFlag(GearId::BOOTS));
 assert(!equipGear(GearSlot::HEAD, static_cast<GearId>(99))); assert(equipGear(GearSlot::HEAD, GearId::FIELD_CAP));
 writes=nvsWrites; assert(equipGear(GearSlot::HEAD, GearId::FIELD_CAP)&&nvsWrites==writes);
 assert(equipGear(GearSlot::HEAD, GearId::NONE));
 // Equip all slots, replace only BODY, reject wrong-slot and locked selections.
 for(uint8_t id=1;id<=7;++id)assert(equipGear(gearSlot(static_cast<GearId>(id)),static_cast<GearId>(id)));
 auto outfit=getBuddySave();assert(outfit.equippedSlots[3]==GearId::WINTER_COAT);
 assert(equipGear(GearSlot::BODY,GearId::RAINCOAT));
 for(uint8_t i=0;i<6;++i)if(i!=3)assert(getBuddySave().equippedSlots[i]==outfit.equippedSlots[i]);
 writes=nvsWrites;assert(!equipGear(GearSlot::HEAD,GearId::RAINCOAT));
 assert(!equipGear(GearSlot::COUNT,GearId::NONE) && nvsWrites==writes);
 initializeJournal();assert(getBuddySave().equippedSlots[3]==GearId::RAINCOAT);
 auto saved=getBuddySave();std::string json=exported();
 BuddySaveData restored;const char *error=nullptr;
 assert(deserializeBuddySave(json.c_str(),json.size(),restored,error));
 assert(buddySaveChecksum(saved)==buddySaveChecksum(restored));
 // JSON corruption, missing/type-invalid fields, inconsistent counters and unsupported versions.
 JsonDocument doc;assert(!deserializeJson(doc,json));doc["totalObservations"]=999;
 std::string bad;serializeJson(doc,bad);assert(!deserializeBuddySave(bad.c_str(),bad.size(),restored,error));
 assert(!deserializeJson(doc,json));doc["saveVersion"]=999;bad.clear();serializeJson(doc,bad);
 assert(!importBuddy(bad.c_str(),bad.size()));assert(buddySaveChecksum(saved)==buddySaveChecksum(getBuddySave()));
 assert(!deserializeJson(doc,json));doc.remove("researchBeganAt");bad.clear();serializeJson(doc,bad);
 assert(!deserializeBuddySave(bad.c_str(),bad.size(),restored,error));
 assert(!deserializeJson(doc,json));doc["unlockedGear"]=true;bad.clear();serializeJson(doc,bad);
 assert(!deserializeBuddySave(bad.c_str(),bad.size(),restored,error));
 // Date/record consistency and overflow checks are independent of checksum.
 auto invalid=saved; invalid.lastObservedDate=20260230; assert(!validateBuddySave(invalid));
 invalid=saved;invalid.lastObservedDate=20260101;assert(!validateBuddySave(invalid));
 invalid=saved;invalid.highestTemperatureAt=invalid.latestObservationAt+1;assert(!validateBuddySave(invalid));
 invalid=saved;invalid.weatherCounts[0]=UINT64_MAX;assert(!validateBuddySave(invalid));
 invalid=saved;invalid.equippedSlots[0]=static_cast<GearId>(9);assert(!validateBuddySave(invalid));
 // Staging writes nothing; checksum confirmation, cancel and expiry protect existing buddy.
 writes=nvsWrites;assert(importBuddy(json.c_str(),json.size()));assert(nvsWrites==writes);
 assert(!confirmBuddyImport(0));cancelBuddyImport();assert(!confirmBuddyImport(buddySaveChecksum(saved)));
 assert(importBuddy(json.c_str(),json.size()));fakeMillis+=60000;assert(!confirmBuddyImport(buddySaveChecksum(saved)));updateBuddySerial();
 assert(importBuddy(json.c_str(),json.size()));assert(confirmBuddyImport(buddySaveChecksum(saved)));assert(nvsWrites==writes+1);
 initializeJournal();assert(buddySaveChecksum(getBuddySave())==buddySaveChecksum(saved));
 // Confirmed import can deliberately replace the current buddy's start/progress.
 BuddySaveData other;other.createdAt=saved.createdAt-86400;
 Print otherOutput;assert(serializeBuddySave(other,otherOutput));
 assert(importBuddy(otherOutput.output.c_str(),otherOutput.output.size()));
 assert(confirmBuddyImport(buddySaveChecksum(other)));initializeJournal();
 assert(getBuddySave().createdAt==other.createdAt && getBuddySave().totalObservations==0);
 assert(importBuddy(json.c_str(),json.size()));assert(confirmBuddyImport(buddySaveChecksum(saved)));
 // Every old equipped ID (including NONE) migrates without changing progress.
 auto migrationStorage=storage;
 for(uint8_t id=0;id<=7;++id) {
   GearId gear=static_cast<GearId>(id);auto record=oldRecord(saved,gear,9);
   storage.clear();storage["fwf-buddy"]["save1"]=record;
   initializeJournal();assert(journalAvailable() && dirty && generation==9);
   auto migrated=getBuddySave();assert(migrated.saveVersion==SAVE_VERSION);
   auto expected=saved;clearLocationHistory(expected);expected.setupComplete=true;expected.furPalette=FurPaletteId::ORANGE;for(auto &item:expected.equippedSlots)item=GearId::NONE;
   if(id)expected.equippedSlots[static_cast<uint8_t>(gearSlot(gear))]=gear;
   assert(buddySaveChecksum(migrated)==buddySaveChecksum(expected));
   failWrite=true;checkpointJournal(true);assert(storage["fwf-buddy"]["save1"]==record);
   failWrite=false;checkpointJournal(true);assert(!dirty && generation==10);
   assert(storage["fwf-buddy"]["save1"]==record); // Valid v1 fallback kept intact.
   initializeJournal();assert(!dirty && buddySaveChecksum(getBuddySave())==buddySaveChecksum(expected));
   storage["fwf-buddy"]["save0"].back()^=1;
   initializeJournal();assert(dirty && buddySaveChecksum(getBuddySave())==buddySaveChecksum(expected));
   // Old JSON checksum must be verified using v1 bytes, then upgrade for confirmation.
   JsonDocument oldJson;assert(!deserializeJson(oldJson,json));oldJson["saveVersion"]=1;legacyTemperatures(oldJson,saved);
   oldJson.remove("equippedSlots");oldJson["equippedGear"]=id;
   auto payload=oldPayload(saved,gear);char hash[9];snprintf(hash,9,"%08lx",(unsigned long)hashBytes(payload.data(),payload.size()));
   oldJson["checksum"]=hash;std::string backup;serializeJson(oldJson,backup);
   assert(deserializeBuddySave(backup.c_str(),backup.size(),restored,error));
   assert(buddySaveChecksum(restored)==buddySaveChecksum(expected));
   oldJson["equippedGear"]=(id+1)%8;backup.clear();serializeJson(oldJson,backup);
   assert(!deserializeBuddySave(backup.c_str(),backup.size(),restored,error));
 }
 storage=migrationStorage;initializeJournal();
 // V2 JSON requires all slots and rejects incompatible/locked gear and wrong types.
 for(int scenario=0;scenario<4;++scenario) {
   assert(!deserializeJson(doc,json));
   if(scenario==0)doc["equippedSlots"].remove("PROP");
   if(scenario==1)doc["equippedSlots"]["HEAD"]=4;
   if(scenario==2)doc["equippedSlots"]["HEAD"]=true;
   if(scenario==3)doc["equippedSlots"]["EXTRA"]=0;
   bad.clear();serializeJson(doc,bad);assert(!deserializeBuddySave(bad.c_str(),bad.size(),restored,error));
 }
 // Frozen v2 records migrate all six slots, palette ORANGE, setup complete.
 auto original=getBuddySave();auto beforeV2=storage;
 std::vector<uint8_t> v2(192);uint8_t *vp=v2.data();
 put32(vp,SAVE_MAGIC);put32(vp,2);put32(vp,168);put64(vp,31);vp+=4;
 auto common=oldPayload(original,GearId::NONE);memcpy(v2.data()+24,common.data(),144);
 vp=v2.data()+24;put32(vp,2);vp=v2.data()+24+144;
 for(auto item:original.equippedSlots)put32(vp,static_cast<uint8_t>(item));
 vp=v2.data()+20;put32(vp,hashBytes(v2.data()+24,168,hashBytes(v2.data(),20)));
 storage.clear();storage["fwf-buddy"]["save1"]=v2;initializeJournal();
 auto v2Expected=original;clearLocationHistory(v2Expected);v2Expected.furPalette=FurPaletteId::ORANGE;v2Expected.setupComplete=true;
 assert(!buddyNeedsSetup() && dirty && buddySaveChecksum(getBuddySave())==buddySaveChecksum(v2Expected));
 failWrite=true;checkpointJournal(true);assert(storage["fwf-buddy"]["save1"]==v2);
 failWrite=false;checkpointJournal(true);initializeJournal();assert(!dirty && !buddyNeedsSetup());
 // V2 JSON verifies its original checksum before upgrading/defaulting palette.
 assert(!deserializeJson(doc,json));doc["saveVersion"]=2;legacyTemperatures(doc,v2Expected);doc.remove("furPalette");doc.remove("setupComplete");
 char v2Hash[9];snprintf(v2Hash,9,"%08lx",(unsigned long)hashBytes(v2.data()+24,168));doc["checksum"]=v2Hash;
 bad.clear();serializeJson(doc,bad);assert(deserializeBuddySave(bad.c_str(),bad.size(),restored,error));
 assert(buddySaveChecksum(restored)==buddySaveChecksum(v2Expected));
 storage=beforeV2;initializeJournal();
 for(int scenario=0;scenario<4;++scenario){
   assert(!deserializeJson(doc,json));
   if(scenario==0)doc["furPalette"]=5;
   if(scenario==1)doc["furPalette"]="BLUE";
   if(scenario==2)doc.remove("furPalette");
   if(scenario==3)doc["setupComplete"]=1;
   bad.clear();serializeJson(doc,bad);assert(!deserializeBuddySave(bad.c_str(),bad.size(),restored,error));
 }
 // Independent v3 fixture retains palette/setup and every progress field.
 auto beforeV3Storage=storage; auto v3Buddy=getBuddySave();
 v3Buddy.furPalette=FurPaletteId::BLUE;v3Buddy.setupComplete=true;
 uint8_t full[PAYLOAD_BYTES];auto oldV3Data=v3Buddy;oldV3Data.saveVersion=3;encodeCurrent(oldV3Data,full);uint8_t *ver=full;put32(ver,3);
 std::vector<uint8_t> v3(200);uint8_t *header=v3.data();
 put32(header,SAVE_MAGIC);put32(header,3);put32(header,176);put64(header,55);header+=4;
 memcpy(v3.data()+24,full,176);header=v3.data()+20;
 put32(header,hashBytes(v3.data()+24,176,hashBytes(v3.data(),20)));
 storage.clear();storage["fwf-buddy"]["save1"]=v3;initializeJournal();
 assert(dirty && !buddyNeedsSetup() && getBuddySave().furPalette==FurPaletteId::BLUE);
 assert(getBuddySave().soundEnabled && getBuddySave().units==UnitsId::US && !getBuddySave().locationConfigured);
 auto expectedV3=v3Buddy;clearLocationHistory(expectedV3);assert(buddySaveChecksum(getBuddySave())==buddySaveChecksum(expectedV3));
 checkpointJournal(true);initializeJournal();assert(!dirty && !buddyNeedsSetup());
 Print oldV3;assert(serializeBuddySave(v3Buddy,oldV3));assert(!deserializeJson(doc,oldV3.output));doc["saveVersion"]=3;legacyTemperatures(doc,v3Buddy);
 for(const char *key:{"soundEnabled","units","locationConfigured","latitudeMicrodegrees","longitudeMicrodegrees"})doc.remove(key);
 char v3Hash[9];snprintf(v3Hash,9,"%08lx",(unsigned long)hashBytes(full,176));doc["checksum"]=v3Hash;
 bad.clear();serializeJson(doc,bad);assert(deserializeBuddySave(bad.c_str(),bad.size(),restored,error));
 assert(buddySaveChecksum(restored)==buddySaveChecksum(expectedV3));
 storage=beforeV3Storage;initializeJournal();
 // All palettes roundtrip and preserve all existing buddy fields.
 for(uint8_t id=0;id<5;++id){
   auto colored=saved;colored.furPalette=static_cast<FurPaletteId>(id);Print output;
   assert(serializeBuddySave(colored,output));assert(deserializeBuddySave(output.output.c_str(),output.output.size(),restored,error));
   assert(buddySaveChecksum(colored)==buddySaveChecksum(restored));
   assert(persistBuddySave(colored));initializeJournal();
   assert(!buddyNeedsSetup() && buddySaveChecksum(getBuddySave())==buddySaveChecksum(colored));
 }
 // Failed writes leave earlier checkpoint valid; rate-limited retry and sleep checkpoint.
 failWrite=true;observe(45,500);auto pending=getBuddySave();writes=nvsWrites;
 for(int i=0;i<10;i++) {fakeMillis+=1000;updateJournal();}assert(nvsWrites==writes);
 failWrite=false;checkpointJournal(true);assert(nvsWrites==writes+1);
 initializeJournal();assert(buddySaveChecksum(getBuddySave())==buddySaveChecksum(pending));
 assert(importBuddy(json.c_str(),json.size()));failWrite=true;
 auto beforeImport=getBuddySave();assert(!confirmBuddyImport(buddySaveChecksum(saved)));
 assert(buddySaveChecksum(beforeImport)==buddySaveChecksum(getBuddySave()));failWrite=false;
 // Newest damaged slot falls back to previous valid slot.
 auto previous=getBuddySave();observe(2,450);auto newest=getBuddySave();
 const char *latestKey=generation%2?"save1":"save0";storage["fwf-buddy"][latestKey].back()^=1;
 initializeJournal();assert(buddySaveChecksum(getBuddySave())==buddySaveChecksum(previous));
 // Unknown version on either slot locks all writes, including import/equipment.
 storage["fwf-buddy"][latestKey][4]=99;auto protectedStorage=storage;
 initializeJournal();assert(!journalAvailable());
 assert(!recordWeatherObservation({fakeEpoch+3600,fahrenheitDeciToMilliC(700),0,WeatherCategory::CLEAR}));
 assert(!importBuddy(json.c_str(),json.size()));checkpointJournal(true);assert(storage==protectedStorage);
 // Unknown payload size also survives boot unchanged.
 blank();checkpointJournal();storage["fwf-buddy"]["save0"]=std::vector<uint8_t>(200,0);protectedStorage=storage;
 initializeJournal();assert(!journalAvailable() && storage==protectedStorage);
 // Legacy two-slot migration: stable birthday authoritative, no friendship/count conversion.
 blank();storage.clear();LegacySlot old={};old.record.magic=0x50455431;old.record.version=1;
 old.record.state={LegacyMood::HAPPY,73,9999,1790000000};old.record.observedEpoch=1790001000;
 old.generation=7;old.checksum=hashBytes((const uint8_t*)&old,offsetof(LegacySlot,checksum));
 storage["altoids-pet"]["state1"]=std::vector<uint8_t>((uint8_t*)&old,(uint8_t*)&old+sizeof(old));
 int64_t birthday=1789000000;storage["altoids-pet"]["birthday"]=std::vector<uint8_t>((uint8_t*)&birthday,(uint8_t*)&birthday+8);
 auto oldKeys=storage["altoids-pet"];initializeJournal();assert(getBuddySave().createdAt==birthday && getBuddySave().totalObservations==0);
 checkpointJournal();assert(storage["altoids-pet"]==oldKeys);initializeJournal();assert(getBuddySave().createdAt==birthday);
 // Original single-key layout migration, including deferred start date.
 blank();storage.clear();old.record.state.createdAt=1791000000;
 storage["altoids-pet"]["state"]=std::vector<uint8_t>((uint8_t*)&old.record,(uint8_t*)&old.record+sizeof(old.record));
 initializeJournal();assert(getBuddySave().createdAt==1791000000);
 blank();timeValid=false;initializeJournal();checkpointJournal();assert(getBuddySave().createdAt==0);
 timeValid=true;fakeEpoch+=100;checkpointJournal();birth=getBuddySave().createdAt;initializeJournal();assert(getBuddySave().createdAt==birth);
 // Setup confirmation retains observations made by the unchanged startup fetch.
 blank();observe(71,300);auto beforeSetup=getBuddySave();assert(buddyNeedsSetup());
 beforeSetup.furPalette=FurPaletteId::CREAM;beforeSetup.setupComplete=true;
 assert(confirmBuddySetup(FurPaletteId::CREAM));initializeJournal();
 assert(!buddyNeedsSetup() && buddySaveChecksum(getBuddySave())==buddySaveChecksum(beforeSetup));
 // Corrupt known records are protected, never classified as a genuinely new buddy.
 blank();checkpointJournal();auto intact=storage;
 storage["fwf-buddy"]["save1"].back()^=1;auto corrupt=storage;
 initializeJournal();assert(!journalAvailable() && !buddyNeedsSetup() && storage==corrupt);
 blank();storage.clear();storage["altoids-pet"]["birthday"]=std::vector<uint8_t>(3,0);
 initializeJournal();assert(!journalAvailable() && !buddyNeedsSetup());
 // Unknown legacy version is preserved, not interpreted as a new buddy.
 blank();storage.clear();old.record.version=2;
 storage["altoids-pet"]["state"]=std::vector<uint8_t>((uint8_t*)&old.record,(uint8_t*)&old.record+sizeof(old.record));
 initializeJournal();assert(!journalAvailable());
 // Storage unavailable doesn't overwrite an unread save.
 blank();checkpointJournal();protectedStorage=storage;failOpen=true;initializeJournal();assert(!journalAvailable());checkpointJournal();assert(storage==protectedStorage);failOpen=false;
 // Serial commands are bounded and JSON must be complete, with explicit confirmation.
 blank();observe();saved=getBuddySave();json=exported();
 assert(!deserializeJson(doc,json));std::string compact;serializeJson(doc,compact);
 writes=nvsWrites;beginBuddyTransfer(true);send("IMPORT_BUDDY "+compact);assert(nvsWrites==writes && importPending);
 char command[64];snprintf(command,sizeof(command),"CONFIRM_IMPORT %08lx",(unsigned long)buddySaveChecksum(saved));
 send(command);assert(nvsWrites==writes && importPending);assert(confirmBuddyImport(buddySaveChecksum(saved)));assert(nvsWrites==writes+1);
 writes=nvsWrites;send(std::string(4200,'x'));assert(nvsWrites==writes && !importPending);
 endBuddyTransfer();Serial.output.clear();send("EXPORT_BUDDY");assert(Serial.output.find("researchBeganAt")!=std::string::npos);
 // LF, CR and CRLF each execute one export, never write or duplicate JSON.
 auto exportSnapshot=getBuddySave();writes=nvsWrites;
 for(const char *ending:{"\n","\r","\r\n"}) {
   beginBuddyTransfer(false);Serial.output.clear();
   std::string commandWithEnding=std::string("EXPORT_BUDDY")+ending;
   for(char c:commandWithEnding)Serial.input.push_back(c);
   while(Serial.available())updateBuddySerial();
   assert(buddyTransferStatus()==BuddyTransferStatus::EXPORT_COMPLETE);
   BuddySaveData parsed;const char *parseError=nullptr;
   assert(deserializeBuddySave(Serial.output.c_str(),Serial.output.size(),parsed,parseError));
   auto position=Serial.output.find("researchBeganAt");assert(position!=std::string::npos);
   assert(Serial.output.find("researchBeganAt",position+1)==std::string::npos);
   assert(nvsWrites==writes && buddySaveChecksum(exportSnapshot)==buddySaveChecksum(getBuddySave()));
 }
 // No line ending means an incomplete command, not an export request.
 beginBuddyTransfer(false);Serial.output.clear();
 for(char c:std::string("EXPORT_BUDDY"))Serial.input.push_back(c);
 while(Serial.available())updateBuddySerial();
 assert(Serial.output.empty() && buddyTransferStatus()==BuddyTransferStatus::NONE);
 Serial.input.push_back('\r');updateBuddySerial();assert(buddyTransferStatus()==BuddyTransferStatus::EXPORT_COMPLETE);
 // Central date boundaries rather than UTC midnight; DST follows system TZ.
 blank();fakeEpoch=1791316800; // 2026-10-06 20:00 UTC, 15:00 Central DST.
 observe(0,700,0);auto day=getBuddySave().lastObservedDate;
 observe(0,700,4*3600);assert(getBuddySave().lastObservedDate==day && getBuddySave().uniqueDaysObserved==1);
 observe(0,700,5*3600);assert(getBuddySave().lastObservedDate!=day && getBuddySave().uniqueDaysObserved==2);
 puts("PASS: NVS power/reboot retention, birthday migration, guards/days/records/counts/discovery/all gear rules, retries/fallback, future-version protection, JSON roundtrip/validation, staged confirmed import and bounded Serial commands.");
}

#endif
