#include "sound.h"
#include "config.h"
#include "pet.h"
#include "journal.h"
#include "display.h"
#include "weather.h"
#include "timer.h"
#include <esp32-hal-ledc.h>

#ifndef PIEZO_PIN
#define PIEZO_PIN D3
#endif
#ifndef SOUND_STARTUP_CHIRP
#define SOUND_STARTUP_CHIRP false
#endif

namespace {
struct ToneStep { uint16_t frequency, durationMs; };
const ToneStep PET_CHIRP[] = {{2400,55}};
const ToneStep TIMER_CHIRP[] = {{2000,90},{0,60},{2600,120}};
const ToneStep STARTUP_CHIRP[] = {{1600,35}};
const ToneStep ACHIEVEMENT_CHIRP[] = {{2000,45},{0,65},{3000,65}};
const ToneStep ITEM_CHIRP[] = {{1800,35},{0,50},{2300,45}};
const ToneStep COLOR_CHIRP[] = {{2300,30},{0,50},{2800,40}};
const ToneStep WEATHER_CHIRP[] = {{2100,35},{0,55},{2500,35}};
const ToneStep RECORD_CHIRP[] = {{2600,45}};
const ToneStep SEVERE_CHIRP[] = {{1400,45},{0,70},{1800,45}};
const ToneStep RAIN_CUE[] = {{1800,14}};
const ToneStep STORM_CUE[] = {{500,25},{0,80},{380,30}};
const ToneStep SNOW_CUE[] = {{2900,22},{0,60},{3300,20}};
const ToneStep WIND_CUE[] = {{1500,20},{0,55},{1900,20}};
const ToneStep NIGHT_CUE[] = {{3000,20},{0,65},{3500,25}};
struct SoundPattern { const ToneStep *steps; uint8_t count, priority; };
const SoundPattern patterns[] = {
 {STARTUP_CHIRP,1,1},{PET_CHIRP,1,2},{TIMER_CHIRP,3,8},
 {ACHIEVEMENT_CHIRP,3,7},{ITEM_CHIRP,3,5},{COLOR_CHIRP,3,5},
 {WEATHER_CHIRP,3,4},{RECORD_CHIRP,1,4},{SEVERE_CHIRP,3,6},
 {RAIN_CUE,1,0},{STORM_CUE,3,0},{SNOW_CUE,3,0},{WIND_CUE,3,0},{NIGHT_CUE,3,0}
};
static_assert(sizeof(patterns)/sizeof(patterns[0]) == uint8_t(SoundEvent::COUNT), "Missing sound pattern");
SoundEvent cue = SoundEvent::COUNT;
uint32_t pendingSounds = 0; // One bit/event: bounded, no backlog of repeated drips.
uint8_t stepIndex = 0;
uint32_t stepStarted = 0, gapStarted = 0, lastPetChirp = 0;
bool gapActive = false, hadPetChirp = false;
bool initialized = false, enabled = true, attached = false;
bool ambientArmed = false;
SoundEvent ambientKind = SoundEvent::COUNT;
uint32_t ambientStarted = 0, ambientInterval = 0, soundRandom = 0x74c2a195;
bool ambient(SoundEvent e) { return e >= SoundEvent::AMBIENT_RAIN && e < SoundEvent::COUNT; }
bool homeSoundContext() { return currentScreen == HOME && !timerActive() && !isPetSleeping(); }
uint32_t nextRandom() {
  soundRandom ^= soundRandom << 13; soundRandom ^= soundRandom >> 17; soundRandom ^= soundRandom << 5;
  return soundRandom;
}
void silenceOutput() {
  if (attached) { ledcWriteTone(PIEZO_PIN,0); ledcDetach(PIEZO_PIN); attached=false; }
  pinMode(PIEZO_PIN,OUTPUT); digitalWrite(PIEZO_PIN,LOW);
}
void endCue(bool gap = true) {
  cue=SoundEvent::COUNT; stepIndex=0;
  gapActive=gap; gapStarted=uint32_t(millis());
  if (initialized) silenceOutput();
}
bool playStep() {
  uint16_t frequency = patterns[uint8_t(cue)].steps[stepIndex].frequency;
  if (!frequency) { silenceOutput(); return true; }
  if (!attached) attached=ledcAttach(PIEZO_PIN,frequency,8);
  if (!attached || !ledcWriteTone(PIEZO_PIN,frequency)) {
    silenceOutput(); enabled=false;
    Serial.println("Piezo output unavailable; sound disabled for this boot."); return false;
  }
  return true;
}
void beginCue(SoundEvent next) {
  if (!initialized || !enabled || !getBuddySave().soundEnabled ||
      (next != SoundEvent::TIMER_COMPLETE && isPetSleeping())) return;
  if (cue != SoundEvent::COUNT && patterns[uint8_t(cue)].priority >= patterns[uint8_t(next)].priority) return;
  endCue(false); cue=next; stepStarted=uint32_t(millis());
  if (!playStep()) stopSound();
}
SoundEvent desiredAmbient() {
  if (!homeSoundContext() || !weatherValid) return SoundEvent::COUNT;
  if (weatherState==WEATHER_STORM) return SoundEvent::AMBIENT_STORM;
  if (weatherState==WEATHER_RAIN) return SoundEvent::AMBIENT_RAIN;
  if (weatherState==WEATHER_SNOW) return SoundEvent::AMBIENT_SNOW;
  const auto &metrics=getBuddySave().latestMetrics;
  if (metrics.has(MetricId::WIND) && metrics.values[uint8_t(MetricId::WIND)]>=3000) return SoundEvent::AMBIENT_WIND;
  if ((weatherState==WEATHER_CLEAR || weatherState==WEATHER_MAINLY_CLEAR) && !isDaylight()) return SoundEvent::AMBIENT_NIGHT;
  return SoundEvent::COUNT;
}
uint32_t ambientDelay(SoundEvent kind) {
  switch(kind) {
    case SoundEvent::AMBIENT_RAIN: return 15000+nextRandom()%25001;
    case SoundEvent::AMBIENT_STORM: case SoundEvent::AMBIENT_WIND: return 45000+nextRandom()%45001;
    case SoundEvent::AMBIENT_SNOW: return 60000+nextRandom()%60001;
    default: return 75000+nextRandom()%75001;
  }
}
void scheduleAmbient(uint32_t now) {
  SoundEvent kind=desiredAmbient();
  if (kind==SoundEvent::COUNT) { ambientArmed=false; return; }
  if (!ambientArmed || kind!=ambientKind) {
    ambientKind=kind; ambientStarted=now; ambientInterval=ambientDelay(kind); ambientArmed=true; return;
  }
  if (now-ambientStarted<ambientInterval) return;
  ambientStarted=now; ambientInterval=ambientDelay(kind);
  // Skip busy deadlines, rather than queueing ambience behind important events.
  if (cue==SoundEvent::COUNT && !pendingSounds && !gapActive) beginCue(kind);
}
} // namespace

