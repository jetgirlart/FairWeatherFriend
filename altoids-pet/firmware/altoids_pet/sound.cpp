#include "sound.h"
#include "config.h"
#include "pet.h"
#include <esp32-hal-ledc.h>
#include <stdint.h>

// Keep existing local config.h files compatible. Copy these settings from
// config.example.h to customize them; this module never touches credentials.
#ifndef PIEZO_PIN
#define PIEZO_PIN D3
#endif
#ifndef SOUND_ENABLED
#define SOUND_ENABLED true
#endif
#ifndef SOUND_STARTUP_CHIRP
#define SOUND_STARTUP_CHIRP false
#endif

namespace {
struct ToneStep { uint16_t frequency; uint16_t durationMs; };
const ToneStep PET_CHIRP[] = {{2400, 55}};
const ToneStep TIMER_CHIRP[] = {{2000, 90}, {0, 60}, {2600, 120}};
const ToneStep STARTUP_CHIRP[] = {{1600, 35}};
enum class Cue : uint8_t { NONE, STARTUP, PET, TIMER };
const ToneStep *steps = nullptr;
uint8_t stepCount = 0;
uint8_t stepIndex = 0;
uint32_t stepStarted = 0;
uint32_t lastPetChirp = 0;
bool hadPetChirp = false;
bool initialized = false;
bool enabled = SOUND_ENABLED;
bool attached = false;
Cue cue = Cue::NONE;

void silenceOutput() {
  if (attached) {
    ledcWriteTone(PIEZO_PIN, 0);
    ledcDetach(PIEZO_PIN);
    attached = false;
  }
  pinMode(PIEZO_PIN, OUTPUT);
  digitalWrite(PIEZO_PIN, LOW);
}

bool playStep() {
  uint16_t frequency = steps[stepIndex].frequency;
  if (frequency == 0) {
    silenceOutput();
    return true;
  }
  if (!attached) attached = ledcAttach(PIEZO_PIN, frequency, 8);
  if (!attached || ledcWriteTone(PIEZO_PIN, frequency) == 0) {
    silenceOutput();
    enabled = false;
    Serial.println("Piezo output unavailable; sound disabled for this boot.");
    return false;
  }
  return true;
}

void beginCue(Cue next, const ToneStep *notes, uint8_t count) {
  if (!initialized || !enabled || (!SOUND_ENABLED)) return;
  if (next != Cue::TIMER && isPetSleeping()) return;
  if (cue == Cue::TIMER || (cue != Cue::NONE && next <= cue)) return;
  stopSound();
  cue = next;
  steps = notes;
  stepCount = count;
  stepIndex = 0;
  stepStarted = static_cast<uint32_t>(millis());
  if (!playStep()) stopSound();
}
} // namespace

void initializeSound() {
  initialized = true;
  enabled = SOUND_ENABLED;
  hadPetChirp = false;
  stopSound();
}

void stopSound() {
  cue = Cue::NONE;
  steps = nullptr;
  stepCount = stepIndex = 0;
  if (initialized) silenceOutput();
}

void soundPetInteraction() {
  if (!initialized || !enabled || isPetSleeping() || cue == Cue::TIMER) return;
  uint32_t now = static_cast<uint32_t>(millis());
  if (hadPetChirp && now - lastPetChirp < 1000) return;
  beginCue(Cue::PET, PET_CHIRP, 1);
  if (cue == Cue::PET) {
    hadPetChirp = true;
    lastPetChirp = now;
  }
}

void soundTimerDone() {
  beginCue(Cue::TIMER, TIMER_CHIRP, 3);
}

void soundStartup() {
  if (SOUND_STARTUP_CHIRP) beginCue(Cue::STARTUP, STARTUP_CHIRP, 1);
}

void updateSound() {
  if (!initialized || cue == Cue::NONE) return;
  if (!enabled || !SOUND_ENABLED || (cue != Cue::TIMER && isPetSleeping())) {
    stopSound();
    return;
  }
  uint32_t now = static_cast<uint32_t>(millis());
  if (now - stepStarted < steps[stepIndex].durationMs) return;
  if (++stepIndex == stepCount) {
    stopSound();
    return;
  }
  // Advance one audible step, never a burst of missed notes after a slow loop.
  stepStarted = now;
  if (!playStep()) stopSound();
}
