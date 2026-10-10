#define FWF_SETTINGS_TEST
#include "../journal_ui/test_journal_ui.cpp"
#include "../../firmware/altoids_pet/units.h"
unsigned silenceCalls=0;
void stopSound(){silenceCalls++;}
void drawHome(){display.clearDisplay();display.setCursor(0,0);display.print("HOME");display.display();}
double configuredLatitude(){const auto &s=getBuddySave();return s.locationConfigured?s.latitudeMicrodegrees/1000000.0:32.0;}
double configuredLongitude(){const auto &s=getBuddySave();return s.locationConfigured?s.longitudeMicrodegrees/1000000.0:-95.0;}
#include "../../firmware/altoids_pet/settings_ui.cpp"
void sendLine(const std::string &line){for(char c:line)Serial.input.push_back(c);Serial.input.push_back('\n');while(Serial.available())updateBuddySerial();}
void choose(unsigned index){openSettings();for(unsigned i=0;i<index;i++)handleSettingsButtons(true,false,false);handleSettingsButtons(false,true,false);}
int main(){
 setenv("TZ","CST6CDT,M3.2.0/2,M11.1.0/2",1);tzset();
 initializeJournal();checkpointJournal();assert(confirmBuddySetup(FurPaletteId::BROWN));
 assert(recordWeatherObservation({fakeEpoch,fahrenheitDeciToMilliC(320),0,WeatherCategory::CLEAR}));
 auto original=getBuddySave();unsigned writes=nvsWrites;
 choose(0);assert(shown("SOUND") && shown("ON"));handleSettingsButtons(true,false,false);handleSettingsButtons(false,false,true);
 assert(getBuddySave().soundEnabled && nvsWrites==writes);
 choose(0);handleSettingsButtons(true,false,false);failWrite=true;handleSettingsButtons(false,true,false);
 assert(shown("SAVE FAILED") && getBuddySave().soundEnabled);failWrite=false;handleSettingsButtons(false,true,false);
 assert(!getBuddySave().soundEnabled && silenceCalls==1 && nvsWrites==writes+1);
 initializeJournal();assert(!getBuddySave().soundEnabled && getBuddySave().furPalette==FurPaletteId::BROWN);
 choose(1);handleSettingsButtons(true,false,false);handleSettingsButtons(false,true,false);
 assert(getBuddySave().units==UnitsId::METRIC && getBuddySave().highestTemperatureMilliC==fahrenheitDeciToMilliC(320));
 char temp[32];formatBuddyTemperature(fahrenheitDeciToMilliC(320),temp,sizeof(temp));assert(std::string(temp)=="0.0 C");
 formatBuddyTemperature(fahrenheitDeciToMilliC(-400),temp,sizeof(temp));assert(std::string(temp)=="-40.0 C");
 formatBuddyTemperature(fahrenheitDeciToMilliC(700),temp,sizeof(temp));assert(std::string(temp)=="21.1 C");
 openJournalScreen(RECORDS_SCREEN);assert(shown("0.0 C"));openJournalScreen(JOURNAL_SCREEN);nextPage();assert(shown("0.0 C"));
 writes=nvsWrites;assert(saveBuddyUnits(UnitsId::METRIC) && nvsWrites==writes);assert(!saveBuddyUnits(static_cast<UnitsId>(2)));
 assert(upsertFieldLocation(1,"FIELD CAMP",41,-87));
 choose(2);assert(shown("ACTIVE: HOME"));
 auto observations=getBuddySave().totalObservations;auto visited=getBuddySave().fieldSitesVisited;
 handleSettingsButtons(true,false,false);handleSettingsButtons(false,true,false);
 assert(getBuddySave().activeLocation==1 && shown("ACTIVE: FIELD CAMP"));
 assert(getBuddySave().totalObservations==observations && getBuddySave().fieldSitesVisited==visited);
 handleSettingsButtons(true,false,false);handleSettingsButtons(false,true,false);
 assert(getBuddySave().activeLocation==0);
 sendLine("SET_LOCATION 41.123456 -87.654321");updateSettingsScreen();assert(shown("ACTIVE: HOME"));
 assert(getBuddySave().latitudeMicrodegrees==41123456 && getBuddySave().longitudeMicrodegrees==-87654321);
 writes=nvsWrites;auto located=getBuddySave();
 for(const char *line:{"SET_LOCATION 91 0","SET_LOCATION 0 -181","SET_LOCATION nan 0","SET_LOCATION inf 2","SET_LOCATION 1 2 junk","SET_LOCATION 1","SET_LOCATION 1    ","SET_LOCATION 1 x"})sendLine(line);
 assert(nvsWrites==writes && buddySaveChecksum(located)==buddySaveChecksum(getBuddySave()));
 sendLine("SET_LOCATION 41.123456 -87.654321");assert(nvsWrites==writes);
 failWrite=true;sendLine("SET_LOCATION -90 180");assert(buddySaveChecksum(located)==buddySaveChecksum(getBuddySave()));failWrite=false;
 sendLine("SET_LOCATION -90 180");assert(configuredLatitude()==-90 && configuredLongitude()==180);
 initializeJournal();assert(configuredLatitude()==-90 && configuredLongitude()==180 && getBuddySave().units==UnitsId::METRIC);
 auto current=getBuddySave();choose(3);writes=nvsWrites;Serial.output.clear();sendLine("EXPORT_BUDDY");updateSettingsScreen();
 assert(shown("EXPORT COMPLETE") && nvsWrites==writes && buddySaveChecksum(current)==buddySaveChecksum(getBuddySave()));
 JsonDocument json;assert(!deserializeJson(json,Serial.output));std::string compact;serializeJson(json,compact);
 // Required v4 fields and types are checked before any replacement or NVS write.
 for(int scenario=0;scenario<7;++scenario){
   JsonDocument malformed;assert(!deserializeJson(malformed,compact));
   if(scenario==0)malformed.remove("soundEnabled");
   if(scenario==1)malformed["soundEnabled"]=1;
   if(scenario==2)malformed["units"]=2;
   if(scenario==3)malformed["latitudeMicrodegrees"]=90000001;
   if(scenario==4)malformed["longitudeMicrodegrees"]=-180000001;
   if(scenario==5)malformed["locationConfigured"]="yes";
   if(scenario==6)malformed.remove("longitudeMicrodegrees");
   std::string invalid;serializeJson(malformed,invalid);
   assert(!importBuddy(invalid.c_str(),invalid.size()) && nvsWrites==writes && buddySaveChecksum(current)==buddySaveChecksum(getBuddySave()));
 }
 choose(4);sendLine("IMPORT_BUDDY {}");updateSettingsScreen();assert(shown("IMPORT FAILED") && nvsWrites==writes);
 sendLine("IMPORT_BUDDY "+compact);updateSettingsScreen();assert(shown("REPLACE CURRENT") && nvsWrites==writes);
 sendLine("CONFIRM_IMPORT 00000000");assert(importPending && nvsWrites==writes);
 handleSettingsButtons(false,false,true);assert(!importPending && nvsWrites==writes);
 choose(4);sendLine("IMPORT_BUDDY "+compact);fakeMillis+=60000;updateBuddySerial();updateSettingsScreen();assert(shown("IMPORT FAILED") && nvsWrites==writes);
 sendLine("IMPORT_BUDDY "+compact);updateSettingsScreen();failWrite=true;handleSettingsButtons(false,true,false);
 assert(shown("IMPORT FAILED") && nvsWrites==writes && buddySaveChecksum(current)==buddySaveChecksum(getBuddySave()));failWrite=false;
 sendLine("IMPORT_BUDDY "+compact);updateSettingsScreen();handleSettingsButtons(false,true,false);
 assert(shown("IMPORT COMPLETE") && nvsWrites==writes+1);handleSettingsButtons(false,false,true);assert(currentScreen==HOME);
 choose(5);assert(shown("XIAO ESP32-S3") && shown("ST7789 240x240") && shown("SAVE FORMAT 10"));
 auto pushes=framePushes;for(unsigned i=0;i<30;i++){fakeMillis+=250;updateSettingsScreen();}assert(framePushes==pushes);
 handleSettingsButtons(false,false,true);handleSettingsButtons(false,false,true);assert(currentScreen==MENU);
 // Explicit import outside the screen is rejected; Serial cannot overwrite.
 writes=nvsWrites;sendLine("IMPORT_BUDDY "+compact);assert(!importPending && nvsWrites==writes);
 storage["fwf-buddy"]["save0"]=std::vector<uint8_t>(204,0);initializeJournal();assert(!journalAvailable());
 assert(!saveBuddySound(true) && !saveBuddyUnits(UnitsId::US) && !saveBuddyLocation(0,0));
 puts("PASS: Settings controls/bounds/cancel/failed writes, persisted sound/units/location, display conversions, export read-only, explicit-button import/cancel/expiry, protected saves and unchanged-frame suppression.");
}
