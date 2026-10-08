#include "display.h"
#include "pet.h"
#include "weather.h"
#include "power.h"
#include "timer.h"
#include "sound.h"
#include "journal.h"
#include "journal_ui.h"
#include "buddy_setup.h"

void setup() {
  // Keep the TFT dark during the existing Serial/clock/cache startup path.
  initializeDisplayBus();
  Serial.begin(115200);
  delay(500);

  // Restore Central timezone on every boot, including deep-sleep wake.
  configureTimezone();
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
  if (!beginBuddySetup() && !showTimerScreen()) drawHome();
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
  updateBuddySetup();
  sleepIfIdle();
}
