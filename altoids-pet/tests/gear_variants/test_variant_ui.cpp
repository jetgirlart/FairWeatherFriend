#define FWF_SETTINGS_TEST
#include "../journal_ui/test_journal_ui.cpp"
int main(){
 setenv("TZ","CST6CDT,M3.2.0/2,M11.1.0/2",1);tzset();initializeJournal();
 for(int i=0;i<10;i++){fakeEpoch+=3600;assert(recordWeatherObservation({fakeEpoch,20000,61,WeatherCategory::RAIN}));}
 openJournalScreen(GEAR_SCREEN);
 for(int i=0;i<5;i++)assert(handleJournalButtons(true,false,false));
 assert(selectedSlot==GearSlot::PROP);handleJournalButtons(false,true,false);handleJournalButtons(true,false,false);
 assert(selectedGear==GearId::UMBRELLA);auto writes=nvsWrites;
 handleJournalButtons(false,true,false);assert(choosingVariant && shown("ORIGINAL") && nvsWrites==writes);
 handleJournalButtons(true,false,false);assert(shown("BLUE"));
 handleJournalButtons(true,false,false);assert(shown("ORIGINAL")); // RED remains locked and is omitted.
 handleJournalButtons(true,false,false);handleJournalButtons(false,false,true);assert(!choosingVariant && nvsWrites==writes);
 handleJournalButtons(false,true,false);handleJournalButtons(true,false,false);handleJournalButtons(false,true,false);
 assert(getBuddySave().equippedSlots[5]==GearId::UMBRELLA && getBuddySave().equippedVariants[5]==1 && shown("EQUIPPED"));
 initializeJournal();assert(getBuddySave().equippedVariants[5]==1);
 // Queue one observation's new colors without duplicate saves or blocking UI timing.
 fakeEpoch+=3600;assert(recordWeatherObservation({fakeEpoch,20000,0,WeatherCategory::CLEAR}));writes=nvsWrites;
 currentScreen=HOME;bool shownColor=false;
 while(drawFieldEventNotification()){
  assert(shown("NEW GEAR COLOR!"));shownColor=true;display.display();fakeMillis+=2500;
 }
 assert(shownColor && nvsWrites==writes);
 puts("PASS: slot/item/color navigation, omitted locked colors, cancel/equip/retention, explicit equipped variant and non-blocking queued color cards without NVS writes.");
}
