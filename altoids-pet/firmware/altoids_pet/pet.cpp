#include "pet.h"
#include "config.h"
#include "display.h"
#include "weather.h"
#include "sound.h"
#include "sprites.h"
#include "gear_overlay.h"

// ==================================================
// PET STATE
// ==================================================

bool petReacting = false;
unsigned long reactionStart = 0;

// ==================================================
// ANIMATION
// ==================================================

unsigned long lastAnimationTime = 0;
unsigned long nextBlinkTime = 3500;
unsigned long blinkStarted = 0;

bool blinking = false;

int animationFrame = 0;

// Mood is visual runtime state only. Permanent field progress belongs to journal.
namespace {
const unsigned long HAPPY_DURATION_MS = 5000;
PetState petState = {PetMood::CALM};
bool petInitialized = false;
bool interactionMoodActive = false;
unsigned long interactionMoodStarted = 0;

void updatePetState() {
  if (!petInitialized) return;
  unsigned long nowMs = millis();
  if (isPetSleeping()) {
    petState.mood = PetMood::SLEEPY;
    interactionMoodActive = false;
  } else if (interactionMoodActive && nowMs - interactionMoodStarted < HAPPY_DURATION_MS) {
    petState.mood = petReacting ? PetMood::EXCITED : PetMood::HAPPY;
  } else {
    interactionMoodActive = false;
    petState.mood = PetMood::CALM;
  }
}
} // namespace

const PetState &getPetState() { return petState; }

void initializePetState() {
  interactionMoodActive = false;
  petInitialized = true;
  updatePetState();
}

bool interactWithPet() {
  if (!petInitialized || currentScreen != HOME || isPetSleeping()) return false;
  updatePetState();
  // Preserve the original 700 ms B hop/heart and five-second visual mood.
  petReacting = true;
  reactionStart = millis();
  interactionMoodActive = true;
  interactionMoodStarted = reactionStart;
  petState.mood = PetMood::EXCITED;
  soundPetInteraction();
  return true;
}

