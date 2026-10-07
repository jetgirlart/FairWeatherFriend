#include "power.h"
#include "config.h"
#include "display.h"
#include "pet.h"
#include <esp_sleep.h>

// ==================================================
// BUTTONS
// ==================================================

const int BUTTON_A = D0;
const int BUTTON_B = D1;
const int BUTTON_C = D2;

bool lastA = HIGH;
bool lastB = HIGH;
bool lastC = HIGH;

unsigned long lastButtonTime = 0;
const unsigned long debounceTime = 120;

unsigned long lastActivityTime = 0;

// ==================================================
// BUTTON HANDLING
// ==================================================

bool pressed(
  int pin,
  bool &lastState
) {

  bool currentState =
    digitalRead(pin);

  bool wasPressed =
    false;

  if (
    lastState == HIGH &&
    currentState == LOW &&
    millis() - lastButtonTime >
      debounceTime
  ) {

    wasPressed =
      true;

    lastButtonTime =
      millis();

    lastActivityTime =
      millis();
  }

  lastState =
    currentState;

  return wasPressed;
}

// ==================================================
// DEEP SLEEP
// ==================================================

void goToSleep() {

  Serial.println(
    "Going to sleep..."
  );

  checkpointPetState(true);

  // OLED completely off.

  display.oled_command(
    SH110X_DISPLAYOFF
  );

  delay(
    100
  );

  // B button wakes the ESP32.

  esp_sleep_enable_ext0_wakeup(
    (gpio_num_t)BUTTON_B,
    0
  );

  Serial.println(
    "Press B to wake."
  );

  delay(
    100
  );

  esp_deep_sleep_start();
}

void initializeButtons() {
  // Buttons.

  pinMode(
    BUTTON_A,
    INPUT_PULLUP
  );

  pinMode(
    BUTTON_B,
    INPUT_PULLUP
  );

  pinMode(
    BUTTON_C,
    INPUT_PULLUP
  );
}

bool wokeFromButton() {
  // Determine why we booted.

  esp_sleep_wakeup_cause_t wakeCause =
    esp_sleep_get_wakeup_cause();

  bool wokeFromButton =
    wakeCause ==
    ESP_SLEEP_WAKEUP_EXT0;

  return wokeFromButton;
}

ButtonPresses readButtons() {
  bool aPressed =
    pressed(
      BUTTON_A,
      lastA
    );

  bool bPressed =
    pressed(
      BUTTON_B,
      lastB
    );

  bool cPressed =
    pressed(
      BUTTON_C,
      lastC
    );

  return {aPressed, bPressed, cPressed};
}

void sleepIfIdle() {
  // ----------------------------------------------
  // AUTO DEEP SLEEP
  // ----------------------------------------------

  if (
    millis() -
      lastActivityTime >
      IDLE_SLEEP_MS
  ) {

    goToSleep();
  }
}
