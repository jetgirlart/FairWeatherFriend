#include "pet.h"
#include "config.h"
#include "display.h"
#include "weather.h"
#include <Preferences.h>
#include <esp_attr.h>
#include <esp_sleep.h>

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

// Pet progress is friendly: only interactions increase friendship; time away
// never reduces it. RTC holds every change, NVS holds occasional checkpoints.
namespace {
const uint32_t PET_RECORD_MAGIC = 0x50455431;
const uint32_t PET_RECORD_VERSION = 1;
const uint32_t FRIENDSHIP_INTERVAL_MS = 10UL * 60UL * 1000UL;
const uint32_t CHECKPOINT_INTERVAL_MS = 10UL * 60UL * 1000UL;
const unsigned long HAPPY_DURATION_MS = 5000;

struct PetRecord {
  uint32_t magic;
  uint32_t version;
  PetState state;
  uint32_t friendshipCooldownMs;
  int64_t observedEpoch;
};

RTC_DATA_ATTR PetRecord petRecord = {};
RTC_DATA_ATTR bool petDirty = false;
RTC_DATA_ATTR uint32_t checkpointCooldownMs = 0;
unsigned long lastPetUpdateMs = 0;
bool petInitialized = false;
bool interactionMoodActive = false;
unsigned long interactionMoodStarted = 0;

bool validRecord(const PetRecord &record) {
  return record.magic == PET_RECORD_MAGIC &&
         record.version == PET_RECORD_VERSION &&
         record.state.friendship <= 100 &&
         record.state.mood <= PetMood::EXCITED &&
         record.state.createdAt >= 0 && record.observedEpoch >= 0 &&
         record.friendshipCooldownMs <= FRIENDSHIP_INTERVAL_MS;
}

void reduceCooldown(uint32_t &remaining, uint64_t elapsed) {
  remaining = elapsed >= remaining ? 0 : remaining - elapsed;
}

int64_t petEpoch() {
  // Do not invent a birthday from an unsynchronized clock.
  return timeValid ? static_cast<int64_t>(time(nullptr)) : 0;
}

void updatePetState() {
  if (!petInitialized) return;

  unsigned long nowMs = millis();
  unsigned long elapsed = nowMs - lastPetUpdateMs;
  lastPetUpdateMs = nowMs;
  reduceCooldown(petRecord.friendshipCooldownMs, elapsed);
  reduceCooldown(checkpointCooldownMs, elapsed);

  int64_t now = petEpoch();
  if (now > 0) {
    petRecord.observedEpoch = now;
    if (petRecord.state.createdAt == 0) {
      petRecord.state.createdAt = now;
      petDirty = true;
      // A first valid birthday is a one-time meaningful checkpoint.
      checkpointCooldownMs = 0;
    }
  }

  if (isPetSleeping()) {
    petRecord.state.mood = PetMood::SLEEPY;
    interactionMoodActive = false;
  } else if (interactionMoodActive &&
             nowMs - interactionMoodStarted < HAPPY_DURATION_MS) {
    petRecord.state.mood = petReacting ? PetMood::EXCITED : PetMood::HAPPY;
  } else {
    interactionMoodActive = false;
    petRecord.state.mood = PetMood::CALM;
  }
}
} // namespace

const PetState &getPetState() {
  return petRecord.state;
}

void checkpointPetState() {
  if (!petInitialized) return;
  updatePetState();
  if (!petDirty || checkpointCooldownMs != 0) return;

  // Even failed writes are rate limited. Keep dirty state for the next retry.
  checkpointCooldownMs = CHECKPOINT_INTERVAL_MS;
  Preferences preferences;
  if (!preferences.begin("altoids-pet", false)) {
    Serial.println("Pet checkpoint unavailable; RTC state retained.");
    return;
  }
  size_t written = preferences.putBytes("state", &petRecord, sizeof(petRecord));
  preferences.end();
  if (written == sizeof(petRecord)) {
    petDirty = false;
    Serial.println("Pet checkpoint saved.");
  } else {
    Serial.println("Pet checkpoint failed; RTC state retained.");
  }
}

