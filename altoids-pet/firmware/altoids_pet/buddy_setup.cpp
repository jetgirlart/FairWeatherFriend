#include "buddy_setup.h"
#include "display.h"
#include "journal.h"
#include "sprites.h"
#include "timer.h"

namespace {
enum class SetupPage : uint8_t { WELCOME, FUR, READY };
SetupPage page = SetupPage::WELCOME;
FurPaletteId highlighted = FurPaletteId::ORANGE;
bool saveFailed = false;
unsigned long readyAt = 0;
void text(int y, const char *value, uint8_t size = 2) {
  display.setTextSize(size); display.setTextColor(COLOR_TEXT);
  display.setCursor((TFT_WIDTH - strlen(value) * 6 * size) / 2, y); display.print(value);
}
void drawSetup() {
  display.clearDisplay();
  if (page == SetupPage::WELCOME) {
    text(15, "WELCOME", 3); text(52, "FAIRWEATHER FRIEND");
    text(175, "Your field buddy"); text(214, "B: BEGIN");
  } else if (page == SetupPage::FUR) {
    text(15, "FUR COLOR", 3); text(58, furPaletteName(highlighted));
    text(185, saveFailed ? "SAVE FAILED: RETRY B" : "A: COLOR B: CONFIRM");
    text(214, "C: BACK");
  } else {
    text(18, "READY", 3); text(61, "Ready for"); text(81, "field work!");
  }
  // Preview the actual expression mask and role renderer, with no saved gear.
  display.beginPet(84, page == SetupPage::READY ? 106 : 89);
  drawColoredKitsune(page == SetupPage::READY ? KITSUNE_HAPPY : KITSUNE_IDLE,
                     0, 0, highlighted);
  display.endPet(); display.display();
}
void finish() {
  currentScreen = HOME;
  if (!showTimerScreen()) drawHome();
}
}
bool buddySetupActive() { return currentScreen == BUDDY_SETUP_SCREEN; }
bool beginBuddySetup() {
  if (!buddyNeedsSetup()) return false;
  currentScreen = BUDDY_SETUP_SCREEN;
  page = SetupPage::WELCOME; highlighted = getBuddySave().furPalette; saveFailed = false;
  drawSetup(); return true;
}
bool handleBuddySetupButtons(bool aPressed, bool bPressed, bool cPressed) {
  if (!buddySetupActive()) return false;
  if (page == SetupPage::READY) return true; // Already committed; no back/second save.
  if (cPressed && page == SetupPage::FUR) { page = SetupPage::WELCOME; saveFailed = false; drawSetup(); }
  else if (bPressed) {
    if (page == SetupPage::WELCOME) { page = SetupPage::FUR; drawSetup(); }
    else if (confirmBuddySetup(highlighted)) {
      page = SetupPage::READY; readyAt = millis(); drawSetup();
    } else { saveFailed = true; drawSetup(); }
  } else if (aPressed && page == SetupPage::FUR) {
    highlighted = static_cast<FurPaletteId>((static_cast<uint8_t>(highlighted) + 1) % FUR_PALETTE_COUNT);
    saveFailed = false; drawSetup();
  }
  return true;
}
void updateBuddySetup() {
  if (!buddySetupActive()) return;
  // Confirmed USB imports can replace a pending buddy while setup is open.
  if (!buddyNeedsSetup() && page != SetupPage::READY) { finish(); return; }
  if (page == SetupPage::READY && millis() - readyAt >= 1600) finish();
}