// Idle expression is disposable runtime state; it never dirties pet progress.
namespace {
enum class IdleAction : uint8_t {
  REST,
  BLINK,
  DOUBLE_BLINK,
  LOOK_LEFT,
  LOOK_RIGHT,
  BOUNCE,
  EAR_TWITCH,
  SUNNY_BOUNCE,
  LOOK_UP,
  UMBRELLA,
  SNOW_SHIVER,
  STORM_CROUCH,
  FOG_LOOK
};

struct IdleTiming {
  unsigned long blinkMin;
  unsigned long blinkRange;
  unsigned long actionMin;
  unsigned long actionRange;
};

IdleAction idleAction = IdleAction::REST;
uint8_t idleStep = 0;
unsigned long idleStepStarted = 0;
unsigned long nextIdleTime = 0;
unsigned long nextWeatherTime = 0;
WeatherState reactionWeather = WEATHER_UNKNOWN;
bool reactionDaylight = true;
unsigned long idleBlinkDeadline = 3500;
PetMood idleMood = PetMood::CALM;
bool idlePaused = true;
uint32_t idleVariation = 0x6D2B79F5UL;

// A small local generator varies timing without affecting any other subsystem.
uint32_t varyIdle() {
  idleVariation ^= idleVariation << 13;
  idleVariation ^= idleVariation >> 17;
  idleVariation ^= idleVariation << 5;
  return idleVariation;
}

IdleTiming idleTiming(PetMood mood) {
  switch (mood) {
    case PetMood::HAPPY:   return {3000, 2001, 2000, 2001};
    case PetMood::CURIOUS: return {3500, 2001, 6000, 4001};
    case PetMood::EXCITED: return {2000, 1501, 1500, 1501};
    case PetMood::SLEEPY:  return {7000, 3001, 20000, 10001};
    default:              return {4000, 3001, 10000, 8001};
  }
}

// Signed difference handles the 32-bit millis rollover on ESP32.
bool idleDue(unsigned long now, unsigned long deadline) {
  return static_cast<int32_t>(static_cast<uint32_t>(now - deadline)) >= 0;
}

void scheduleIdle(unsigned long now) {
  IdleTiming timing = idleTiming(idleMood);
  nextIdleTime = now + timing.actionMin + varyIdle() % timing.actionRange;
}

void scheduleBlink(unsigned long now) {
  IdleTiming timing = idleTiming(idleMood);
  nextBlinkTime = now + timing.blinkMin + varyIdle() % timing.blinkRange;
  idleBlinkDeadline = nextBlinkTime;
}

IdleAction chooseIdle() {
  uint32_t choice = varyIdle() % 100;
  switch (idleMood) {
    case PetMood::HAPPY:
      if (choice < 45) return IdleAction::BOUNCE;
      if (choice < 65) return IdleAction::DOUBLE_BLINK;
      if (choice < 80) return IdleAction::LOOK_LEFT;
      if (choice < 95) return IdleAction::LOOK_RIGHT;
      return IdleAction::EAR_TWITCH;
    case PetMood::CURIOUS:
      if (choice < 35) return IdleAction::LOOK_LEFT;
      if (choice < 70) return IdleAction::LOOK_RIGHT;
      if (choice < 80) return IdleAction::DOUBLE_BLINK;
      if (choice < 95) return IdleAction::EAR_TWITCH;
      return IdleAction::BOUNCE;
    case PetMood::EXCITED:
      if (choice < 60) return IdleAction::BOUNCE;
      if (choice < 90) return IdleAction::DOUBLE_BLINK;
      return choice < 95 ? IdleAction::LOOK_LEFT : IdleAction::LOOK_RIGHT;
    case PetMood::SLEEPY:
      return IdleAction::REST;
    default:
      if (choice < 20) return IdleAction::LOOK_LEFT;
      if (choice < 40) return IdleAction::LOOK_RIGHT;
      if (choice < 55) return IdleAction::EAR_TWITCH;
      if (choice < 60) return IdleAction::BOUNCE;
      if (choice < 70) return IdleAction::DOUBLE_BLINK;
      return IdleAction::REST;
  }
}

bool isWeatherAction(IdleAction action) {
  return action >= IdleAction::SUNNY_BOUNCE;
}

void scheduleWeather(unsigned long now, bool first = false) {
  // A first opportunity fits into a normal 30-second awake session. Later
  // reactions are deliberately rarer, even if the user keeps the pet awake.
  nextWeatherTime = now + (first ? 12000 : 25000) + varyIdle() % (first ? 10001 : 20001);
}

IdleAction chooseWeatherReaction() {
  if (!weatherValid) return IdleAction::REST;
  switch (weatherState) {
    case WEATHER_CLEAR:
    case WEATHER_MAINLY_CLEAR:
      return isDaylight() ? IdleAction::SUNNY_BOUNCE : IdleAction::LOOK_UP;
    case WEATHER_RAIN: return IdleAction::UMBRELLA;
    case WEATHER_SNOW: return IdleAction::SNOW_SHIVER;
    case WEATHER_STORM: return IdleAction::STORM_CROUCH;
    case WEATHER_FOG: return IdleAction::FOG_LOOK;
    default: return IdleAction::REST; // Cloudy, partly cloudy, or unknown: ordinary idle.
  }
}

bool weatherReactionVisible() {
  return weatherValid && weatherState == reactionWeather &&
         isDaylight() == reactionDaylight;
}

void pauseIdle() {
  idleAction = IdleAction::REST;
  idleStep = 0;
  blinking = false;
  idlePaused = true;
}

unsigned long idleStepDuration() {
  // Every visible phase lasts at least one existing 250 ms display interval.
  if (idleAction == IdleAction::LOOK_LEFT || idleAction == IdleAction::LOOK_RIGHT) {
    return 1000;
  }
  if (idleAction == IdleAction::LOOK_UP) return 1000;
  if (idleAction == IdleAction::UMBRELLA || idleAction == IdleAction::STORM_CROUCH) return 500;
  if (idleAction == IdleAction::FOG_LOOK) return 750;
  if (idleAction == IdleAction::BLINK && idleMood == PetMood::SLEEPY) return 500;
  return 250;
}

uint8_t idleStepCount() {
  switch (idleAction) {
    case IdleAction::DOUBLE_BLINK: return 3; // shut, open, shut
    case IdleAction::BOUNCE: return 3;       // up 1, up 2, up 1
    case IdleAction::EAR_TWITCH: return 2;   // tip shifts 1 pixel, then 2
    case IdleAction::SUNNY_BOUNCE: return 3;
    case IdleAction::UMBRELLA: return 3;
    case IdleAction::SNOW_SHIVER: return 4;
    case IdleAction::STORM_CROUCH: return 2;
    case IdleAction::FOG_LOOK: return 2;
    default: return 1;
  }
}

bool idleVisible() {
  return currentScreen == HOME && !petReacting && !isPetSleeping();
}
} // namespace

// ==================================================
// PET GRAPHICS
// ==================================================

