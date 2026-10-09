#include <assert.h>
#include <map>
#include <vector>
#include <deque>
#include <string>
#include "Arduino.h"
#include "Preferences.h"
#include "weather.h"
void invalidateWeatherLocation() {}
SerialType Serial;
#include "palette.h"
PetPalette petPalette;
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
#include "../../firmware/altoids_pet/gear_variants.cpp"

#include "../../firmware/altoids_pet/journal_ui.cpp"
std::vector<TextRow> frameText;
unsigned frameClears=0,framePushes=0,discoveryDots=0;
DisplaySurface fakeDisplay;
DisplaySurface &display=fakeDisplay;
ScreenMode currentScreen=HOME;
int menuIndex=2;
void drawMenu() {display.clearDisplay();display.setTextSize(1);display.setCursor(0,0);display.print("MENU");display.display();}
bool shown(const char *text) {for(auto &row:frameText)if(row.value==text)return true;return false;}
void nextPage() {assert(handleJournalButtons(true,false,false));}
#ifndef FWF_SETTINGS_TEST
int main() {
 setenv("TZ","CST6CDT,M3.2.0/2,M11.1.0/2",1);tzset();
 initializeJournal();timeValid=false;checkpointJournal();unsigned writes=nvsWrites;
 openJournalScreen(JOURNAL_SCREEN);assert(shown("0") && shown("WAITING FOR TIME"));
 nextPage();assert(shown("NO OBSERVATIONS YET"));
 nextPage();assert(shown("NO OBSERVATIONS YET"));
 nextPage();assert(shown("CLEAR") && shown("CLOUDY") && discoveryDots==0);
 nextPage();assert(shown("RAIN") && shown("FOG"));nextPage();assert(shown("SEVERE WEATHER") && shown("--"));
 for(int i=0;i<5;++i)nextPage();assert(shown("FIELD NOTES") && shown("No field notes yet"));
 nextPage();assert(shown("SUMMARY 1/11"));
 assert(nvsWrites==writes);unsigned pushes=framePushes;
 for(int i=0;i<50;i++){fakeMillis+=250;updateJournalScreens();}assert(framePushes==pushes);
 assert(handleJournalButtons(false,false,true));assert(currentScreen==MENU && menuIndex==2);
 assert(!handleJournalButtons(true,true,true));
 openJournalScreen(RECORDS_SCREEN);assert(shown("NO OBSERVATIONS YET"));
 pushes=framePushes;handleJournalButtons(true,true,false);assert(framePushes==pushes+1 && nvsWrites==writes);openJournalScreen(RECORDS_SCREEN);
 timeValid=true;assert(recordWeatherObservation({fakeEpoch,fahrenheitDeciToMilliC(-1),71,WeatherCategory::SNOW}));
 fakeMillis+=250;updateJournalScreens();assert(shown("-0.1 F") && shown("HIGHEST") && shown("LOWEST"));
 openJournalScreen(JOURNAL_SCREEN);assert(shown("1") && shown("WEATHER TYPES 1/8"));
 nextPage();assert(shown("SNOW") && shown("-0.1 F"));
 nextPage();assert(shown("PRESSURE --"));nextPage();assert(discoveryDots==0);nextPage();assert(discoveryDots==1);
 // Field Notes: open newest, three detail pages, older wraps, C backs one level.
 openJournalScreen(JOURNAL_SCREEN);for(int i=0;i<10;i++)nextPage();
 assert(shown("FIELD NOTES") && shown("1 / 16 NOTES"));writes=nvsWrites;
 handleJournalButtons(false,true,false);assert(shown("NOTE 1/1 - 1/3") && shown("SNOW") && shown("HUM --"));
 handleJournalButtons(false,true,false);assert(shown("ATMOSPHERE") && shown("GUST --"));
 handleJournalButtons(false,true,false);assert(shown("NEW WEATHER TYPE") && shown("NEW GEAR UNLOCK"));
 nextPage();assert(shown("NOTE 1/1 - 3/3"));
 handleJournalButtons(false,true,false);assert(shown("NOTE 1/1 - 1/3"));
 pushes=framePushes;fakeMillis+=250;updateJournalScreens();assert(framePushes==pushes && nvsWrites==writes);
 handleJournalButtons(false,false,true);assert(currentScreen==JOURNAL_SCREEN && shown("B:OPEN"));
 handleJournalButtons(false,false,true);assert(currentScreen==MENU);
 openJournalScreen(GEAR_SCREEN);assert(shown("A:NEXT B:OPEN"));
 auto pressB=[](){handleJournalButtons(false,true,false);};
 auto pressC=[](){handleJournalButtons(false,false,true);};
 writes=nvsWrites;pressB();assert(shown("HEAD") && shown("NONE") && shown("EQUIPPED"));
 pressB();assert(nvsWrites==writes);nextPage();assert(shown("FIELD CAP"));
 pressB();assert(choosingVariant && shown("ORIGINAL") && nvsWrites==writes);
 pressB();assert(getBuddySave().equippedSlots[0]==GearId::FIELD_CAP && nvsWrites==writes+1);
 pressB();assert(nvsWrites==writes+1);pressC();assert(!choosingVariant && choosingItem);pressC();nextPage();pressB();
 nextPage();assert(shown("NONE")); // No FACE items unlocked.
 pressC();nextPage();pressB();nextPage();assert(shown("WINTER SCARF"));
 pressB();assert(choosingVariant && shown("ORIGINAL"));
 failWrite=true;pressB();assert(shown("SAVE FAILED") && getBuddySave().equippedSlots[2]==GearId::NONE);
 failWrite=false;pressB();assert(shown("EQUIPPED") && getBuddySave().equippedSlots[2]==GearId::WINTER_SCARF);
 assert(getBuddySave().equippedSlots[0]==GearId::FIELD_CAP);
 pressC();nextPage();pressB();assert(getBuddySave().equippedSlots[2]==GearId::NONE);
 initializeJournal();assert(getBuddySave().equippedSlots[0]==GearId::FIELD_CAP);
 openJournalScreen(GEAR_SCREEN);pressB();assert(shown("FIELD CAP") && shown("EQUIPPED"));
 // Successful import while a screen is open refreshes it once, with no polling writes.
 BuddySaveData imported;imported.createdAt=fakeEpoch;
 Print json;assert(serializeBuddySave(imported,json));
 assert(importBuddy(json.output.c_str(),json.output.size()));assert(confirmBuddyImport(buddySaveChecksum(imported)));
 fakeMillis+=250;updateJournalScreens();assert(shown("NONE")); // Open submenu reconciles selection after import.
 // Fully unlocked outfit: all slot transitions, BODY replacement and single-slot NONE.
 for(int i=0;i<10;++i){fakeEpoch+=3600;assert(recordWeatherObservation({fakeEpoch,fahrenheitDeciToMilliC(700),61,WeatherCategory::RAIN}));}
 for(int i=0;i<5;++i){fakeEpoch+=3600;assert(recordWeatherObservation({fakeEpoch,fahrenheitDeciToMilliC(310),71,WeatherCategory::SNOW}));}
 fakeEpoch+=3600;assert(recordWeatherObservation({fakeEpoch,fahrenheitDeciToMilliC(700),0,WeatherCategory::CLEAR}));
 openJournalScreen(GEAR_SCREEN);
 const GearId expected[]={GearId::FIELD_CAP,GearId::SUNGLASSES,GearId::WINTER_SCARF,
                          GearId::RAINCOAT,GearId::BOOTS,GearId::UMBRELLA};
 for(uint8_t slot=0;slot<6;++slot) {
   assert(selectedSlot==static_cast<GearSlot>(slot));pressB();nextPage();pressB();pressB();
   assert(getBuddySave().equippedSlots[slot]==expected[slot]);pressC();pressC();nextPage();
 }
 // HEAD wraps after PROP. Move to BODY and cycle raincoat -> winter coat.
 nextPage();nextPage();nextPage();pressB();nextPage();pressB();pressB();
 assert(getBuddySave().equippedSlots[3]==GearId::WINTER_COAT);
 for(uint8_t slot=0;slot<6;++slot)if(slot!=3)assert(getBuddySave().equippedSlots[slot]==expected[slot]);
 pressC();nextPage();pressB();assert(getBuddySave().equippedSlots[3]==GearId::NONE);
 pressC();pressC();assert(currentScreen==MENU);
 initializeJournal();for(uint8_t slot=0;slot<6;++slot)assert(getBuddySave().equippedSlots[slot]==(slot==3?GearId::NONE:expected[slot]));
 // Full-width counters, negative/large temperatures and record dates stay in bounds.
 buddy=BuddySaveData{};buddy.createdAt=fakeEpoch;buddy.totalObservations=UINT64_MAX;buddy.uniqueDaysObserved=UINT32_MAX;
 buddy.latestObservationAt=fakeEpoch;buddy.lastObservedDate=localDate(fakeEpoch);buddy.latestTemperatureMilliC=fahrenheitDeciToMilliC(-2000);
 buddy.latestWeatherCode=INT32_MAX;buddy.latestCategory=WeatherCategory::CLEAR;
 buddy.highestTemperatureMilliC=fahrenheitDeciToMilliC(2000);buddy.lowestTemperatureMilliC=fahrenheitDeciToMilliC(-2000);
 buddy.highestTemperatureAt=buddy.lowestTemperatureAt=fakeEpoch;
 buddy.weatherCounts[0]=UINT64_MAX;buddy.discoveredWeather=1;evaluateGearUnlocks(buddy);
 openJournalScreen(JOURNAL_SCREEN);assert(shown("18446744073709551615") && shown("DAYS 4294967295"));
 nextPage();assert(shown("CLEAR"));nextPage();nextPage();assert(shown("18446744073709551615"));
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

#endif
