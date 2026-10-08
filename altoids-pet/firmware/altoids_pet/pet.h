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

bool isPetSleeping();
void drawPet(int x, int y, bool sleeping, bool eyesClosed);
void drawHeart(int x, int y);
void updateBlink();
void updateAnimations();

void initializeAnimations();
void updatePetReaction();

// Temporary visual moods only; SLEEPY follows the existing sleep schedule.
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