void drawPet(
  int x,
  int y,
  bool sleeping,
  bool eyesClosed
) {

  // Keep the B hop and sleeping pose authoritative, even on immediate redraws.
  bool expressIdle = !sleeping && idleVisible() && nextBlinkTime == idleBlinkDeadline;
  // A weather/daylight change suppresses an obsolete reaction immediately.
  if (isWeatherAction(idleAction) && !weatherReactionVisible()) expressIdle = false;
  bool crouching = expressIdle && idleAction == IdleAction::STORM_CROUCH;
  int lookOffset = 0;
  int lookUp = 0;
  int earOffset = 0;
  if (expressIdle) {
    if (idleAction == IdleAction::LOOK_LEFT) lookOffset = -2;
    if (idleAction == IdleAction::LOOK_RIGHT) lookOffset = 2;
    if (idleAction == IdleAction::BOUNCE || idleAction == IdleAction::SUNNY_BOUNCE) {
      y -= idleStep == 1 ? 2 : 1;
    }
    if (idleAction == IdleAction::LOOK_UP) { lookUp = -2; lookOffset = 1; }
    if (idleAction == IdleAction::FOG_LOOK) lookOffset = idleStep == 0 ? -2 : 2;
    if (idleAction == IdleAction::SNOW_SHIVER) x += idleStep % 2 == 0 ? -1 : 1;
    if (crouching) { y += 3; eyesClosed = true; }
    if (idleAction == IdleAction::EAR_TWITCH) earOffset = idleStep + 1;
  }

  // Artwork selection is independent of the existing timing/state machine.
  const uint8_t *sprite = KITSUNE_IDLE;
  if (currentScreen != FOCUS_SCREEN) {
    switch (getPetState().mood) {
      case PetMood::HAPPY: sprite = KITSUNE_HAPPY; break;
      case PetMood::EXCITED: sprite = KITSUNE_EXCITED; break;
      case PetMood::SLEEPY: sprite = KITSUNE_SLEEPY; break;
      case PetMood::CALM:
      case PetMood::CURIOUS: break; // Curious looks remain occasional actions.
    }
  }
  if (currentScreen == TIMER_DONE) sprite = KITSUNE_HAPPY;
  if (petReacting) sprite = KITSUNE_EXCITED;
  if (expressIdle) {
    if (lookOffset < 0) sprite = KITSUNE_LOOK_LEFT;
    if (lookOffset > 0) sprite = KITSUNE_LOOK_RIGHT;
    if (lookUp < 0) sprite = KITSUNE_LOOK_UP;
    if (idleAction == IdleAction::BOUNCE || idleAction == IdleAction::SUNNY_BOUNCE) {
      sprite = KITSUNE_BOUNCE;
    }
  }
  // Blinks and the authoritative night-time sleep pose override expressions.
  if (eyesClosed) sprite = KITSUNE_BLINK;
  if (sleeping) sprite = KITSUNE_SLEEP;

  // 48x48 presentation inside the old 48x64 anchor preserves the feet baseline
  // and existing B hop, focus book, heart, and DONE bounce coordinates.
  drawKitsuneSprite(sprite, x, y + 16, earOffset, crouching);
  GearId weatherAccessory = GearId::NONE;
  if (expressIdle && idleAction == IdleAction::UMBRELLA) weatherAccessory = GearId::UMBRELLA;
  if (expressIdle && idleAction == IdleAction::SNOW_SHIVER) weatherAccessory = GearId::WINTER_SCARF;
  drawEquippedGear(x, y + 16, crouching, sprite == KITSUNE_BOUNCE, weatherAccessory);

  if (expressIdle && idleAction == IdleAction::UMBRELLA) {
    // Chunky canopy beside the pet, entirely below the clock/weather header.
    int umbrellaY = y + (idleStep == 1 ? 1 : 0);
    display.fillTriangle(x + 40, umbrellaY + 14, x + 52, umbrellaY + 5,
                         x + 64, umbrellaY + 14, SH110X_WHITE);
    display.drawLine(x + 52, umbrellaY + 14, x + 52, umbrellaY + 32, SH110X_WHITE);
    display.drawLine(x + 52, umbrellaY + 32, x + 48, umbrellaY + 32, SH110X_WHITE);
  }
  if (expressIdle && idleAction == IdleAction::SNOW_SHIVER) {
    display.fillRect(x + 8, y + 44, 24, 3, SH110X_WHITE);
    display.fillRect(x + 28, y + 47, 3, 7, SH110X_WHITE);
  }

}

// ==================================================
// HEART
// ==================================================

