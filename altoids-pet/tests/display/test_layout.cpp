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
#include "../../firmware/altoids_pet/achievements.cpp"
#include "../../firmware/altoids_pet/sound.h"
void queueSoundEvent(SoundEvent){}
AchievementId queuedAchievement=AchievementId::COUNT;
AchievementId takeNewAchievement(){auto id=queuedAchievement;queuedAchievement=AchievementId::COUNT;return id;}
void soundPetInteraction(){}
void stopSound(){}
double configuredLatitude(){return 32;}
double configuredLongitude(){return -95;}
extern BuddySaveData buddy;
bool activateFieldLocation(uint8_t id){if(id>=FIELD_LOCATION_COUNT || !buddy.locations[id].used)return false;buddy.activeLocation=id;return true;}
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
bool equipJournalGearVariant(GearSlot slot,GearId gear,uint8_t variant){buddy.equippedVariants[uint8_t(slot)]=variant;return equipJournalGear(slot,gear);}
bool takeNewGearVariant(GearId&,uint8_t&){return false;}
#include "../../firmware/altoids_pet/gear.cpp"
#include "../../firmware/altoids_pet/gear_variants.cpp"
#include "../../firmware/altoids_pet/gear_sprites.cpp"
#include "../../firmware/altoids_pet/gear_overlay.cpp"
#include "../../firmware/altoids_pet/pet.cpp"
#include "../../firmware/altoids_pet/journal_ui.cpp"
#include "../../firmware/altoids_pet/background.cpp"
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
void testHomeBackgrounds() {
  // Every existing state maps independently for day/night; lightning keeps its cadence.
  for (bool day : {false, true}) {
    for (auto weather : {WEATHER_CLEAR, WEATHER_MAINLY_CLEAR, WEATHER_PARTLY_CLOUDY,
                        WEATHER_CLOUDY, WEATHER_RAIN, WEATHER_STORM, WEATHER_SNOW, WEATHER_FOG}) {
      auto scene = selectHomeScene(weather, day, 2);
      bool mostlyClear = weather == WEATHER_CLEAR || weather == WEATHER_MAINLY_CLEAR || weather == WEATHER_PARTLY_CLOUDY;
      assert(scene.nightSky == (!day && mostlyClear));
      auto clouds = mostlyClear ? (weather == WEATHER_CLEAR ? HomeCloudLayer::NONE : HomeCloudLayer::LIGHT)
                   : weather == WEATHER_STORM ? HomeCloudLayer::STORM
                   : weather == WEATHER_FOG ? HomeCloudLayer::NONE : HomeCloudLayer::HEAVY;
      assert(scene.clouds == clouds);
      HomePalette expected = weather == WEATHER_RAIN ? (day ? HomePalette::RAIN_DAY : HomePalette::RAIN_NIGHT)
            : weather == WEATHER_STORM ? (day ? HomePalette::STORM_DAY : HomePalette::STORM_NIGHT)
            : weather == WEATHER_SNOW ? (day ? HomePalette::SNOW_DAY : HomePalette::SNOW_NIGHT)
            : weather == WEATHER_FOG ? (day ? HomePalette::FOG_DAY : HomePalette::FOG_NIGHT)
            : weather == WEATHER_CLOUDY ? (day ? HomePalette::OVERCAST_DAY : HomePalette::OVERCAST_NIGHT)
            : day ? HomePalette::CLEAR_DAY : HomePalette::CLEAR_NIGHT;
      assert(scene.palette == expected && !scene.lightning);
    }
  }
  // Cloud pixels use the corresponding authored asset without moving margins.
  for (auto weather : {WEATHER_PARTLY_CLOUDY, WEATHER_RAIN, WEATHER_STORM}) {
    auto selected=selectHomeScene(weather,true,2);
    drawHomeEnvironment(selected);drawHomeClouds(selected);
    const uint8_t *asset=weather==WEATHER_PARTLY_CLOUDY?BACKGROUND_CLOUDS_LIGHT
                         :weather==WEATHER_RAIN?BACKGROUND_CLOUDS_HEAVY:BACKGROUND_STORM_CLOUDS;
    auto c=homeColors(selected);uint16_t palette[]={c.sky,c.outline,c.cloudLow,c.cloudMid,c.cloudHigh};
    for(int i=0;i<57600;++i){uint8_t value=pgm_read_byte(asset+i/2);
      unsigned role=i%2?value&15:value>>4;assert(display.getBuffer()[i]==palette[role]);}
  }
  auto night = selectHomeScene(WEATHER_CLEAR, false, 2);
  drawHomeEnvironment(night);animationFrame=2;currentMoonPhase=MOON_FULL;
  drawHomeNightSky(night);drawHomeClouds(night);
  assert(display.getBuffer()[62*240+24] == COLOR_WARM);
  assert(display.getBuffer()[92*240+201] == COLOR_WARM);
  currentMoonPhase=MOON_NEW;drawHomeEnvironment(night);drawHomeNightSky(night);
  assert(display.getBuffer()[92*240+201] == homeSkyColor(night));
  currentMoonPhase=MOON_FULL;
  // All source alpha/roles retain their exact coordinates at native resolution.
  auto scene = selectHomeScene(WEATHER_CLEAR, true, 2);
  drawHomeEnvironment(scene);uint16_t sky=homeSkyColor(scene);
  assert(display.getBuffer()[120*240+120] == sky);
  display.fillRect(0,0,240,240,0xF81F);drawHomeForeground(scene);
  bool sawForeground=false,sawTransparent=false;
  for(int i=0;i<57600;++i){
    uint8_t byte=pgm_read_byte(BACKGROUND_FOREGROUND+i/2),role=i%2?byte&15:byte>>4;
    if(!role){sawTransparent=true;assert(display.getBuffer()[i]==0xF81F);}
    else {sawForeground=true;assert(display.getBuffer()[i]!=0xF81F);}
  }
  assert(sawForeground && sawTransparent);
  // Synthetic buddy+gear reach grass solely to prove occlusion, without moving them in HOME.
  display.fillRect(0,210,240,30,0xFFFF);display.fillRect(0,215,240,10,0xF800);
  drawHomeForeground(scene);assert(display.getBuffer()[239*240+120]==homeColors(scene).grass);
  // Transparent clock glyphs leave the composed scenery intact between pixels.
  drawHomeEnvironment(scene);
  auto beforeText=std::vector<uint16_t>(display.getBuffer(),display.getBuffer()+57600);
  drawTime();int textPixels=0,untouched=0;
  for(int i=0;i<57600;++i){
    if(display.getBuffer()[i]!=beforeText[i]){
      assert(display.getBuffer()[i]==COLOR_TEXT);++textPixels;
    } else if(i/240>=8 && i/240<58 && i%240>=8 && i%240<146) ++untouched;
  }
  assert(textPixels>0 && untouched>0);
  // Full HOME must use this same final foreground placement and never show black intermediate frames.
  weatherState=WEATHER_STORM;currentHour=12;animationFrame=2;drawHome();
  auto dark=panel;int oldWindows=windows;
  animationFrame=0;drawHome();assert(windows>oldWindows);
  assert(selectHomeScene(WEATHER_STORM,true,0).lightning);
  assert(selectHomeScene(WEATHER_STORM,false,1).palette==HomePalette::LIGHTNING);
  assert(panel[170*240+0]!=dark[170*240+0]);
  assert(panel==std::vector<uint16_t>(display.getBuffer(),display.getBuffer()+57600));
  auto brightScene=selectHomeScene(WEATHER_STORM,true,0);
  assert(panel[239*240+120]==homeColors(brightScene).grass);
  uint8_t groundRole=pgm_read_byte(BACKGROUND_FOREGROUND+(212*240+120)/2)>>4;
  assert(panel[212*240+120]==(groundRole?homeColors(brightScene).grass:homeSkyColor(brightScene)));
  // Blank space immediately above the weather label retains sky/grass, not a panel.
  int nonBlack=0;for(auto color:panel)if(color)++nonBlack;assert(nonBlack>50000);
  oldWindows=windows;drawHome();assert(windows==oldWindows);
  weatherState=WEATHER_CLEAR;currentHour=12;animationFrame=2;
}
int main(){
 initializeDisplayBus();initializeDisplay();initializePetState();initializeAnimations();
 currentScreen=HOME;queuedEvent=FieldEventId::SEVERE_THUNDERSTORM_WARNING;
 drawHome();snapshot("field-event");assert(panel==std::vector<uint16_t>(display.getBuffer(),display.getBuffer()+57600));
 fakeMillis+=2500;drawHome();
 testHomeBackgrounds();
 for(int i=0;i<8;i++){buddy.locations[i].used=true;strcpy(buddy.locations[i].name,"123456789012345");}
 buddy.activeLocation=0;buddy.latestLocation=0;buddy.highestTemperatureLocation=0;buddy.lowestTemperatureLocation=1;
 buddy.unlockedGear=0x7f;initializeGearVariants(buddy);
 const GearId gear[]={GearId::FIELD_CAP,GearId::SUNGLASSES,GearId::WINTER_SCARF,GearId::RAINCOAT,GearId::BOOTS,GearId::UMBRELLA};
 for(int i=0;i<6;i++)buddy.equippedSlots[i]=gear[i];
 for(int i=0;i<=8;++i){weatherState=static_cast<WeatherState>(i);currentScreen=HOME;drawHome();assert(panel==std::vector<uint16_t>(display.getBuffer(),display.getBuffer()+57600));}
 // Active field label changes only the temperature/location strip, with one update.
 weatherState=WEATHER_RAIN;
 strcpy(buddy.locations[0].name,"CHICAGO, IL");
 strcpy(buddy.locations[1].name,"MADISON, WI");
 buddy.activeLocation=0;drawHome();
 std::vector<uint16_t> firstLocation=panel;int locationWindows=windows;
 buddy.activeLocation=1;drawHome();bool locationChanged=false;
 for(int i=0;i<57600;i++)if(panel[i]!=firstLocation[i]){
  locationChanged=true;assert(i/240>=188 && i/240<204);
 }
 assert(locationChanged && windows>locationWindows && windows-locationWindows<15);
 locationWindows=windows;drawHome();assert(windows==locationWindows);
 buddy.activeLocation=0;drawHome();snapshot("home");
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
 buddy.equippedVariants[uint8_t(GearSlot::BODY)]=4;buddy.unlockedVariants[uint8_t(GearId::RAINCOAT)-1]|=1UL<<4;
 currentScreen=HOME;drawHome();snapshot("variant-outfit");
 openJournalScreen(GEAR_SCREEN);snapshot("gear");
 handleJournalButtons(false,true,false);snapshot("slot");
 handleJournalButtons(false,true,false);snapshot("gear-color");
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

 FieldNote note;note.timestamp=1800000000;note.temperatureMilliC=22222;note.weatherCode=61;
 note.category=WeatherCategory::RAIN;note.location=0;note.metrics=buddy.latestMetrics;
 note.outcomes=FIELD_NOTE_OUTCOME_MASK;note.severeEvents=0x7fff;appendFieldNote(buddy,note);
 openJournalScreen(JOURNAL_SCREEN);for(int i=0;i<10;i++)handleJournalButtons(true,false,false);
 snapshot("field-notes-index");handleJournalButtons(false,true,false);
 for(int i=0;i<3;i++){char name[32];snprintf(name,sizeof(name),"field-note-%d",i);snapshot(name);
   assert(panel==std::vector<uint16_t>(display.getBuffer(),display.getBuffer()+57600));handleJournalButtons(false,true,false);}
 openJournalScreen(JOURNAL_SCREEN);for(int i=0;i<11;i++)handleJournalButtons(true,false,false);
 snapshot("achievements-index");handleJournalButtons(false,true,false);snapshot("achievement-locked");
 buddy.unlockedAchievements=ACHIEVEMENT_MASK;for(auto &at:buddy.achievementUnlockedAt)at=1800000000;
 for(int i=0;i<18;i++){
   handleJournalButtons(false,true,false);char name[40];snprintf(name,sizeof(name),"achievement-details-%d",i);snapshot(name);
   handleJournalButtons(false,false,true);handleJournalButtons(true,false,false);
 }
 currentScreen=HOME;currentHour=12;queuedAchievement=AchievementId::FLASH_FLOOD_WARNING_SEEN;
 drawHome();snapshot("achievement-card");fakeMillis+=2500;drawHome();
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
 puts("PASS: physical TFT compositing, all day/night PNG scenes, moon/stars, exact cloud roles/positions, foreground/UI order, nonblack lightning, combined gear, idle poses, sleep, minute-only partial updates, menus/timer/journal/records/gear layouts; previews saved in /tmp/fwf-tft-*.ppm.");
}
