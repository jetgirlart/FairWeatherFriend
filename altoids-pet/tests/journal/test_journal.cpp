#include <assert.h>
#include <map>
#include <vector>
#include <deque>
#include <string>
#include "Arduino.h"
#include "Preferences.h"
#include "weather.h"
SerialType Serial;
unsigned long fakeMillis=0;
bool timeValid=true;
time_t fakeEpoch=1800000000;
extern "C" time_t time(time_t *p) { if(p)*p=fakeEpoch; return fakeEpoch; }
std::map<std::string,Entries> storage;
unsigned nvsWrites=0;
bool failOpen=false,failWrite=false,corruptWrite=false;
// Weather interface fake: the real mapping is preserved by the refactor.
WeatherState mapWeatherCode(int code) {
 switch(code) {
  case 0:return WEATHER_CLEAR; case 1:return WEATHER_MAINLY_CLEAR;
  case 2:return WEATHER_PARTLY_CLOUDY; case 3:return WEATHER_CLOUDY;
  case 45:return WEATHER_FOG; case 61:return WEATHER_RAIN;
  case 71:return WEATHER_SNOW; case 95:return WEATHER_STORM;
  default:return WEATHER_UNKNOWN;
 }
}
#include "../../firmware/altoids_pet/save.cpp"
#include "../../firmware/altoids_pet/journal.cpp"
#include "../../firmware/altoids_pet/gear.cpp"
void blank() { storage.clear();nvsWrites=0;failOpen=failWrite=corruptWrite=false;fakeMillis=0;fakeEpoch=1800000000;timeValid=true;Serial.input.clear();initializeJournal(); }
void observe(int code=0,int temperature=700,int seconds=3600) {
 fakeEpoch+=seconds;fakeMillis+=seconds*1000;
 assert(recordWeatherObservation({fakeEpoch,temperature,code,observationCategoryForCode(code)}));
}
std::string exported() { Print p;assert(exportBuddy(p));return p.output; }
void send(const std::string &line) { for(char c:line)Serial.input.push_back(c);Serial.input.push_back('\n');while(Serial.available())updateBuddySerial(); }
int main() {
 setenv("TZ","CST6CDT,M3.2.0/2,M11.1.0/2",1);tzset();
 blank(); checkpointJournal();assert(nvsWrites==1 && getBuddySave().createdAt==fakeEpoch);
 int64_t birth=getBuddySave().createdAt;
 observe();assert(getBuddySave().totalObservations==1 && getBuddySave().uniqueDaysObserved==1);
 assert(getBuddySave().unlockedGear==(gearFlag(GearId::FIELD_CAP)|gearFlag(GearId::SUNGLASSES)));
 assert(getBuddySave().highestTemperatureAt==fakeEpoch && getBuddySave().lowestTemperatureAt==fakeEpoch);
 unsigned writes=nvsWrites;
 for(int i=0;i<50;i++)updateJournal();assert(nvsWrites==writes);
 // Deep-sleep/cold reboot loads permanent progress; reopening or same live timestamp does not award.
 initializeJournal();assert(getBuddySave().createdAt==birth && getBuddySave().totalObservations==1);
 assert(!recordWeatherObservation({fakeEpoch+59,999,0,WeatherCategory::CLEAR}));assert(nvsWrites==writes);
 assert(!recordWeatherObservation({fakeEpoch-10,999,0,WeatherCategory::CLEAR}));
 assert(!recordWeatherObservation({fakeEpoch+60,999,0,WeatherCategory::SNOW}));
 observe(61,650);assert(getBuddySave().uniqueDaysObserved==1 && getBuddySave().lowestTemperatureDeciF==650);
 int64_t lowAt=getBuddySave().lowestTemperatureAt;
 observe(1,800,86400);assert(getBuddySave().uniqueDaysObserved==2 && getBuddySave().highestTemperatureDeciF==800);
 observe(3,650);assert(getBuddySave().lowestTemperatureAt==lowAt);
 observe(95,550);assert(getBuddySave().weatherCounts[5]==1); // Ties retain first record date.
 for(int i=1;i<10;i++)observe(61,600);
 assert(getBuddySave().weatherCounts[4]==10 && (getBuddySave().unlockedGear&gearFlag(GearId::RAINCOAT)));
 for(int i=0;i<5;i++)observe(71,310);
 assert(getBuddySave().weatherCounts[6]==5 && (getBuddySave().unlockedGear&gearFlag(GearId::WINTER_COAT)));
 assert(getBuddySave().unlockedGear&gearFlag(GearId::BOOTS));
 assert(!equipGear(static_cast<GearId>(99))); assert(equipGear(GearId::FIELD_CAP));
 writes=nvsWrites; assert(equipGear(GearId::FIELD_CAP)&&nvsWrites==writes);
 assert(equipGear(GearId::NONE));
 auto saved=getBuddySave();std::string json=exported();
 BuddySaveData restored;const char *error=nullptr;
 assert(deserializeBuddySave(json.c_str(),json.size(),restored,error));
 assert(buddySaveChecksum(saved)==buddySaveChecksum(restored));
 // JSON corruption, missing/type-invalid fields, inconsistent counters and unsupported versions.
 JsonDocument doc;assert(!deserializeJson(doc,json));doc["totalObservations"]=999;
 std::string bad;serializeJson(doc,bad);assert(!deserializeBuddySave(bad.c_str(),bad.size(),restored,error));
 assert(!deserializeJson(doc,json));doc["saveVersion"]=2;bad.clear();serializeJson(doc,bad);
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
 invalid=saved;invalid.equippedGear=static_cast<GearId>(9);assert(!validateBuddySave(invalid));
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
 storage["fwf-buddy"][latestKey][4]=2;auto protectedStorage=storage;
 initializeJournal();assert(!journalAvailable());
 assert(!recordWeatherObservation({fakeEpoch+3600,700,0,WeatherCategory::CLEAR}));
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
 // Unknown legacy version is preserved, not interpreted as a new buddy.
 blank();storage.clear();old.record.version=2;
 storage["altoids-pet"]["state"]=std::vector<uint8_t>((uint8_t*)&old.record,(uint8_t*)&old.record+sizeof(old.record));
 initializeJournal();assert(!journalAvailable());
 // Storage unavailable doesn't overwrite an unread save.
 blank();checkpointJournal();protectedStorage=storage;failOpen=true;initializeJournal();assert(!journalAvailable());checkpointJournal();assert(storage==protectedStorage);failOpen=false;
 // Serial commands are bounded and JSON must be complete, with explicit confirmation.
 blank();observe();saved=getBuddySave();json=exported();
 assert(!deserializeJson(doc,json));std::string compact;serializeJson(doc,compact);
 writes=nvsWrites;send("IMPORT_BUDDY "+compact);assert(nvsWrites==writes && importPending);
 char command[64];snprintf(command,sizeof(command),"CONFIRM_IMPORT %08lx",(unsigned long)buddySaveChecksum(saved));
 send(command);assert(nvsWrites==writes+1);
 writes=nvsWrites;send(std::string(4200,'x'));assert(nvsWrites==writes && !importPending);
 Serial.output.clear();send("EXPORT_BUDDY");assert(Serial.output.find("researchBeganAt")!=std::string::npos);
 // Central date boundaries rather than UTC midnight; DST follows system TZ.
 blank();fakeEpoch=1791316800; // 2026-10-06 20:00 UTC, 15:00 Central DST.
 observe(0,700,0);auto day=getBuddySave().lastObservedDate;
 observe(0,700,4*3600);assert(getBuddySave().lastObservedDate==day && getBuddySave().uniqueDaysObserved==1);
 observe(0,700,5*3600);assert(getBuddySave().lastObservedDate!=day && getBuddySave().uniqueDaysObserved==2);
 puts("PASS: NVS power/reboot retention, birthday migration, guards/days/records/counts/discovery/all gear rules, retries/fallback, future-version protection, JSON roundtrip/validation, staged confirmed import and bounded Serial commands.");
}
