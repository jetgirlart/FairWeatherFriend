#include <assert.h>
#include "Arduino.h"
#include "journal.h"
#include "sound.h"
#include "display.h"
#include "weather.h"
SerialType Serial;
unsigned long fakeMillis=0;
BuddySaveData saved;
bool asleep=false;
ScreenMode currentScreen=HOME;
bool focusRunning=false,weatherValid=true,fakeDaylight=true;
WeatherState weatherState=WEATHER_CLEAR;
bool timerActive(){return focusRunning;}
bool isDaylight(){return fakeDaylight;}
unsigned audible=0;
bool pwm=false;
const BuddySaveData &getBuddySave(){return saved;}
bool isPetSleeping(){return asleep;}
void digitalWrite(int pin,int level){assert(pin==D3 && level==LOW);}
void pinMode(int pin,int mode){assert(pin==D3 && mode==OUTPUT);}
bool ledcAttach(uint8_t pin,uint32_t,uint8_t){assert(pin==D3);pwm=true;return true;}
uint32_t ledcWriteTone(uint8_t pin,uint32_t frequency){assert(pin==D3);if(frequency)audible++;return frequency;}
bool ledcDetach(uint8_t pin){assert(pin==D3);pwm=false;return true;}
#define FWF_HARDWARE_H // The mocked display surface supplies dimensions; Arduino stub supplies pins.
#include "../../firmware/altoids_pet/sound.cpp"
int main(){
 saved.soundEnabled=false;initializeSound();soundPetInteraction();soundTimerDone();soundStartup();assert(audible==0 && !pwm);
 saved.soundEnabled=true;soundPetInteraction();assert(audible==1 && pwm);
 saved.soundEnabled=false;updateSound();assert(!pwm); // Disabling silences an active cue.
 saved.soundEnabled=true;asleep=true;soundPetInteraction();soundStartup();assert(audible==1);
 soundTimerDone();assert(audible==2 && pwm); // Existing timer-at-night exception.
 fakeMillis+=90;updateSound();assert(!pwm);fakeMillis+=60;updateSound();assert(audible==3 && pwm);
 fakeMillis+=120;updateSound();assert(!pwm);
 saved.soundEnabled=false;soundTimerDone();assert(audible==3 && !pwm);
 // Every named event respects SOUND OFF, including queued work and ambience.
 saved.soundEnabled=false;asleep=false;currentScreen=HOME;weatherState=WEATHER_RAIN;
 unsigned silent=audible;initializeSound();
 for(uint8_t i=0;i<uint8_t(SoundEvent::COUNT);i++)queueSoundEvent(SoundEvent(i));
 for(int i=0;i<300;i++){fakeMillis+=1000;updateSound();}assert(audible==silent && !pwm);
 auto resetScene=[&](){saved.soundEnabled=true;asleep=false;currentScreen=HOME;focusRunning=false;
  weatherValid=true;weatherState=WEATHER_RAIN;fakeDaylight=true;fakeMillis=1000;initializeSound();};
 resetScene();queueSoundEvent(SoundEvent::AMBIENT_RAIN);assert(pwm);
 queueSoundEvent(SoundEvent::LIFETIME_RECORD);queueSoundEvent(SoundEvent::ACHIEVEMENT);
 assert(!pwm);updateSound();assert(cue==SoundEvent::ACHIEVEMENT && pwm); // Ambient preempted, record waits.
 for(int i=0;i<3;i++) {fakeMillis+=patterns[uint8_t(cue)].steps[stepIndex].durationMs;updateSound();}
 fakeMillis+=650;updateSound();assert(cue==SoundEvent::LIFETIME_RECORD);
 resetScene();updateSound();uint32_t previousDrip=0;unsigned drips=0;
 for(int i=0;i<20000;i++){
  unsigned before=audible;fakeMillis+=10;updateSound();
  if(audible!=before){uint32_t at=fakeMillis;assert(cue==SoundEvent::AMBIENT_RAIN);
   if(previousDrip)assert(at-previousDrip>=15000 && at-previousDrip<=40010);
   else assert(at-1000>=15000 && at-1000<=40010);
   previousDrip=at;drips++;}
 }assert(drips>=4 && drips<=14);
 // No immediate catch-up after leaving menus or focus; context change rearms.
 for(int mode=0;mode<4;mode++){
  resetScene();if(mode==0)asleep=true;if(mode==1)currentScreen=MENU;
  if(mode==2){focusRunning=true;currentScreen=FOCUS_SCREEN;}if(mode==3){focusRunning=true;currentScreen=HOME;}
  silent=audible;for(int i=0;i<200;i++){fakeMillis+=1000;updateSound();}assert(audible==silent && !pwm);
  asleep=false;focusRunning=false;currentScreen=HOME;updateSound();assert(audible==silent);
  fakeMillis+=14999;updateSound();assert(audible==silent);
 }
 // Weather-specific ranges, wind threshold and daylight gating.
 const WeatherState states[]={WEATHER_STORM,WEATHER_SNOW,WEATHER_CLEAR,WEATHER_CLEAR};
 const SoundEvent events[]={SoundEvent::AMBIENT_STORM,SoundEvent::AMBIENT_SNOW,SoundEvent::AMBIENT_WIND,SoundEvent::AMBIENT_NIGHT};
 const uint32_t lower[]={45000,60000,45000,75000},upper[]={90000,120000,90000,150000};
 for(int i=0;i<4;i++){
  resetScene();weatherState=states[i];saved.latestMetrics=WeatherMetrics{};
  if(i==2){saved.latestMetrics.validMask=2;saved.latestMetrics.values[1]=2999;updateSound();assert(!ambientArmed);saved.latestMetrics.values[1]=3000;}
  if(i==3)fakeDaylight=false;
  updateSound();assert(ambientKind==events[i] && ambientInterval>=lower[i] && ambientInterval<=upper[i]);
  silent=audible;fakeMillis+=ambientInterval-1;updateSound();assert(audible==silent);
  fakeMillis++;updateSound();assert(cue==events[i] && audible==silent+1);
 }
 resetScene();queueSoundEvent(SoundEvent::GEAR_ITEM);queueSoundEvent(SoundEvent::GEAR_ITEM);
 updateSound();assert(cue==SoundEvent::GEAR_ITEM); // Repeated pending types coalesce.
 asleep=true;updateSound();assert(!pwm && !pendingSounds);
 soundTimerDone();assert(pwm && cue==SoundEvent::TIMER_COMPLETE);
 saved.soundEnabled=false;updateSound();assert(!pwm && !pendingSounds);
 // Millis rollover still waits the full ambient interval.
 resetScene();fakeMillis=0xfffffff0;updateSound();uint32_t interval=ambientInterval;
 fakeMillis=uint32_t(0xfffffff0UL+interval-1);silent=audible;updateSound();assert(audible==silent);
 fakeMillis=uint32_t(0xfffffff0UL+interval);updateSound();assert(audible==silent+1);
 // Cold-boot field cues survive late sound initialization without playing early.
 stopSound();initialized=false;saved.soundEnabled=true;asleep=false;weatherState=WEATHER_CLEAR;
 saved.latestMetrics=WeatherMetrics{};fakeDaylight=true;silent=audible;
 queueSoundEvent(SoundEvent::WEATHER_DISCOVERY);assert(audible==silent && !pwm);
 initializeSound();updateSound();assert(audible==silent+1 && cue==SoundEvent::WEATHER_DISCOVERY);
 stopSound();
 puts("PASS: all named cues/SOUND OFF, queued priority over ambience, rain spacing, weather ranges, wind threshold, sleep/focus/menu suppression, no catch-up, coalescing, millis rollover and unchanged timer/pet cues.");
}
