#include <assert.h>
#include "Arduino.h"
#include "display.h"
SerialType Serial;
unsigned long fakeMillis=500;
int pinLevels[4]={HIGH,HIGH,HIGH,HIGH};
DisplaySurface fakeDisplay;
DisplaySurface &display=fakeDisplay;
void checkpointJournal(bool) {}
void stopSound() {}
void sleepDisplay() {}
bool timerActive() {return false;}
#include "../../firmware/altoids_pet/power.cpp"

void reset() {
  for(int &level:pinLevels)level=HIGH;
  fakeMillis=500;lastActivityTime=0;initializeButtons();
}
ButtonPresses tick(uint32_t elapsed) {
  fakeMillis=static_cast<uint32_t>(fakeMillis+elapsed);
  return readButtons();
}
void noEvent(ButtonPresses b) {assert(!b.a && !b.b && !b.c);}
int main() {
  // Press is immediate; holding does not auto-repeat, even past debounce.
  reset();pinLevels[D0]=LOW;assert(tick(1).a);unsigned long activity=lastActivityTime;
  noEvent(tick(100));noEvent(tick(1000));assert(lastActivityTime==activity);
  // Releasing after a long hold, including contact bounce, must not fire again.
  pinLevels[D0]=HIGH;noEvent(tick(1));pinLevels[D0]=LOW;noEvent(tick(5));
  pinLevels[D0]=HIGH;noEvent(tick(2));pinLevels[D0]=LOW;noEvent(tick(20));
  pinLevels[D0]=HIGH;noEvent(tick(1));noEvent(tick(119));noEvent(tick(1));
  assert(lastActivityTime==activity);pinLevels[D0]=LOW;assert(tick(1).a);
  // Press bounce is absorbed by the same latch, with no delayed second event.
  reset();pinLevels[D0]=LOW;assert(tick(1).a);
  for(int i=0;i<10;i++){pinLevels[D0]=HIGH;noEvent(tick(3));pinLevels[D0]=LOW;noEvent(tick(3));}
  noEvent(tick(500));pinLevels[D0]=HIGH;noEvent(tick(1));noEvent(tick(120));
  pinLevels[D0]=LOW;assert(tick(1).a);
  // Stable release is mandatory; a too-short release doesn't rearm.
  reset();pinLevels[D0]=LOW;assert(tick(1).a);
  pinLevels[D0]=HIGH;noEvent(tick(1));noEvent(tick(119));pinLevels[D0]=LOW;noEvent(tick(1));
  // Buttons debounce independently, including combined samples.
  reset();pinLevels[D0]=pinLevels[D1]=LOW;auto b=tick(1);assert(b.a && b.b && !b.c);
  pinLevels[D2]=LOW;b=tick(1);assert(!b.a && !b.b && b.c);noEvent(tick(500));
  // Held-at-boot/wake B must release before another HOME action.
  reset();pinLevels[D1]=LOW;initializeButtons();noEvent(tick(1000));
  pinLevels[D1]=HIGH;noEvent(tick(1));noEvent(tick(120));pinLevels[D1]=LOW;assert(tick(1).b);
  // The 120 ms release timer is correct across the ESP32's millis rollover.
  reset();pinLevels[D0]=LOW;assert(tick(1).a);
  fakeMillis=UINT32_MAX-50;pinLevels[D0]=HIGH;noEvent(tick(0));
  noEvent(tick(119));assert(!buttonA.armed);noEvent(tick(1));assert(buttonA.armed);
  pinLevels[D0]=LOW;assert(tick(1).a);
  // Many full cycles produce exactly one event per cycle, none on release.
  reset();unsigned events=0;
  for(int i=0;i<100;i++) {
    pinLevels[D0]=LOW;events+=tick(1).a;events+=tick(300).a;
    pinLevels[D0]=HIGH;events+=tick(1).a;events+=tick(120).a;
  }
  assert(events==100);
  puts("PASS: immediate press, held/release/press bounce suppression, stable-release rearming, independent buttons, held wake suppression, activity updates, millis rollover and one event per full cycle.");
}
