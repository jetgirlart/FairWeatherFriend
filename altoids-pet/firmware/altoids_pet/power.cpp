#include "power.h"
#include "config.h"
#include "display.h"
#include "pet.h"
#include "journal.h"
#include "timer.h"
#include "sound.h"
#include <esp_sleep.h>
#include <driver/rtc_io.h>

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
  // EXT0 is level-triggered: entering sleep with B already LOW wakes instantly.
  // Defer before switching off the OLED, even if the button is held/stuck LOW.
  if (digitalRead(BUTTON_B) == LOW) {
    Serial.println("Sleep deferred: B is LOW; release B to allow sleep.");
    lastActivityTime = millis();
    return;
  }

  gpio_num_t wakePin = static_cast<gpio_num_t>(BUTTON_B);
  esp_err_t result = rtc_gpio_init(wakePin);
  if (result == ESP_OK) result = rtc_gpio_set_direction(wakePin, RTC_GPIO_MODE_INPUT_ONLY);
  if (result == ESP_OK) result = rtc_gpio_pulldown_dis(wakePin);
  if (result == ESP_OK) result = rtc_gpio_pullup_en(wakePin);
  if (result == ESP_OK) result = esp_sleep_enable_ext0_wakeup(wakePin, 0);
  if (result != ESP_OK) {
    rtc_gpio_deinit(wakePin);
    pinMode(BUTTON_B, INPUT_PULLUP);
    Serial.printf("Sleep deferred: RTC wake setup failed (%d).\n", static_cast<int>(result));
    lastActivityTime = millis();
    return;
  }

  // Use the existing first 100 ms pause to settle the RTC pull-up and check
  // again before blanking the display. A button/noise during setup stays awake.
  delay(100);
  if (rtc_gpio_get_level(wakePin) == LOW) {
    rtc_gpio_deinit(wakePin);
    pinMode(BUTTON_B, INPUT_PULLUP);
    Serial.println("Sleep deferred: RTC B input is LOW.");
    lastActivityTime = millis();
    return;
  }

  Serial.println("Going to sleep... B wake input HIGH.");
  checkpointJournal(true);

  stopSound();
  display.oled_command(SH110X_DISPLAYOFF);
  Serial.println("Press B to wake.");
  delay(100);
  esp_deep_sleep_start();
}

void initializeButtons() {
  // EXT0 leaves B routed through RTC IO. Restore normal digital reads first.
  rtc_gpio_deinit(static_cast<gpio_num_t>(BUTTON_B));

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
  if (timerActive()) return;
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
