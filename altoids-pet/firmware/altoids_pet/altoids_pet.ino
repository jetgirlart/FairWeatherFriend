#include "display.h"
#include "pet.h"
#include "weather.h"
#include "power.h"

void setup() {
  Serial.begin(115200);
  delay(500);

  // Restore Central timezone on every boot, including deep-sleep wake.
  configureTimezone();
  initializeDisplayBus();
  initializeButtons();
  initializeDisplay();
  initializeWeather(wokeFromButton());
  initializePetState();

  lastActivityTime = millis();
  lastDisplayedMinute = currentMinute;
  initializeAnimations();
  drawHome();
}

void loop() {
  // Poll all buttons before acting on A, B, then C, as in the prototype.
  ButtonPresses buttons = readButtons();
  handleButtons(buttons.a, buttons.b, buttons.c);
  updatePetReaction();
  checkClock();
  updateAnimations();
  sleepIfIdle();
}