void drawHeart(int x, int y) {

  display.fillCircle(
    x + 4,
    y + 4,
    4,
    SH110X_WHITE
  );

  display.fillCircle(
    x + 12,
    y + 4,
    4,
    SH110X_WHITE
  );

  display.fillTriangle(
    x,
    y + 5,
    x + 16,
    y + 5,
    x + 8,
    y + 15,
    SH110X_WHITE
  );
}

// ==================================================
// BLINK LOGIC
// ==================================================

void updateBlink() {
  if (!idleVisible()) {
    pauseIdle();
    return;
  }

  // C already resets the shared blink deadline. Honor that existing control
  // by canceling the idle pose and preserving its three-second restart.
  if (nextBlinkTime != idleBlinkDeadline) {
    pauseIdle();
    idleBlinkDeadline = nextBlinkTime;
  }

  unsigned long now = millis();
  PetMood mood = getPetState().mood;
  if (idlePaused || mood != idleMood) {
    if (idlePaused) scheduleWeather(now, true);
    idleMood = mood;
    scheduleIdle(now);
    // Keep the existing first/return-home blink deadline. Mood transitions
    // otherwise use their new cadence; an active action completes naturally.
    if (!idlePaused && idleAction == IdleAction::REST) scheduleBlink(now);
    idlePaused = false;
  }

  if (isWeatherAction(idleAction) && !weatherReactionVisible()) {
    idleAction = IdleAction::REST;
    idleStep = 0;
    scheduleWeather(now);
  }

  if (idleAction != IdleAction::REST) {
    if (now - idleStepStarted >= idleStepDuration()) {
      idleStepStarted = now;
      if (++idleStep >= idleStepCount()) {
        bool wasBlink = idleAction == IdleAction::BLINK || idleAction == IdleAction::DOUBLE_BLINK;
        bool wasIdleAction = idleAction != IdleAction::BLINK;
        bool wasWeather = isWeatherAction(idleAction);
        idleAction = IdleAction::REST;
        idleStep = 0;
        if (wasBlink) scheduleBlink(now);
        if (wasIdleAction) scheduleIdle(now);
        if (wasWeather) scheduleWeather(now);
      }
    }
  } else {
    IdleAction action = IdleAction::REST;
    if (idleDue(now, nextBlinkTime)) {
      action = IdleAction::BLINK;
    } else if (idleDue(now, nextWeatherTime)) {
      action = chooseWeatherReaction();
      if (action == IdleAction::REST) scheduleWeather(now);
      else {
        reactionWeather = weatherState;
        reactionDaylight = isDaylight();
      }
    } else if (idleDue(now, nextIdleTime)) {
      action = chooseIdle();
      if (action == IdleAction::REST) scheduleIdle(now);
    }
    if (action != IdleAction::REST) {
      idleAction = action;
      idleStep = 0;
      idleStepStarted = now;
      if (action == IdleAction::BLINK || action == IdleAction::DOUBLE_BLINK) blinkStarted = now;
    }
  }

  blinking = idleAction == IdleAction::BLINK ||
             (idleAction == IdleAction::DOUBLE_BLINK && idleStep != 1);
}

// ==================================================
// ANIMATION UPDATE
// ==================================================

void updateAnimations() {

  if (
    currentScreen != HOME
  ) {

    pauseIdle();
    return;
  }

  unsigned long now =
    millis();

  if (
    now - lastAnimationTime <
      ANIMATION_INTERVAL_MS
  ) {

    return;
  }

  lastAnimationTime =
    now;

  updateBlink();
  animationFrame++;

  // Redraw one complete framebuffer.
  // clearDisplay() only clears RAM;
  // the OLED doesn't see the blank buffer.
  // display.display() happens once at the end.

  drawHome();
}


bool isPetSleeping() {
  if (!timeValid) {
    return false;
  }

  return
    currentHour >= PET_SLEEP_HOUR ||
    currentHour < PET_WAKE_HOUR;
}

void initializeAnimations() {
  pauseIdle();
  idleVariation = 0x6D2B79F5UL ^ static_cast<uint32_t>(millis());
  if (idleVariation == 0) idleVariation = 1;
  lastAnimationTime =
    millis();

  nextBlinkTime =
    millis() +
    3000;
  idleBlinkDeadline = nextBlinkTime;
}

void updatePetReaction() {
  updatePetState();
  // ----------------------------------------------
  // PET REACTION END
  // ----------------------------------------------

  if (
    petReacting &&
    millis() -
      reactionStart >
      700
  ) {

    petReacting =
      false;

    if (
      currentScreen ==
      HOME
    ) {

      drawHome();
    }
  }

}
