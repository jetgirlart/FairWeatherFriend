#include "timer.h"
#include "display.h"
#include "pet.h"
#include "power.h"
#include "weather.h"
#include "sound.h"
#include <esp_attr.h>
#include <stddef.h>
#include <stdint.h>

namespace {
const uint32_t TIMER_MAGIC = 0x464F4331;
const uint32_t TIMER_VERSION = 1;
const uint32_t PRESET_MINUTES[] = {5, 10, 15, 25};
const uint32_t MAX_DURATION_MS = 25UL * 60UL * 1000UL;
enum class TimerPhase : uint8_t { READY, RUNNING, DONE };

struct TimerRecord {
  uint32_t magic;
  uint32_t version;
  int64_t deadlineUtc;
  uint32_t remainingMs;
  uint8_t preset;
  TimerPhase phase;
  uint32_t checksum;
};

// NOINIT preserves this region on supported warm resets as well as deep sleep.
// It is never trusted on power-up without validating magic/version/checksum.
RTC_NOINIT_ATTR TimerRecord retainedTimer;
TimerRecord timer = {};
uint32_t lastTickMs = 0;
uint32_t lastDrawnSeconds = UINT32_MAX;
uint32_t doneStartedMs = 0;
uint32_t lastDoneFrame = UINT32_MAX;
bool initialized = false;

uint32_t checksum(const TimerRecord &record) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
  uint32_t value = 2166136261UL;
  for (size_t i = 0; i < offsetof(TimerRecord, checksum); ++i) {
    value = (value ^ bytes[i]) * 16777619UL;
  }
  return value;
}

bool valid(const TimerRecord &record) {
  return record.magic == TIMER_MAGIC && record.version == TIMER_VERSION &&
         record.preset < 4 && record.phase <= TimerPhase::DONE &&
         record.remainingMs <= MAX_DURATION_MS && record.deadlineUtc >= 0 &&
         record.checksum == checksum(record);
}

void saveTimer() {
  timer.checksum = checksum(timer);
  retainedTimer = timer;
}

uint32_t secondsRemaining() {
  return (timer.remainingMs + 999UL) / 1000UL;
}

int64_t clockEpoch() {
  return timeValid ? static_cast<int64_t>(time(nullptr)) : 0;
}

void finishTimer() {
  timer.phase = TimerPhase::DONE;
  timer.remainingMs = 0;
  timer.deadlineUtc = 0;
  doneStartedMs = static_cast<uint32_t>(millis());
  lastDoneFrame = UINT32_MAX;
  lastActivityTime = millis(); // Give DONE its own normal 30-second viewing window.
  currentScreen = TIMER_DONE;
  saveTimer();
  Serial.println("Focus timer done.");
  soundTimerDone();
}
} // namespace

bool timerActive() {
  return initialized && timer.phase == TimerPhase::RUNNING;
}

void initializeTimer() {
  timer = {};
  timer.magic = TIMER_MAGIC;
  timer.version = TIMER_VERSION;
  timer.phase = TimerPhase::READY;
  if (valid(retainedTimer)) timer = retainedTimer;
  initialized = true;
  lastTickMs = static_cast<uint32_t>(millis());
  lastDrawnSeconds = UINT32_MAX;
  lastDoneFrame = UINT32_MAX;
  doneStartedMs = lastTickMs;

  if (timer.phase == TimerPhase::RUNNING) {
    int64_t now = clockEpoch();
    if (now > 0 && timer.deadlineUtc > 0) {
      int64_t seconds = timer.deadlineUtc - now;
      // Clamp against the checkpoint so a backward wall-clock jump cannot
      // extend the focus session beyond its saved remaining duration.
      if (seconds <= 0) timer.remainingMs = 0;
      else if (seconds <= MAX_DURATION_MS / 1000) {
        uint32_t resumedMs = static_cast<uint32_t>(seconds) * 1000UL;
        if (resumedMs < timer.remainingMs) timer.remainingMs = resumedMs;
      }
    }
    if (timer.remainingMs == 0) finishTimer();
    else Serial.printf("Focus timer resumed: %lu seconds.\n", static_cast<unsigned long>(secondsRemaining()));
  }
  saveTimer();
}

