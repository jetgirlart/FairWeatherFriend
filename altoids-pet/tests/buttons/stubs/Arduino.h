#pragma once
#include "../../journal/stubs/Arduino.h"
#define D0 1
#define D1 2
#define D2 3
#define HIGH 1
#define LOW 0
#define INPUT_PULLUP 2
extern int pinLevels[4];
inline int digitalRead(int pin) {return pinLevels[pin];}
inline void pinMode(int,int) {}
inline void delay(unsigned long) {}

#define D4 5
#define D5 6
#define D6 43
#define D7 44
#define D8 7
#define D9 8
#define D10 9
