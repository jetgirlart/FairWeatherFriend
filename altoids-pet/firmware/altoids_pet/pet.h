#pragma once

#include <Arduino.h>
#include <stdint.h>

extern bool petReacting;
extern unsigned long reactionStart;
extern unsigned long lastAnimationTime;
extern unsigned long nextBlinkTime;
extern unsigned long blinkStarted;
extern bool blinking;
extern int animationFrame;

// Compatibility query: always false; the pet has no night-time sleep schedule.
bool isPetSleeping();
void drawPet(int x, int y, bool sleeping, bool eyesClosed);
void drawHeart(int x, int y);
void updateBlink();
void updateAnimations();

void initializeAnimations();
void updatePetReaction();

// Temporary visual moods only. SLEEPY is retained as a legacy rendering ID;
// the active companion no longer enters it from the clock.
enum class PetMood : uint8_t {
  HAPPY,
  CALM,
  CURIOUS,
  SLEEPY,
  EXCITED
};

struct PetState {
  PetMood mood;
};

void initializePetState();
const PetState &getPetState();
bool interactWithPet();