void openTimerSetup() {
  timer.phase = TimerPhase::READY;
  timer.remainingMs = 0;
  timer.deadlineUtc = 0;
  saveTimer();
  currentScreen = TIMER_SETUP;
  drawTimerSetup(PRESET_MINUTES[timer.preset]);
}

bool showTimerScreen() {
  if (timerActive()) {
    currentScreen = FOCUS_SCREEN;
    lastDrawnSeconds = secondsRemaining();
    drawFocusTimer(lastDrawnSeconds);
    return true;
  }
  if (initialized && timer.phase == TimerPhase::DONE) {
    currentScreen = TIMER_DONE;
    drawTimerDone(0);
    lastDoneFrame = 0;
    return true;
  }
  return false;
}

bool handleTimerButtons(bool aPressed, bool bPressed, bool cPressed) {
  if (currentScreen != TIMER_SETUP && currentScreen != FOCUS_SCREEN && currentScreen != TIMER_DONE) return false;

  if (cPressed) {
    bool wasDone = currentScreen == TIMER_DONE;
    timer.phase = TimerPhase::READY;
    timer.remainingMs = 0;
    timer.deadlineUtc = 0;
    saveTimer();
    if (wasDone) {
      currentScreen = HOME;
      petReacting = false;
      blinking = false;
      nextBlinkTime = millis() + 3000;
      drawHome();
    } else {
      currentScreen = MENU;
      drawMenu();
    }
    return true;
  }

  if (currentScreen == TIMER_SETUP) {
    if (aPressed) {
      timer.preset = (timer.preset + 1) % 4;
      saveTimer();
      drawTimerSetup(PRESET_MINUTES[timer.preset]);
    }
    if (bPressed) {
      timer.phase = TimerPhase::RUNNING;
      timer.remainingMs = PRESET_MINUTES[timer.preset] * 60UL * 1000UL;
      int64_t now = clockEpoch();
      timer.deadlineUtc = now > 0 ? now + secondsRemaining() : 0;
      lastTickMs = static_cast<uint32_t>(millis());
      saveTimer();
      Serial.printf("Focus timer started: %lu minutes.\n", static_cast<unsigned long>(PRESET_MINUTES[timer.preset]));
      showTimerScreen();
    }
  }
  // A/B during RUNNING or DONE do not restart or interact with the pet.
  return true;
}

void updateTimer() {
  if (!initialized) return;
  uint32_t nowMs = static_cast<uint32_t>(millis());
  if (timerActive()) {
    uint32_t elapsed = nowMs - lastTickMs; // Correct across millis rollover.
    lastTickMs = nowMs;
    timer.remainingMs = elapsed >= timer.remainingMs ? 0 : timer.remainingMs - elapsed;
    int64_t now = clockEpoch();
    if (timer.deadlineUtc == 0 && now > 0) timer.deadlineUtc = now + secondsRemaining();
    saveTimer(); // RTC RAM only; no flash writes.
    if (timer.remainingMs == 0) finishTimer();
    else if (currentScreen == FOCUS_SCREEN && secondsRemaining() != lastDrawnSeconds) {
      lastDrawnSeconds = secondsRemaining();
      drawFocusTimer(lastDrawnSeconds);
    }
  }
  if (timer.phase == TimerPhase::DONE && currentScreen == TIMER_DONE) {
    uint32_t age = static_cast<uint32_t>(millis()) - doneStartedMs;
    uint32_t frame = age < 1500 ? age / 250 : 6;
    if (frame != lastDoneFrame) {
      lastDoneFrame = frame;
      drawTimerDone(frame);
    }
  }
}
