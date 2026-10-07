#pragma once

#include <Arduino.h>

struct ButtonPresses {
  bool a;
  bool b;
  bool c;
};

extern unsigned long lastActivityTime;

void initializeButtons();
bool wokeFromButton();
ButtonPresses readButtons();
void sleepIfIdle();
void goToSleep();
