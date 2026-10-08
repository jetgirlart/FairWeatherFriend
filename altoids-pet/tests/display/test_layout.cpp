#define FWF_LAYOUT_TEST
#include "test_display.cpp"
#include "weather.h"
#include "journal.h"
#include "gear.h"
#include "display.h"
bool timeValid=true,weatherValid=true,sunTimesValid=true;
int currentHour=12,currentMinute=34,sunriseHour=7,sunriseMinute=12,sunsetHour=18,sunsetMinute=42;
int temperatureF=72;
MoonPhase currentMoonPhase=MOON_FULL;
WeatherState weatherState=WEATHER_CLEAR;
bool isDaylight(){return currentHour>=7 && currentHour<19;}
const char *weatherName(){const char *names[]={"UNKNOWN","CLEAR","MAINLY CLEAR","PARTLY CLOUDY","CLOUDY","RAIN","STORM","SNOW","FOG"};return names[weatherState];}
const char *moonPhaseName(){return "FULL MOON";}
void soundPetInteraction(){}
bool handleTimerButtons(bool,bool,bool){return false;}
void openTimerSetup(){}
BuddySaveData buddy;
const BuddySaveData &getBuddySave(){return buddy;}
bool journalAvailable(){return true;}
uint32_t buddySaveChecksum(const BuddySaveData&){return 123;}
bool equipJournalGear(GearSlot slot,GearId gear){buddy.equippedSlots[static_cast<uint8_t>(slot)]=gear;return true;}
#include "../../firmware/altoids_pet/gear.cpp"
#include "../../firmware/altoids_pet/gear_sprites.cpp"
#include "../../firmware/altoids_pet/gear_overlay.cpp"
#include "../../firmware/altoids_pet/pet.cpp"
#include "../../firmware/altoids_pet/journal_ui.cpp"
#include "../../firmware/altoids_pet/display.cpp"
void snapshot(const char *name){
 char path[100];snprintf(path,sizeof(path),"/tmp/fwf-tft-%s.ppm",name);
 FILE *out=fopen(path,"wb");assert(out);fprintf(out,"P6\n240 240\n255\n");
 for(int i=0;i<240*240;i++){
   uint16_t c=display.getBuffer()[i];uint8_t rgb[]={uint8_t(((c>>11)&31)*255/31),uint8_t(((c>>5)&63)*255/63),uint8_t((c&31)*255/31)};
   fwrite(rgb,1,3,out);
 }fclose(out);
}
int main(){
 initializeDisplayBus();initializeDisplay();initializePetState();initializeAnimations();
 buddy.unlockedGear=0x7f;
 const GearId gear[]={GearId::FIELD_CAP,GearId::SUNGLASSES,GearId::WINTER_SCARF,GearId::RAINCOAT,GearId::BOOTS,GearId::UMBRELLA};
 for(int i=0;i<6;i++)buddy.equippedSlots[i]=gear[i];
 for(int i=0;i<=8;++i){weatherState=static_cast<WeatherState>(i);currentScreen=HOME;drawHome();assert(panel==std::vector<uint16_t>(display.getBuffer(),display.getBuffer()+57600));}
 weatherState=WEATHER_RAIN;drawHome();snapshot("home");
 int previous=windows;drawHome();assert(windows==previous); // Static frame no transfer.
 currentMinute++;drawHome();assert(windows>previous && windows-previous<15);
 for(auto action:{IdleAction::LOOK_LEFT,IdleAction::LOOK_RIGHT,IdleAction::BOUNCE,IdleAction::EAR_TWITCH,IdleAction::UMBRELLA,IdleAction::STORM_CROUCH,IdleAction::SNOW_SHIVER}) {
   idleAction=action;reactionWeather=weatherState=action==IdleAction::SNOW_SHIVER?WEATHER_SNOW:action==IdleAction::STORM_CROUCH?WEATHER_STORM:WEATHER_RAIN;
   reactionDaylight=isDaylight();idleStep=1;nextBlinkTime=idleBlinkDeadline;drawHome();
 }
 currentHour=23;drawHome();snapshot("night");
 weatherState=WEATHER_CLEAR;drawWeatherScreen();snapshot("weather");
 currentScreen=MENU;drawMenu();snapshot("menu");
 for(auto duration:{5,10,15,25})drawTimerSetup(duration);snapshot("timer");
 currentHour=12;currentScreen=FOCUS_SCREEN;drawFocusTimer(1499);snapshot("focus");
 currentScreen=TIMER_DONE;drawTimerDone(0);snapshot("done");
 openJournalScreen(GEAR_SCREEN);snapshot("gear");
 handleJournalButtons(false,true,false);snapshot("slot");
 buddy.createdAt=1800000000;buddy.totalObservations=UINT64_MAX;buddy.uniqueDaysObserved=999;
 buddy.discoveredWeather=0xff;buddy.latestObservationAt=1800000000;buddy.latestCategory=WeatherCategory::RAIN;
 buddy.latestTemperatureDeciF=721;buddy.highestTemperatureDeciF=1082;buddy.lowestTemperatureDeciF=-142;
 buddy.highestTemperatureAt=buddy.lowestTemperatureAt=1800000000;
 for(auto &count:buddy.weatherCounts)count=UINT64_MAX;
 openJournalScreen(JOURNAL_SCREEN);snapshot("journal");
 handleJournalButtons(true,false,false);snapshot("latest");
 handleJournalButtons(true,false,false);snapshot("counts");
 openJournalScreen(RECORDS_SCREEN);snapshot("records");
 puts("PASS: physical TFT compositing, all weather and combined gear, idle poses, sleep, minute-only partial updates, menus/timer/journal/records/gear layouts; previews saved in /tmp/fwf-tft-*.ppm.");
}
