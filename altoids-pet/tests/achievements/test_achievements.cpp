#define FWF_RICH_WEATHER_TEST
#include "../journal/test_journal.cpp"
void invalidateWeatherLocation(){invalidations++;}
#include "../../firmware/altoids_pet/field_events.cpp"
uint32_t bit(AchievementId id){return 1UL<<uint8_t(id);}
void resetAchievementFixture(BuddySaveData &s){s.unlockedAchievements=0;for(auto &at:s.achievementUnlockedAt)at=0;}
int main(){
 setenv("TZ","CST6CDT,M3.2.0/2,M11.1.0/2",1);tzset();blank();
 assert(getBuddySave().unlockedAchievements==0 && takeNewAchievement()==AchievementId::COUNT);
 // Exact milestone boundaries, no dependence on units/display rounding.
 BuddySaveData s;s.createdAt=1800000000;s.latestObservationAt=s.createdAt+3600;s.totalObservations=1;
 s.highestTemperatureMilliC=20000;s.lowestTemperatureMilliC=10000;
 assert(eligibleAchievements(s)==bit(AchievementId::FIRST_NOTES));
 for(auto id:{AchievementId::FIELD_10,AchievementId::FIELD_50,AchievementId::FIELD_100}){
  unsigned threshold=id==AchievementId::FIELD_10?10:id==AchievementId::FIELD_50?50:100;
  s.totalObservations=threshold-1;assert(!(eligibleAchievements(s)&bit(id)));
  s.totalObservations=threshold;assert(eligibleAchievements(s)&bit(id));
 }
 for(unsigned n=3;n<=8;n++){
  s.discoveredWeather=(1UL<<n)-1;auto mask=eligibleAchievements(s);
  assert(bool(mask&bit(AchievementId::WEATHER_4))==(n>=4));
  assert(bool(mask&bit(AchievementId::WEATHER_6))==(n>=6));
  assert(bool(mask&bit(AchievementId::WEATHER_ALL_CORE))==(n==8));
 }
 for(auto units:{UnitsId::US,UnitsId::METRIC}){
  s.units=units;s.highestTemperatureMilliC=37777;assert(!(eligibleAchievements(s)&bit(AchievementId::CENTURY)));
  s.highestTemperatureMilliC=37778;assert(eligibleAchievements(s)&bit(AchievementId::CENTURY));
  s.lowestTemperatureMilliC=1;assert(!(eligibleAchievements(s)&bit(AchievementId::FREEZING)));
  s.lowestTemperatureMilliC=0;assert(eligibleAchievements(s)&bit(AchievementId::FREEZING));
  s.lowestTemperatureMilliC=-17777;assert(!(eligibleAchievements(s)&bit(AchievementId::DEEP_FREEZE)));
  s.lowestTemperatureMilliC=-17778;assert(eligibleAchievements(s)&bit(AchievementId::DEEP_FREEZE));
 }
 for(unsigned n=2;n<=8;n++){
  s.fieldSitesVisited=n;auto mask=eligibleAchievements(s);
  assert(bool(mask&bit(AchievementId::FIELD_SITE_3))==(n>=3));
  assert(bool(mask&bit(AchievementId::FIELD_SITE_5))==(n>=5));
  assert(bool(mask&bit(AchievementId::FIELD_SITE_8))==(n>=8));
 }
 resetAchievementFixture(s);uint32_t added=evaluateAchievements(s,s.latestObservationAt);
 assert(added==eligibleAchievements(s));auto firstAt=s.achievementUnlockedAt[0];
 assert(!evaluateAchievements(s,s.latestObservationAt) && s.achievementUnlockedAt[0]==firstAt);
 observe(0,1000);assert(achievementUnlocked(getBuddySave(),AchievementId::FIRST_NOTES));
 assert(achievementUnlocked(getBuddySave(),AchievementId::CENTURY));
 assert(takeNewAchievement()==AchievementId::FIRST_NOTES && takeNewAchievement()==AchievementId::CENTURY && takeNewAchievement()==AchievementId::COUNT);
 auto writes=nvsWrites;assert(!recordWeatherObservation({fakeEpoch+59,20000,0,WeatherCategory::CLEAR}));assert(nvsWrites==writes);
 assert(takeNewAchievement()==AchievementId::COUNT);
 ActiveFieldAlert alerts[]={{FieldEventId::TORNADO_WARNING,100},{FieldEventId::HURRICANE_WARNING,101},
  {FieldEventId::BLIZZARD_WARNING,102},{FieldEventId::FLASH_FLOOD_WARNING,103}};
 assert(collectFieldAlerts(alerts,4,fakeEpoch));assert(nvsWrites==writes+1);
 const AchievementId severe[]={AchievementId::FIRST_SEVERE,AchievementId::TORNADO_WARNING_SEEN,
  AchievementId::HURRICANE_WARNING_SEEN,AchievementId::BLIZZARD_WARNING_SEEN,AchievementId::FLASH_FLOOD_WARNING_SEEN};
 for(auto id:severe){assert(achievementUnlocked(getBuddySave(),id));assert(takeNewAchievement()==id);}
 assert(takeNewAchievement()==AchievementId::COUNT);writes=nvsWrites;assert(collectFieldAlerts(alerts,4,fakeEpoch));
 assert(nvsWrites==writes && takeNewAchievement()==AchievementId::COUNT);
 for(int i=1;i<8;i++){assert(upsertFieldLocation(i,"FIELD SITE",30+i,-90) && activateFieldLocation(i));observe(0,700);}
 assert(getBuddySave().fieldSitesVisited==8 && achievementUnlocked(getBuddySave(),AchievementId::FIELD_SITE_8));
 auto beforeDeletion=getBuddySave().unlockedAchievements;assert(deleteFieldLocation(0));
 assert(getBuddySave().unlockedAchievements==beforeDeletion); // Permanent travel progress.
 auto old=getBuddySave();auto json=exported();BuddySaveData restored;const char *error;
 assert(deserializeBuddySave(json.c_str(),json.size(),restored,error));assert(buddySaveChecksum(restored)==buddySaveChecksum(old));
 initializeJournal();assert(buddySaveChecksum(getBuddySave())==buddySaveChecksum(old) && takeNewAchievement()==AchievementId::COUNT);
 // Freeze the previous v9 payload, including the ring buffer, for migration.
 auto legacy=old;legacy.saveVersion=9;uint8_t payload[V9_PAYLOAD_BYTES];encodeCurrent(legacy,payload);
 encodeEvents(legacy,payload+V5_PAYLOAD_BYTES);encodeLocations(legacy,payload+V6_PAYLOAD_BYTES);
 encodeVariants(legacy,payload+V7_PAYLOAD_BYTES);encodeNotes(legacy,payload+V8_PAYLOAD_BYTES);
 std::vector<uint8_t> envelope(HEADER_BYTES+sizeof(payload));uint8_t *p=envelope.data();
 put32(p,SAVE_MAGIC);put32(p,9);put32(p,sizeof(payload));put64(p,100);
 memcpy(envelope.data()+24,payload,sizeof(payload));put32(p,hashBytes(payload,sizeof(payload),hashBytes(envelope.data(),20)));
 storage.clear();storage["fwf-buddy"]["save0"]=envelope;initializeJournal();
 assert(dirty && v9Checksum(getBuddySave())==v9Checksum(old));assert(getBuddySave().unlockedAchievements==eligibleAchievements(old));
 for(uint8_t i=0;i<ACHIEVEMENT_COUNT;i++)if(achievementUnlocked(getBuddySave(),AchievementId(i)))assert(getBuddySave().achievementUnlockedAt[i]==old.latestObservationAt);
 assert(takeNewAchievement()==AchievementId::COUNT);checkpointJournal();initializeJournal();assert(!dirty);
 JsonDocument doc;assert(!deserializeJson(doc,json));doc["saveVersion"]=9;doc.remove("achievements");
 char hash[9];snprintf(hash,9,"%08lx",(unsigned long)v9Checksum(old));doc["checksum"]=hash;
 std::string backup;serializeJson(doc,backup);assert(deserializeBuddySave(backup.c_str(),backup.size(),restored,error));
 assert(v9Checksum(restored)==v9Checksum(old) && restored.unlockedAchievements==eligibleAchievements(old));
 for(int scenario=0;scenario<9;scenario++){
  assert(!deserializeJson(doc,json));auto a=doc["achievements"][0];
  if(scenario==0)a["id"]=ACHIEVEMENT_COUNT;
  if(scenario==1)doc["achievements"][1]["id"]=0;
  if(scenario==2)a["unlockedAt"]=-1;
  if(scenario==3)a["unlockedAt"]=old.latestObservationAt+1;
  if(scenario==4)a["unlockedAt"]=old.createdAt-1;
  if(scenario==5)a["unlocked"]=1;
  if(scenario==6)doc["achievements"].remove(0);
  if(scenario==7)a["unlocked"]=false; // Nonzero timestamp on locked entry.
  if(scenario==8){doc["achievements"][uint8_t(AchievementId::DEEP_FREEZE)]["unlocked"]=true;doc["achievements"][uint8_t(AchievementId::DEEP_FREEZE)]["unlockedAt"]=old.latestObservationAt;}
  backup.clear();serializeJson(doc,backup);auto current=buddySaveChecksum(getBuddySave());
  assert(!importBuddy(backup.c_str(),backup.size()) && buddySaveChecksum(getBuddySave())==current);
 }
 auto invalid=old;invalid.unlockedAchievements|=bit(AchievementId::DEEP_FREEZE);invalid.achievementUnlockedAt[uint8_t(AchievementId::DEEP_FREEZE)]=old.latestObservationAt;
 assert(!validateBuddySave(invalid));
 puts("PASS: all 18 achievement thresholds/IDs, canonical temperatures, multiple/no duplicate awards, travel/deletion, severe batches, v9 NVS/JSON reconstruction, retained notes/progress and import validation.");
}