void initializePetState() {
  bool deepSleepWake = esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED;
  if (!deepSleepWake || !validRecord(petRecord)) {
    PetRecord restored = {};
    Preferences preferences;
    bool loaded = false;
    if (preferences.begin("altoids-pet", true)) {
      loaded = preferences.getBytesLength("state") == sizeof(restored) &&
               preferences.getBytes("state", &restored, sizeof(restored)) == sizeof(restored) &&
               validRecord(restored);
      preferences.end();
    }
    if (loaded) {
      petRecord = restored;
      petDirty = false;
      checkpointCooldownMs = CHECKPOINT_INTERVAL_MS;
    } else {
      petRecord = {};
      petRecord.magic = PET_RECORD_MAGIC;
      petRecord.version = PET_RECORD_VERSION;
      petRecord.state.mood = PetMood::CALM;
      petDirty = true;
      checkpointCooldownMs = 0;
    }
  }

  // Count elapsed sleep/offline time only when the existing clock is valid.
  // Without it, RTC cooldowns count awake millis, conservatively preventing
  // sleep/wake or clock failures from bypassing the friendship rate limit.
  int64_t now = petEpoch();
  if (now > petRecord.observedEpoch && petRecord.observedEpoch > 0) {
    uint64_t elapsed = static_cast<uint64_t>(now - petRecord.observedEpoch) * 1000ULL;
    reduceCooldown(petRecord.friendshipCooldownMs, elapsed);
    reduceCooldown(checkpointCooldownMs, elapsed);
  }
  lastPetUpdateMs = millis();
  interactionMoodActive = false;
  petInitialized = true;
  checkpointPetState();
  Serial.printf("Pet: friendship %u, interactions %llu, birthday %lld\n",
                static_cast<unsigned>(petRecord.state.friendship),
                static_cast<unsigned long long>(petRecord.state.interactions),
                static_cast<long long>(petRecord.state.createdAt));
}

bool interactWithPet() {
  if (!petInitialized || currentScreen != HOME || isPetSleeping()) return false;
  updatePetState();

  // Retain the original hop/heart reaction and its 700 ms duration.
  petReacting = true;
  reactionStart = millis();
  interactionMoodActive = true;
  interactionMoodStarted = reactionStart;
  petRecord.state.mood = PetMood::EXCITED;
  if (petRecord.state.interactions < UINT64_MAX) {
    petRecord.state.interactions++;
  }
  if (petRecord.state.friendship < 100 && petRecord.friendshipCooldownMs == 0) {
    petRecord.state.friendship++;
    petRecord.friendshipCooldownMs = FRIENDSHIP_INTERVAL_MS;
  }
  petDirty = true;
  Serial.printf("Pet interaction: friendship %u, interactions %llu\n",
                static_cast<unsigned>(petRecord.state.friendship),
                static_cast<unsigned long long>(petRecord.state.interactions));
  return true;
}

// ==================================================
// PET GRAPHICS
// ==================================================

void drawPet(
  int x,
  int y,
  bool sleeping,
  bool eyesClosed
) {

  // ears

  display.drawTriangle(
    x + 6, y + 12,
    x + 14, y,
    x + 20, y + 14,
    SH110X_WHITE
  );

  display.drawTriangle(
    x + 28, y + 14,
    x + 34, y,
    x + 42, y + 12,
    SH110X_WHITE
  );

  // head

  display.drawRoundRect(
    x + 5,
    y + 10,
    38,
    30,
    8,
    SH110X_WHITE
  );

  // eyes

  if (
    sleeping ||
    eyesClosed
  ) {

    display.drawLine(
      x + 13, y + 24,
      x + 19, y + 24,
      SH110X_WHITE
    );

    display.drawLine(
      x + 29, y + 24,
      x + 35, y + 24,
      SH110X_WHITE
    );

  } else {

    display.fillCircle(
      x + 16,
      y + 24,
      2,
      SH110X_WHITE
    );

    display.fillCircle(
      x + 32,
      y + 24,
      2,
      SH110X_WHITE
    );
  }

  // nose

  display.fillCircle(
    x + 24,
    y + 29,
    1,
    SH110X_WHITE
  );

  // body

  display.drawRoundRect(
    x + 13,
    y + 39,
    22,
    22,
    7,
    SH110X_WHITE
  );

  // feet

  display.drawLine(
    x + 16, y + 60,
    x + 12, y + 64,
    SH110X_WHITE
  );

  display.drawLine(
    x + 32, y + 60,
    x + 36, y + 64,
    SH110X_WHITE
  );
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

  if (
    currentScreen != HOME ||
    isPetSleeping()
  ) {

    blinking =
      false;

    return;
  }

  unsigned long now =
    millis();

  if (
    !blinking &&
    now >= nextBlinkTime
  ) {

    blinking =
      true;

    blinkStarted =
      now;
  }

  if (
    blinking &&
    now - blinkStarted >= 160
  ) {

    blinking =
      false;

    // Variable-ish blink timing without using
    // expensive random behavior.

    nextBlinkTime =
      now +
      3200 +
      (
        animationFrame % 5
      ) * 350;
  }
}

// ==================================================
// ANIMATION UPDATE
// ==================================================

void updateAnimations() {

  if (
    currentScreen != HOME
  ) {

    return;
  }

  unsigned long now =
    millis();

  updateBlink();

  if (
    now - lastAnimationTime <
      ANIMATION_INTERVAL_MS
  ) {

    return;
  }

  lastAnimationTime =
    now;

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
  lastAnimationTime =
    millis();

  nextBlinkTime =
    millis() +
    3000;
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

  checkpointPetState();
}
