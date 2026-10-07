#include "pet.h"
#include "config.h"
#include "display.h"
#include "weather.h"
#include <Preferences.h>
#include <esp_attr.h>
#include <esp_sleep.h>
#include <stddef.h>

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
RTC_DATA_ATTR uint32_t checkpointRetryMs = 0;
RTC_DATA_ATTR uint64_t petGeneration = 0;
RTC_DATA_ATTR bool meaningfulCheckpointPending = false;

// Alternate slots preserve the previous checkpoint if the next one is invalid.
// Keep PetRecord v1 unchanged so existing single-key checkpoints can migrate.
struct StoredPetRecord {
  PetRecord record;
  uint64_t generation;
  uint32_t checksum;
};
unsigned long lastPetUpdateMs = 0;
bool petInitialized = false;
bool petStorageAvailable = false;
bool birthdayStored = false;
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

uint32_t recordChecksum(const StoredPetRecord &stored) {
  const unsigned char *bytes = reinterpret_cast<const unsigned char *>(&stored);
  uint32_t checksum = 2166136261UL;
  for (size_t i = 0; i < offsetof(StoredPetRecord, checksum); ++i) {
    checksum = (checksum ^ bytes[i]) * 16777619UL;
  }
  return checksum;
}

bool readCheckpoint(Preferences &preferences, const char *key, StoredPetRecord &stored) {
  return preferences.isKey(key) && preferences.getBytesLength(key) == sizeof(stored) &&
         preferences.getBytes(key, &stored, sizeof(stored)) == sizeof(stored) &&
         stored.generation > 0 && validRecord(stored.record) &&
         stored.checksum == recordChecksum(stored);
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
  reduceCooldown(checkpointRetryMs, elapsed);

  int64_t now = petEpoch();
  if (now > 0) {
    petRecord.observedEpoch = now;
    if (petRecord.state.createdAt == 0) {
      petRecord.state.createdAt = now;
      petDirty = true;
      // A first valid birthday is a one-time meaningful checkpoint.
      meaningfulCheckpointPending = true;
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

void checkpointPetState(bool beforeSleep) {
  if (!petInitialized) return;
  updatePetState();
  if (!petStorageAvailable || !petDirty || checkpointRetryMs != 0) return;
  if (!beforeSleep && !meaningfulCheckpointPending && checkpointCooldownMs != 0) return;

  Preferences preferences;
  if (!preferences.begin("altoids-pet", false)) {
    checkpointRetryMs = CHECKPOINT_INTERVAL_MS;
    Serial.println("Pet NVS unavailable; unsaved state retained in RTC.");
    return;
  }

  // Store the immutable birthday separately once. If a later progress slot
  // is damaged, falling back must not turn an established birthday into 0.
  if (!birthdayStored && petRecord.state.createdAt > 0) {
    int64_t birthday = petRecord.state.createdAt;
    int64_t verifiedBirthday = 0;
    bool savedBirthday = preferences.putBytes("birthday", &birthday, sizeof(birthday)) == sizeof(birthday) &&
                         preferences.getBytes("birthday", &verifiedBirthday, sizeof(verifiedBirthday)) == sizeof(verifiedBirthday) &&
                         verifiedBirthday == birthday;
    if (!savedBirthday) {
      preferences.end();
      checkpointRetryMs = CHECKPOINT_INTERVAL_MS;
      Serial.println("Pet birthday save failed; RTC state retained.");
      return;
    }
    birthdayStored = true;
  }

  StoredPetRecord stored = {};
  stored.record = petRecord;
  stored.generation = petGeneration + 1;
  // Counter wrap is not practical, but never emit an invalid generation.
  if (stored.generation == 0) stored.generation = 1;
  stored.checksum = recordChecksum(stored);
  const char *key = stored.generation % 2 ? "state1" : "state0";
  size_t written = preferences.putBytes(key, &stored, sizeof(stored));
  StoredPetRecord verified = {};
  bool saved = written == sizeof(stored) && readCheckpoint(preferences, key, verified) &&
               verified.generation == stored.generation && verified.checksum == stored.checksum;
  preferences.end();
  if (saved) {
    petGeneration = stored.generation;
    petDirty = false;
    meaningfulCheckpointPending = false;
    checkpointCooldownMs = CHECKPOINT_INTERVAL_MS;
    Serial.printf("Pet NVS saved (%s): friendship %u, interactions %llu, birthday %lld\n",
                  beforeSleep ? "sleep" : "checkpoint",
                  static_cast<unsigned>(petRecord.state.friendship),
                  static_cast<unsigned long long>(petRecord.state.interactions),
                  static_cast<long long>(petRecord.state.createdAt));
  } else {
    checkpointRetryMs = CHECKPOINT_INTERVAL_MS;
    Serial.println("Pet NVS save failed; previous checkpoint and RTC state retained.");
  }
}

void initializePetState() {
  bool deepSleepWake = esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED;
  bool rtcValid = deepSleepWake && validRecord(petRecord);
  StoredPetRecord newest = {};
  bool nvsValid = false;
  bool legacy = false;
  bool nvsAvailable = false;
  int64_t persistedBirthday = 0;
  birthdayStored = false;
  Preferences preferences;
  // Read/write open also creates the namespace on first use. A failed open
  // must not cause us to overwrite an unread existing pet on the next loop.
  if (preferences.begin("altoids-pet", false)) {
    nvsAvailable = true;
    birthdayStored = preferences.isKey("birthday") && preferences.getBytesLength("birthday") == sizeof(persistedBirthday) &&
                     preferences.getBytes("birthday", &persistedBirthday, sizeof(persistedBirthday)) == sizeof(persistedBirthday) &&
                     persistedBirthday > 0;
    StoredPetRecord candidate = {};
    if (readCheckpoint(preferences, "state0", candidate)) {
      newest = candidate;
      nvsValid = true;
    }
    if (readCheckpoint(preferences, "state1", candidate) &&
        (!nvsValid || candidate.generation > newest.generation)) {
      newest = candidate;
      nvsValid = true;
    }
    if (!nvsValid) {
      // Import the previous firmware's v1 record without changing its birthday.
      PetRecord restored = {};
      if (preferences.isKey("state") && preferences.getBytesLength("state") == sizeof(restored) &&
          preferences.getBytes("state", &restored, sizeof(restored)) == sizeof(restored) &&
          validRecord(restored)) {
        newest.record = restored;
        nvsValid = true;
        legacy = true;
      }
    }
    preferences.end();
  }

  const char *source;
  if (rtcValid && (!nvsValid || petGeneration > newest.generation ||
      (petGeneration == newest.generation &&
       petRecord.state.interactions >= newest.record.state.interactions &&
       petRecord.state.friendship >= newest.record.state.friendship))) {
    source = "RTC";
    if (legacy && petGeneration == 0) {
      petDirty = true;
      meaningfulCheckpointPending = true;
    }
    // Preserve a known birthday even if an older RTC state still says unknown.
    if (nvsValid && petRecord.state.createdAt == 0 && newest.record.state.createdAt > 0) {
      petRecord.state.createdAt = newest.record.state.createdAt;
      petDirty = true;
    }
  } else if (nvsValid) {
    petRecord = newest.record;
    petGeneration = newest.generation;
    petDirty = legacy;
    meaningfulCheckpointPending = legacy;
    checkpointCooldownMs = CHECKPOINT_INTERVAL_MS;
    checkpointRetryMs = 0;
    source = "NVS";
  } else {
    petRecord = {};
    petRecord.magic = PET_RECORD_MAGIC;
    petRecord.version = PET_RECORD_VERSION;
    petRecord.state.mood = PetMood::CALM;
    petGeneration = 0;
    petDirty = true;
    meaningfulCheckpointPending = true;
    checkpointCooldownMs = 0;
    checkpointRetryMs = 0;
    source = "newly initialized";
  }

  if (birthdayStored) {
    // The one-time birthday is authoritative, including when a slot is lost.
    if (petRecord.state.createdAt != persistedBirthday) {
      petRecord.state.createdAt = persistedBirthday;
      petDirty = true;
      meaningfulCheckpointPending = true;
    }
  } else if (petRecord.state.createdAt > 0) {
    petDirty = true;
    meaningfulCheckpointPending = true;
  }

  // If NVS could not be read, retain this session in RTC but do not replace an
  // unknown existing checkpoint. A later boot can try loading it again.
  petStorageAvailable = nvsAvailable;
  if (!nvsAvailable) {
    Serial.println("Pet NVS read unavailable; recovery deferred until next boot.");
  }

  // Count elapsed sleep/offline time only when the existing clock is valid.
  // Without it, RTC cooldowns count awake millis, conservatively preventing
  // sleep/wake or clock failures from bypassing the friendship rate limit.
  int64_t now = petEpoch();
  if (now > petRecord.observedEpoch && petRecord.observedEpoch > 0) {
    uint64_t elapsed = static_cast<uint64_t>(now - petRecord.observedEpoch) * 1000ULL;
    reduceCooldown(petRecord.friendshipCooldownMs, elapsed);
    reduceCooldown(checkpointCooldownMs, elapsed);
    if (nvsAvailable) reduceCooldown(checkpointRetryMs, elapsed);
  }
  lastPetUpdateMs = millis();
  interactionMoodActive = false;
  petInitialized = true;
  updatePetState();
  Serial.printf("Pet loaded from %s: friendship %u, interactions %llu, birthday %lld\n",
                source,
                static_cast<unsigned>(petRecord.state.friendship),
                static_cast<unsigned long long>(petRecord.state.interactions),
                static_cast<long long>(petRecord.state.createdAt));
  checkpointPetState();
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
    meaningfulCheckpointPending = true;
    petRecord.friendshipCooldownMs = FRIENDSHIP_INTERVAL_MS;
  }
  petDirty = true;
  Serial.printf("Pet interaction: friendship %u, interactions %llu\n",
                static_cast<unsigned>(petRecord.state.friendship),
                static_cast<unsigned long long>(petRecord.state.interactions));
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
  EAR_TWITCH
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
  if (idleAction == IdleAction::BLINK && idleMood == PetMood::SLEEPY) return 500;
  return 250;
}

uint8_t idleStepCount() {
  switch (idleAction) {
    case IdleAction::DOUBLE_BLINK: return 3; // shut, open, shut
    case IdleAction::BOUNCE: return 3;       // up 1, up 2, up 1
    case IdleAction::EAR_TWITCH: return 2;   // tip shifts 1 pixel, then 2
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
  int lookOffset = 0;
  int earOffset = 0;
  if (expressIdle) {
    if (idleAction == IdleAction::LOOK_LEFT) lookOffset = -2;
    if (idleAction == IdleAction::LOOK_RIGHT) lookOffset = 2;
    if (idleAction == IdleAction::BOUNCE) y -= idleStep == 1 ? 2 : 1;
    if (idleAction == IdleAction::EAR_TWITCH) earOffset = idleStep + 1;
  }

  // ears

  display.drawTriangle(
    x + 6, y + 12,
    x + 14 + earOffset, y,
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
      x + 16 + lookOffset,
      y + 24,
      2,
      SH110X_WHITE
    );

    display.fillCircle(
      x + 32 + lookOffset,
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
    idleMood = mood;
    scheduleIdle(now);
    // Keep the existing first/return-home blink deadline. Mood transitions
    // otherwise use their new cadence; an active action completes naturally.
    if (!idlePaused && idleAction == IdleAction::REST) scheduleBlink(now);
    idlePaused = false;
  }

  if (idleAction != IdleAction::REST) {
    if (now - idleStepStarted >= idleStepDuration()) {
      idleStepStarted = now;
      if (++idleStep >= idleStepCount()) {
        bool wasBlink = idleAction == IdleAction::BLINK || idleAction == IdleAction::DOUBLE_BLINK;
        bool wasIdleAction = idleAction != IdleAction::BLINK;
        idleAction = IdleAction::REST;
        idleStep = 0;
        if (wasBlink) scheduleBlink(now);
        if (wasIdleAction) scheduleIdle(now);
      }
    }
  } else {
    IdleAction action = IdleAction::REST;
    if (idleDue(now, nextBlinkTime)) {
      action = IdleAction::BLINK;
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

  checkpointPetState();
}
