#define FWF_RICH_WEATHER_TEST
#include "../journal/test_journal.cpp"
#include "../../firmware/altoids_pet/weather_observation.cpp"
#include "../../firmware/altoids_pet/units.h"
int temperatureF=70, weatherCode=0;
int32_t temperatureMilliC=21111;
bool weatherValid=true,sunTimesValid=true;
WeatherState weatherState=WEATHER_CLEAR;
int sunriseHour=7,sunriseMinute=12,sunsetHour=18,sunsetMinute=42;
double configuredLatitude(){return 32;}
double configuredLongitude(){return -95;}
#include "../../firmware/altoids_pet/weather_cache.cpp"
WeatherObservation parsed(const char *text) {
 JsonDocument doc;assert(!deserializeJson(doc,text));WeatherObservation out;
 assert(parseLiveObservation(doc.as<JsonVariantConst>(),fakeEpoch,out));return out;
}
void capture(WeatherObservation out) {
 fakeEpoch+=3600;out.timestamp=fakeEpoch;assert(recordWeatherObservation(out));
}
int main(){
 setenv("TZ","CST6CDT,M3.2.0/2,M11.1.0/2",1);tzset();blank();
 int32_t temperature;assert(!latestJournalTemperature(temperature));WeatherRecord record;assert(!journalRecord(RecordId::WIND,record));
 auto complete=parsed(R"({"temperature_2m":-0.1,"weather_code":61,"relative_humidity_2m":80,"wind_speed_10m":32.18,"wind_gusts_10m":64.37,"surface_pressure":1013.25,"precipitation":25.4})");
 assert(complete.temperatureMilliC==-100 && complete.metrics.validMask==31);
 assert(complete.metrics.values[0]==8000 && complete.metrics.values[4]==2540);
 capture(complete);auto first=getBuddySave();
 assert(first.unlockedGear & gearFlag(GearId::BOOTS));
 for(auto &r:first.records)assert(r.timestamp==fakeEpoch);
 assert(latestJournalMetric(MetricId::WIND,temperature) && temperature==3218);
 assert(latestJournalTemperature(temperature) && temperature==-100);
 auto worse=complete;worse.metrics.values[0]=7000;worse.metrics.values[1]=2000;worse.metrics.values[2]=4000;
 worse.metrics.values[3]=100000;worse.metrics.values[4]=100;capture(worse);
 auto next=getBuddySave();assert(next.records[0].value==3218 && next.records[1].value==6437 && next.records[2].value==8000);
 assert(next.records[3].value==100000 && next.records[4].value==101325 && next.records[5].value==2540);
 auto missing=parsed(R"({"temperature_2m":20,"weather_code":0,"relative_humidity_2m":null,"wind_speed_10m":"12","wind_gusts_10m":-1,"surface_pressure":0,"precipitation":false})");
 assert(missing.metrics.validMask==0);capture(missing);auto afterMissing=getBuddySave();
 for(int i=0;i<RECORD_COUNT;++i)assert(afterMissing.records[i].value==next.records[i].value && afterMissing.records[i].timestamp==next.records[i].timestamp);
 assert(!latestJournalMetric(MetricId::WIND,temperature));
 auto equal=complete;capture(equal);assert(getBuddySave().records[0].timestamp==first.records[0].timestamp);
 assert(getBuddySave().records[4].timestamp==first.records[4].timestamp);
 auto stronger=complete;stronger.metrics.values[1]=5000;stronger.metrics.values[2]=9000;stronger.metrics.values[0]=10000;
 stronger.metrics.values[3]=110000;stronger.metrics.values[4]=5000;capture(stronger);
 assert(getBuddySave().records[0].value==5000 && getBuddySave().records[1].value==9000 && getBuddySave().records[4].value==110000);
 // Canonical values and all records survive NVS and JSON; units only affect text.
 auto saved=getBuddySave();initializeJournal();assert(buddySaveChecksum(saved)==buddySaveChecksum(getBuddySave()));
 Print out;assert(serializeBuddySave(saved,out));BuddySaveData restored;const char *error;
 assert(deserializeBuddySave(out.output.c_str(),out.output.size(),restored,error));assert(buddySaveChecksum(saved)==buddySaveChecksum(restored));
 assert(saveBuddyUnits(UnitsId::US));char text[32];
 formatBuddyTemperature(0,text,sizeof(text));assert(std::string(text)=="32.0 F");
 formatBuddyTemperature(810,text,sizeof(text),false);assert(std::string(text)=="33 F");
 // Every supported legacy tenth Fahrenheit reverses exactly after migration.
 for(int32_t f=-2000;f<=2000;f++)assert(milliCToFahrenheitDeci(fahrenheitDeciToMilliC(f))==f);
 formatBuddyTemperature(-40000,text,sizeof(text));assert(std::string(text)=="-40.0 F");
 formatBuddyMetric(MetricId::WIND,1609,text,sizeof(text));assert(std::string(text)=="10.0 mph");
 formatBuddyMetric(MetricId::PRESSURE,101325,text,sizeof(text));assert(std::string(text)=="29.92 inHg");
 formatBuddyMetric(MetricId::PRECIPITATION,2540,text,sizeof(text));assert(std::string(text)=="1.00 in");
 assert(saveBuddyUnits(UnitsId::METRIC));formatBuddyTemperature(-100,text,sizeof(text));assert(std::string(text)=="-0.1 C");
 formatBuddyMetric(MetricId::WIND,1609,text,sizeof(text));assert(std::string(text)=="16.1 km/h");
 formatBuddyMetric(MetricId::PRESSURE,101325,text,sizeof(text));assert(std::string(text)=="1013.3 hPa");
 formatBuddyMetric(MetricId::PRECIPITATION,2540,text,sizeof(text));assert(std::string(text)=="25.40 mm");
 // Missing optionals do not turn into records; observed zero is explicitly valid.
 JsonDocument invalidRequired;WeatherObservation badObservation;
 assert(!deserializeJson(invalidRequired,R"({"temperature_2m":null,"weather_code":0})"));
 assert(!parseLiveObservation(invalidRequired.as<JsonVariantConst>(),fakeEpoch,badObservation));
 assert(!deserializeJson(invalidRequired,R"({"temperature_2m":"20","weather_code":0})"));
 assert(!parseLiveObservation(invalidRequired.as<JsonVariantConst>(),fakeEpoch,badObservation));
 auto zero=parsed(R"({"temperature_2m":20,"weather_code":0,"relative_humidity_2m":0,"wind_speed_10m":0,"wind_gusts_10m":0,"precipitation":0})");
 assert(zero.metrics.validMask==23 && zero.metrics.values[4]==0);
 // The real RTC cache save/restore path never creates observations or writes progress.
 saved=getBuddySave();auto writes=nvsWrites;saveCachedData();
 for(int i=0;i<100;++i){weatherValid=false;temperatureMilliC=0;initializeJournal();restoreCachedData();updateJournal();}
 assert(temperatureMilliC==21111 && weatherValid && sunTimesValid);
 assert(buddySaveChecksum(saved)==buddySaveChecksum(getBuddySave()) && nvsWrites==writes);
 // Migrating a real v4 envelope retains dates/settings/gear; added records are unset.
 auto old=saved;old.saveVersion=4;old.latestMetrics={};for(auto &r:old.records)r={};
 uint8_t payload[PAYLOAD_BYTES];encodeCurrent(old,payload);std::vector<uint8_t> envelope(24+196);
 uint8_t *header=envelope.data();put32(header,SAVE_MAGIC);put32(header,4);put32(header,196);put64(header,90);
 memcpy(envelope.data()+24,payload,196);header=envelope.data()+20;put32(header,hashBytes(payload,196,hashBytes(envelope.data(),20)));
 storage.clear();storage["fwf-buddy"]["save0"]=envelope;initializeJournal();
 assert(dirty && getBuddySave().totalObservations==saved.totalObservations && getBuddySave().highestTemperatureAt==saved.highestTemperatureAt);
 assert(getBuddySave().latestMetrics.validMask==0 && getBuddySave().records[0].timestamp==0);
 assert(milliCToFahrenheitDeci(getBuddySave().lowestTemperatureMilliC)==milliCToFahrenheitDeci(saved.lowestTemperatureMilliC));
 checkpointJournal(true);initializeJournal();assert(!dirty);
 // A v4 JSON backup retains its original checksum representation.
 JsonDocument legacy;assert(!deserializeJson(legacy,out.output));BuddySaveData backupState;assert(deserializeBuddySave(out.output.c_str(),out.output.size(),backupState,error));legacyTemperatures(legacy,backupState);legacy["saveVersion"]=4;
 char hash[9];snprintf(hash,9,"%08lx",(unsigned long)v4Checksum(backupState));legacy["checksum"]=hash;
 std::string json;serializeJson(legacy,json);assert(deserializeBuddySave(json.c_str(),json.size(),restored,error));
 assert(restored.records[0].timestamp==0 && restored.latestMetrics.validMask==0);
 // Reject corrupt new records and missing/invalid new JSON fields.
 for(int scenario=0;scenario<5;++scenario){
  JsonDocument bad;assert(!deserializeJson(bad,out.output));
  if(scenario==0)bad["records"]["strongestWind"]["timestamp"]=saved.latestObservationAt+1;
  if(scenario==1)bad["latestMetrics"]["humidityCentiPercent"]=10001;
  if(scenario==2)bad["records"]["lowestPressure"]["value"]=119999;
  if(scenario==3)bad["latestMetrics"].remove("windCentiKmh");
  if(scenario==4)bad["records"].remove("highestHumidity");
  std::string invalid;serializeJson(bad,invalid);assert(!deserializeBuddySave(invalid.c_str(),invalid.size(),restored,error));
 }
 puts("PASS: live optional parsing, valid zero vs missing, all metric/US conversions, monotonic/tied records, NVS/JSON v5 and v4 migration, queries, rejection and 100 actual RTC wakes without progress writes.");
}