void initializeSound() {
  // Accepted startup observations can enqueue before audio initialization.
  uint32_t startupPending=initialized ? 0 : pendingSounds;
  initialized=true; enabled=true; hadPetChirp=false; stopSound();
  pendingSounds=getBuddySave().soundEnabled ? startupPending : 0;
  soundRandom ^= uint32_t(millis()) ^ uint32_t(getBuddySave().createdAt);
  if (!soundRandom) soundRandom=0x74c2a195;
}
void stopSound() {
  pendingSounds=0; ambientArmed=false; endCue(false);
}
void queueSoundEvent(SoundEvent event) {
  if (event>=SoundEvent::COUNT || !enabled || !getBuddySave().soundEnabled ||
      (event!=SoundEvent::TIMER_COMPLETE && isPetSleeping())) return;
  if (event==SoundEvent::TIMER_COMPLETE) { soundTimerDone(); return; }
  if (ambient(event)) {
    if (homeSoundContext() && cue==SoundEvent::COUNT && !pendingSounds && !gapActive) beginCue(event);
    return;
  }
  pendingSounds |= 1UL<<uint8_t(event);
  if (ambient(cue)) endCue(false); // Higher-priority event replaces a drip immediately.
}
void soundPetInteraction() {
  if (!initialized || !enabled || !getBuddySave().soundEnabled || isPetSleeping() || cue==SoundEvent::TIMER_COMPLETE) return;
  uint32_t now=uint32_t(millis());
  if (hadPetChirp && now-lastPetChirp<1000) return;
  beginCue(SoundEvent::PET_INTERACTION);
  if (cue==SoundEvent::PET_INTERACTION) { hadPetChirp=true; lastPetChirp=now; }
}
void soundTimerDone() { beginCue(SoundEvent::TIMER_COMPLETE); }
void soundStartup() { if (SOUND_STARTUP_CHIRP) beginCue(SoundEvent::STARTUP); }
void updateSound() {
  if (!initialized) return;
  if (!enabled || !getBuddySave().soundEnabled) { stopSound(); return; }
  uint32_t now=uint32_t(millis());
  if (isPetSleeping()) {
    pendingSounds=0; ambientArmed=false;
    if (cue!=SoundEvent::TIMER_COMPLETE) { endCue(false); return; }
  }
  if (ambient(cue) && !homeSoundContext()) endCue(false);
  if (cue!=SoundEvent::COUNT && now-stepStarted>=patterns[uint8_t(cue)].steps[stepIndex].durationMs) {
    if (++stepIndex==patterns[uint8_t(cue)].count) endCue();
    else { stepStarted=now; if (!playStep()) { stopSound(); return; } }
  }
  if (gapActive && now-gapStarted>=650) gapActive=false;
  if (pendingSounds && homeSoundContext()) {
    SoundEvent next=SoundEvent::COUNT;
    for (uint8_t i=0;i<uint8_t(SoundEvent::AMBIENT_RAIN);++i) if ((pendingSounds & (1UL<<i)) &&
        (next==SoundEvent::COUNT || patterns[i].priority>patterns[uint8_t(next)].priority)) next=SoundEvent(i);
    if (next!=SoundEvent::COUNT && cue==SoundEvent::COUNT && !gapActive) {
      pendingSounds &= ~(1UL<<uint8_t(next)); beginCue(next);
    }
  }
  scheduleAmbient(now);
}
