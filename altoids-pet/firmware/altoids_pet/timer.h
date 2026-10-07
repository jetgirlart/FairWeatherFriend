#pragma once

#include <Arduino.h>

void initializeTimer();
void openTimerSetup();
bool showTimerScreen();
bool handleTimerButtons(bool aPressed, bool bPressed, bool cPressed);
void updateTimer();
bool timerActive();
