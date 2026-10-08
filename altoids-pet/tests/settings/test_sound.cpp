#include <assert.h>
#include "Arduino.h"
#include "journal.h"
#include "sound.h"
SerialType Serial;
unsigned long fakeMillis=0;
BuddySaveData saved;
bool asleep=false;
unsigned audible=0;
bool pwm=false;
const BuddySaveData &getBuddySave(){return saved;}
bool isPetSleeping(){return asleep;}
void digitalWrite(int pin,int level){assert(pin==D3 && level==LOW);}
void pinMode(int pin,int mode){assert(pin==D3 && mode==OUTPUT);}
bool ledcAttach(uint8_t pin,uint32_t,uint8_t){assert(pin==D3);pwm=true;return true;}
uint32_t ledcWriteTone(uint8_t pin,uint32_t frequency){assert(pin==D3);if(frequency)audible++;return frequency;}
bool ledcDetach(uint8_t pin){assert(pin==D3);pwm=false;return true;}
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
 puts("PASS: saved sound OFF gates every cue and active output; ON retains brief non-blocking timer tones and asleep-pet silence.");
}
