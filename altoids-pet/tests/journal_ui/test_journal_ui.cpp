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

#include "../../firmware/altoids_pet/journal_ui.cpp"
std::vector<TextRow> frameText;
unsigned frameClears=0,framePushes=0,discoveryDots=0;
Adafruit_SH1107 fakeDisplay;
Adafruit_SH1107 &display=fakeDisplay;
ScreenMode currentScreen=HOME;
int menuIndex=2;
void drawMenu() {display.clearDisplay();display.setTextSize(1);display.setCursor(0,0);display.print("MENU");display.display();}
bool shown(const char *text) {for(auto &row:frameText)if(row.value==text)return true;return false;}
void nextPage() {assert(handleJournalButtons(true,false,false));}
int main() {
 setenv("TZ","CST6CDT,M3.2.0/2,M11.1.0/2",1);tzset();
 initializeJournal();timeValid=false;checkpointJournal();unsigned writes=nvsWrites;
 openJournalScreen(JOURNAL_SCREEN);assert(shown("0") && shown("WAITING FOR TIME"));
 nextPage();assert(shown("NO OBSERVATIONS YET"));
 nextPage();assert(shown("CLEAR") && shown("CLOUDY") && discoveryDots==0);
 nextPage();assert(shown("RAIN") && shown("FOG"));nextPage();assert(shown("SUMMARY 1/4"));
 assert(nvsWrites==writes);unsigned pushes=framePushes;
 for(int i=0;i<50;i++){fakeMillis+=250;updateJournalScreens();}assert(framePushes==pushes);
 assert(handleJournalButtons(false,false,true));assert(currentScreen==MENU && menuIndex==2);
 assert(!handleJournalButtons(true,true,true));
 openJournalScreen(RECORDS_SCREEN);assert(shown("NO OBSERVATIONS YET"));
 pushes=framePushes;handleJournalButtons(true,true,false);assert(framePushes==pushes && nvsWrites==writes);
 timeValid=true;assert(recordWeatherObservation({fakeEpoch,-1,71,WeatherCategory::SNOW}));
 fakeMillis+=250;updateJournalScreens();assert(shown("-0.1 F") && shown("HIGHEST") && shown("LOWEST"));
 openJournalScreen(JOURNAL_SCREEN);assert(shown("1") && shown("WEATHER TYPES 1/8"));
 nextPage();assert(shown("SNOW") && shown("CODE 71") && shown("-0.1 F"));
 nextPage();assert(discoveryDots==0);nextPage();assert(discoveryDots==1);
 openJournalScreen(GEAR_SCREEN);assert(shown("NONE") && shown("EQUIPPED"));
 writes=nvsWrites;handleJournalButtons(false,true,false);assert(nvsWrites==writes); // NONE already selected.
 nextPage();assert(shown("FIELD CAP") && shown("UNLOCKED"));
 handleJournalButtons(false,true,false);assert(getBuddySave().equippedGear==GearId::FIELD_CAP && nvsWrites==writes+1 && shown("EQUIPPED"));
 handleJournalButtons(false,true,false);assert(nvsWrites==writes+1); // No duplicate equipment write.
 nextPage();assert(shown("SUNGLASSES") && shown("LOCKED"));
 handleJournalButtons(false,true,false);assert(nvsWrites==writes+1 && getBuddySave().equippedGear==GearId::FIELD_CAP);
 nextPage();assert(shown("UMBRELLA") && shown("LOCKED"));
 nextPage();assert(shown("RAINCOAT") && shown("10 rain observations"));
 nextPage();assert(shown("WINTER SCARF") && shown("UNLOCKED"));
 failWrite=true;handleJournalButtons(false,true,false);assert(shown("SAVE FAILED") && getBuddySave().equippedGear==GearId::FIELD_CAP);
 failWrite=false;handleJournalButtons(false,true,false);assert(shown("EQUIPPED") && getBuddySave().equippedGear==GearId::WINTER_SCARF);
 nextPage();assert(shown("WINTER COAT") && shown("LOCKED"));
 nextPage();assert(shown("BOOTS") && shown("UNLOCKED"));
 nextPage();handleJournalButtons(false,true,false);assert(getBuddySave().equippedGear==GearId::NONE);
 initializeJournal();assert(getBuddySave().equippedGear==GearId::NONE);
 openJournalScreen(GEAR_SCREEN);nextPage();handleJournalButtons(false,true,false);
 initializeJournal();assert(getBuddySave().equippedGear==GearId::FIELD_CAP);
 openJournalScreen(GEAR_SCREEN);assert(shown("FIELD CAP") && shown("EQUIPPED"));
 // Successful import while a screen is open refreshes it once, with no polling writes.
 BuddySaveData imported;imported.createdAt=fakeEpoch;
 Print json;assert(serializeBuddySave(imported,json));
 assert(importBuddy(json.output.c_str(),json.output.size()));assert(confirmBuddyImport(buddySaveChecksum(imported)));
 fakeMillis+=250;updateJournalScreens();assert(shown("LOCKED")); // Field cap not unlocked in replacement buddy.
 // Full-width counters, negative/large temperatures and record dates stay in bounds.
 buddy=BuddySaveData{};buddy.createdAt=fakeEpoch;buddy.totalObservations=UINT64_MAX;buddy.uniqueDaysObserved=UINT32_MAX;
 buddy.latestObservationAt=fakeEpoch;buddy.lastObservedDate=localDate(fakeEpoch);buddy.latestTemperatureDeciF=-2000;
 buddy.latestWeatherCode=INT32_MAX;buddy.latestCategory=WeatherCategory::CLEAR;
 buddy.highestTemperatureDeciF=2000;buddy.lowestTemperatureDeciF=-2000;
 buddy.highestTemperatureAt=buddy.lowestTemperatureAt=fakeEpoch;
 buddy.weatherCounts[0]=UINT64_MAX;buddy.discoveredWeather=1;evaluateGearUnlocks(buddy);
 openJournalScreen(JOURNAL_SCREEN);assert(shown("18446744073709551615") && shown("DAYS 4294967295"));
 nextPage();assert(shown("CODE 2147483647"));nextPage();assert(shown("18446744073709551615"));
 openJournalScreen(RECORDS_SCREEN);assert(shown("200.0 F") && shown("-200.0 F"));
 // Protected storage clearly shown in all screens, no writes from A/B.
 available=false;writes=nvsWrites;
 for(auto screen:{JOURNAL_SCREEN,RECORDS_SCREEN,GEAR_SCREEN}) {
  openJournalScreen(screen);assert(shown("JOURNAL UNAVAILABLE") && shown("SAVE PRESERVED"));
  handleJournalButtons(true,true,false);assert(nvsWrites==writes);
  handleJournalButtons(false,false,true);assert(currentScreen==MENU);
 }
 currentScreen=HOME;pushes=framePushes;fakeMillis+=1000;updateJournalScreens();assert(pushes==framePushes);
 assert(frameClears==framePushes);
 puts("PASS: Journal pages, dated Records, all Gear choices/locks/equip/unequip/save errors, NVS restoration, import refresh, bounded text, protected storage, navigation and one update per changed frame.");
}
