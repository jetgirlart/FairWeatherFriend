#define FWF_SETTINGS_TEST
#include "../journal_ui/test_journal_ui.cpp"
int main(){
 setenv("TZ","CST6CDT,M3.2.0/2,M11.1.0/2",1);tzset();initializeJournal();
 openJournalScreen(JOURNAL_SCREEN);for(int i=0;i<11;i++)nextPage();
 assert(shown("ACHIEVEMENTS") && shown("0 / 18 RECORDED"));
 handleJournalButtons(false,true,false);assert(shown("???") && shown("UNDISCOVERED"));
 handleJournalButtons(false,true,false);assert(shown("Record your first"));
 handleJournalButtons(false,false,true);assert(browsingAchievements && !achievementDetails);
 for(int i=0;i<18;i++)nextPage();assert(achievementIndex==0);
 handleJournalButtons(false,false,true);assert(!browsingAchievements && shown("B:OPEN"));
 handleJournalButtons(false,false,true);assert(currentScreen==MENU);
 fakeEpoch+=3600;assert(recordWeatherObservation({fakeEpoch,37778,0,WeatherCategory::CLEAR}));
 auto writes=nvsWrites;currentScreen=HOME;unsigned sounds=queuedSoundEvents.size();
 fakePetSleeping=true;
 while(drawFieldEventNotification()){assert(!shown("FIELD ACHIEVEMENT!"));display.display();fakeMillis+=2500;}
 assert(queuedSoundEvents.size()==sounds);
 fakePetSleeping=false;assert(drawFieldEventNotification());assert(shown("FIELD ACHIEVEMENT!") && shown("FIRST NOTES"));
 assert(queuedSoundEvents.back()==SoundEvent::ACHIEVEMENT);display.display();fakeMillis+=2500;
 assert(drawFieldEventNotification() && shown("CENTURY"));display.display();fakeMillis+=2500;
 assert(!drawFieldEventNotification() && nvsWrites==writes);
 openJournalScreen(JOURNAL_SCREEN);for(int i=0;i<11;i++)nextPage();handleJournalButtons(false,true,false);
 assert(shown("FIRST NOTES") && shown("RECORDED") && shown("2 / 18 RECORDED"));
 handleJournalButtons(false,true,false);assert(shown("Record your first"));
 unsigned pushes=framePushes;fakeMillis+=250;updateJournalScreens();assert(framePushes==pushes && nvsWrites==writes);
 // Render every flash label and wrapped description, including the longest names.
 auto actual=buddy;buddy.unlockedAchievements=ACHIEVEMENT_MASK;
 for(auto &at:buddy.achievementUnlockedAt)at=fakeEpoch;
 openJournalScreen(JOURNAL_SCREEN);for(int i=0;i<11;i++)nextPage();handleJournalButtons(false,true,false);
 for(int i=0;i<18;i++){
  assert(shown(achievementName(AchievementId(i))));handleJournalButtons(false,true,false);
  handleJournalButtons(false,false,true);nextPage();
 }
 buddy=actual;
 initializeJournal();assert(takeNewAchievement()==AchievementId::COUNT);
 puts("PASS: locked/unlocked achievements, description wrap/bounds, A/B/C levels, 18-entry wrap, queued cards/audio, sleep suppression, no extra writes or unchanged-frame redraws.");
}
