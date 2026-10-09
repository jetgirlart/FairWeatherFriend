#define FWF_RICH_WEATHER_TEST
#define FWF_NETWORK_TEST
#include "../journal/test_journal.cpp"
#include "../../firmware/altoids_pet/weather_observation.cpp"
#include "../../firmware/altoids_pet/weather.cpp"
#include "../../firmware/altoids_pet/weather_cache.cpp"
#include "../../firmware/altoids_pet/field_events.cpp"
#include "../../firmware/altoids_pet/nws_client.cpp"
const char *weatherReply=R"({"current":{"temperature_2m":20,"weather_code":0},"daily":{"sunrise":["2027-01-15T07:00"],"sunset":["2027-01-15T18:00"]}})";
const char *pointReply=R"({"properties":{"forecastZone":"https://api.weather.gov/zones/forecast/TXZ001"}})";
const char *alertReply=R"({"features":[{"properties":{"id":"urn:nws:1","event":"Tornado Warning","description":"NOT STORED"}}]})";
void prepare(){blank();assert(saveBuddyLocation(32,-95));httpUrls.clear();httpHeaders.clear();httpReplies.clear();}
int main(){
 setenv("TZ","CST6CDT,M3.2.0/2,M11.1.0/2",1);tzset();
 prepare();httpReplies={{200,weatherReply},{200,pointReply},{200,alertReply}};
 assert(fetchWeather() && weatherValid && getBuddySave().totalObservations==1 && getBuddySave().fieldEvents[1].count==1);
 assert(httpUrls.size()==3 && httpUrls[2]=="https://api.weather.gov/alerts/active?point=32.000000,-95.000000");
 assert(httpHeaders[2]["User-Agent"]=="FairWeatherFriend/0.6.0 (https://github.com/jetgirlart/FairWeatherFriend)");
 assert(httpHeaders[2]["Accept"]=="application/geo+json");
 auto hash=buddySaveChecksum(getBuddySave());saveCachedData();WiFi.mode(WIFI_OFF);
 for(int i=0;i<100;++i)initializeWeather(true);
 assert(httpUrls.size()==3 && WiFi.currentMode==WIFI_OFF && buddySaveChecksum(getBuddySave())==hash);
 // HTTP / invalid JSON / oversized / coverage failures never reject live weather.
 for(int scenario=0;scenario<6;++scenario){
  prepare();httpReplies.push_back({200,weatherReply});
  if(scenario==0)httpReplies.push_back({503,"failure"});
  else if(scenario==1)httpReplies.push_back({404,"outside coverage"});
  else if(scenario==2)httpReplies.push_back({200,R"({"properties":{}})"});
  else {httpReplies.push_back({200,pointReply});
   if(scenario==3)httpReplies.push_back({500,"failure"});
   if(scenario==4)httpReplies.push_back({200,"not JSON"});
   if(scenario==5)httpReplies.push_back({200,alertReply,100000});
  }
  assert(fetchWeather() && weatherValid && getBuddySave().totalObservations==1 && getBuddySave().fieldEvents[1].count==0);
 }
 prepare();assert(saveBuddyLocation(51.5,-.1));httpReplies={{200,weatherReply}};
 assert(fetchWeather() && httpUrls.size()==1 && getBuddySave().totalObservations==1);
 // Border-region foreign location rejected by metadata, not bounding box guess.
 prepare();assert(saveBuddyLocation(43.65,-79.38));httpReplies={{200,weatherReply},{404,"outside coverage"}};
 assert(fetchWeather() && httpUrls.size()==2 && getBuddySave().totalObservations==1);
 prepare();httpReplies={{200,weatherReply},{200,pointReply},{200,alertReply}};assert(fetchWeather());
 httpReplies={{200,weatherReply}};assert(fetchWeather());assert(httpUrls.size()==4); // guard rejected: no NWS.
 prepare();httpReplies={{500,"fail"}};assert(!fetchWeather() && httpUrls.size()==1);
 puts("PASS: actual weather/NWS clients: correct endpoint/headers, supplemental failures, non-US skip, 100 cached wakes without NWS, rejected/failed observations without NWS.");
}
