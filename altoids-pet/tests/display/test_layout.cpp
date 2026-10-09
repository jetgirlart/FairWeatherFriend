#define FWF_LAYOUT_TEST
#include "test_display.cpp"
#include "weather.h"
#include "journal.h"
#include "gear.h"
#include "journal.h"
#include "display.h"
bool timeValid=true,weatherValid=true,sunTimesValid=true;
int currentHour=12,currentMinute=34,sunriseHour=7,sunriseMinute=12,sunsetHour=18,sunsetMinute=42;
int temperatureF=72;
int32_t temperatureMilliC=22222;
MoonPhase currentMoonPhase=MOON_FULL;
WeatherState weatherState=WEATHER_CLEAR;
bool isDaylight(){return currentHour>=7 && currentHour<19;}
const char *weatherName(){const char *names[]={"UNKNOWN","CLEAR","MAINLY CLEAR","PARTLY CLOUDY","CLOUDY","RAIN","STORM","SNOW","FOG"};return names[weatherState];}
const char *moonPhaseName(){return "FULL MOON";}
void soundPetInteraction(){}
void stopSound(){}
double configuredLatitude(){return 32;}
double configuredLongitude(){return -95;}
bool saveBuddySound(bool){return true;}
bool saveBuddyUnits(UnitsId){return true;}
BuddyTransferStatus buddyTransferStatus(){return BuddyTransferStatus::NONE;}
uint32_t pendingBuddyImportChecksum(){return 0;}
void beginBuddyTransfer(bool){}
void endBuddyTransfer(){}
bool confirmBuddyImport(uint32_t){return false;}
bool handleTimerButtons(bool,bool,bool){return false;}
void openTimerSetup(){}
bool showTimerScreen(){return false;}
BuddySaveData buddy;
const BuddySaveData &getBuddySave(){return buddy;}
bool journalAvailable(){return true;}
FieldEventId queuedEvent=FieldEventId::COUNT;
FieldEventId takeNewFieldEvent(){auto id=queuedEvent;queuedEvent=FieldEventId::COUNT;return id;}
const char *fieldEventName(FieldEventId id){static const char *names[]={"Tornado Watch","Tornado Warning","Severe Thunderstorm Watch","Severe Thunderstorm Warning","Flash Flood Warning","Flood Warning","Hurricane Watch","Hurricane Warning","Tropical Storm Watch","Tropical Storm Warning","Winter Storm Warning","Blizzard Warning","Ice Storm Warning","Extreme Heat Warning","Extreme Cold Warning"};return names[uint8_t(id)];}
bool buddyNeedsSetup(){return !buddy.setupComplete;}
bool setupCommitFails=false;unsigned setupCommits=0;
bool confirmBuddySetup(FurPaletteId id){if(setupCommitFails)return false;buddy.furPalette=id;buddy.setupComplete=true;setupCommits++;return true;}
uint32_t buddySaveChecksum(const BuddySaveData&){return 123;}
bool equipJournalGear(GearSlot slot,GearId gear){buddy.equippedSlots[static_cast<uint8_t>(slot)]=gear;return true;}
#include "../../firmware/altoids_pet/gear.cpp"
#include "../../firmware/altoids_pet/gear_sprites.cpp"
#include "../../firmware/altoids_pet/gear_overlay.cpp"
#include "../../firmware/altoids_pet/pet.cpp"
#include "../../firmware/altoids_pet/journal_ui.cpp"
#include "../../firmware/altoids_pet/display.cpp"
#include "../../firmware/altoids_pet/buddy_setup.cpp"
#include "../../firmware/altoids_pet/settings_ui.cpp"
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
 currentScreen=HOME;queuedEvent=FieldEventId::SEVERE_THUNDERSTORM_WARNING;
 drawHome();snapshot("field-event");assert(panel==std::vector<uint16_t>(display.getBuffer(),display.getBuffer()+57600));
 fakeMillis+=2500;drawHome();
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
 for(int item=0;item<6;item++) {
   openSettings();for(int step=0;step<item;step++)handleSettingsButtons(true,false,false);
   char name[24];snprintf(name,sizeof(name),"settings-list-%d",item);snapshot(name);
   handleSettingsButtons(false,true,false);snprintf(name,sizeof(name),"settings-page-%d",item);snapshot(name);
   int pushed=windows;updateSettingsScreen();assert(windows==pushed);
 }
 openJournalScreen(GEAR_SCREEN);snapshot("gear");
 handleJournalButtons(false,true,false);snapshot("slot");
 buddy.createdAt=1800000000;buddy.totalObservations=UINT64_MAX;buddy.uniqueDaysObserved=999;
 buddy.discoveredWeather=0xff;buddy.latestObservationAt=1800000000;buddy.latestCategory=WeatherCategory::RAIN;
 buddy.latestTemperatureMilliC=fahrenheitDeciToMilliC(721);buddy.highestTemperatureMilliC=fahrenheitDeciToMilliC(1082);buddy.lowestTemperatureMilliC=fahrenheitDeciToMilliC(-142);
 buddy.highestTemperatureAt=buddy.lowestTemperatureAt=1800000000;
 for(auto &count:buddy.weatherCounts)count=UINT64_MAX;
 openJournalScreen(JOURNAL_SCREEN);snapshot("journal");
 handleJournalButtons(true,false,false);snapshot("latest");
 handleJournalButtons(true,false,false);snapshot("counts");
 openJournalScreen(RECORDS_SCREEN);snapshot("records");
 for(unsigned i=0;i<METRIC_COUNT;i++){buddy.latestMetrics.validMask|=1UL<<i;buddy.latestMetrics.values[i]=i==3?101325:2000;}
 for(unsigned i=0;i<RECORD_COUNT;i++)buddy.records[i]={i==3||i==4?101325:2000,1800000000};
 for(auto units:{UnitsId::US,UnitsId::METRIC}){
  buddy.units=units;openJournalScreen(RECORDS_SCREEN);
  for(int i=0;i<4;i++){char name[32];snprintf(name,sizeof(name),"records-%u-%d",uint8_t(units),i);snapshot(name);handleJournalButtons(true,false,false);}
  openJournalScreen(JOURNAL_SCREEN);
  for(int i=0;i<10;i++){char name[32];snprintf(name,sizeof(name),"journal-%u-%d",uint8_t(units),i);snapshot(name);handleJournalButtons(true,false,false);}
 }

 buddy.setupComplete=true;assert(!beginBuddySetup());
 buddy.setupComplete=false;assert(beginBuddySetup() && buddySetupActive());snapshot("welcome");
 unsigned commits=setupCommits;handleBuddySetupButtons(true,false,false);assert(page==SetupPage::WELCOME);
 handleBuddySetupButtons(false,true,false);assert(page==SetupPage::FUR);
 for(int id=0;id<5;++id){
   assert(highlighted==static_cast<FurPaletteId>(id));snapshot(furPaletteName(highlighted));
   handleBuddySetupButtons(true,false,false);
 }
 assert(highlighted==FurPaletteId::ORANGE && setupCommits==commits);
 handleBuddySetupButtons(false,false,true);assert(page==SetupPage::WELCOME);
 handleBuddySetupButtons(false,true,false);handleBuddySetupButtons(true,false,false);
 setupCommitFails=true;handleBuddySetupButtons(false,true,false);
 assert(saveFailed && page==SetupPage::FUR && !buddy.setupComplete && setupCommits==commits);
 setupCommitFails=false;handleBuddySetupButtons(false,true,false);assert(page==SetupPage::READY && buddy.furPalette==FurPaletteId::CREAM && setupCommits==commits+1);snapshot("ready");
 handleBuddySetupButtons(true,true,true);assert(setupCommits==commits+1 && page==SetupPage::READY);
 fakeMillis=readyAt+1599;updateBuddySetup();assert(buddySetupActive());
 fakeMillis++;updateBuddySetup();assert(currentScreen==HOME && !beginBuddySetup());
 // Importing a completed buddy during unfinished setup returns home without another save.
 buddy.setupComplete=false;assert(beginBuddySetup());buddy.setupComplete=true;updateBuddySetup();assert(currentScreen==HOME);
 puts("PASS: physical TFT compositing, all weather and combined gear, idle poses, sleep, minute-only partial updates, menus/timer/journal/records/gear layouts; previews saved in /tmp/fwf-tft-*.ppm.");
}
