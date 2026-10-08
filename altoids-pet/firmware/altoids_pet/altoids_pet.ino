#include "display.h"
#include "pet.h"
#include "weather.h"
#include "power.h"
#include "timer.h"
#include "sound.h"
#include "journal.h"
#include "journal_ui.h"

void setup() {
  Serial.begin(115200);
  delay(500);

  // Restore Central timezone on every boot, including deep-sleep wake.
  configureTimezone();
  initializeDisplayBus();
  initializeButtons();
  initializeDisplay();
  initializeJournal();
  initializeWeather(wokeFromButton());
  initializePetState();

  lastActivityTime = millis();
  lastDisplayedMinute = currentMinute;
  initializeAnimations();
  initializeSound();
  initializeTimer();
  if (!showTimerScreen()) drawHome();
  soundStartup();
}

void loop() {
  // Poll all buttons before acting on A, B, then C, as in the prototype.
  ButtonPresses buttons = readButtons();
  handleButtons(buttons.a, buttons.b, buttons.c);
  updatePetReaction();
  checkClock();
  updateAnimations();
  updateTimer();
  updateSound();
  updateJournal();
  updateJournalScreens();
  sleepIfIdle();
}
