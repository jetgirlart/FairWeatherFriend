#include "journal_ui.h"
#include "journal.h"
#include "gear.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

namespace {
uint8_t journalPage = 0;
GearId selectedGear = GearId::NONE;
GearSlot selectedSlot = GearSlot::HEAD;
bool choosingItem = false;
bool equipmentSaveFailed = false;
uint32_t displayedChecksum = 0;
bool displayedAvailable = false;
unsigned long lastRefresh = 0;

const char *categoryLabels[] = {
  "CLEAR", "MAINLY CLEAR", "PART CLOUD", "CLOUDY", "RAIN", "STORM", "SNOW", "FOG"
};
const char *gearLabels[] = {
  "NONE", "FIELD CAP", "SUNGLASSES", "UMBRELLA", "RAINCOAT", "WINTER SCARF", "WINTER COAT", "BOOTS"
};

void textAt(int x, int y, const char *text, uint8_t size = 1) {
  display.setTextSize(size * 2);
  int width = strlen(text) * 12 * size;
  int left = x * 2;
  if (left + width > TFT_WIDTH) left = TFT_WIDTH - width;
  display.setCursor(left < 0 ? 0 : left, y * 2);
  display.print(text);
}
void centered(int y, const char *text, uint8_t size = 1) {
  // Small text for wide values; exact uint64 totals still fit in 120 pixels.
  if (strlen(text) * 12 * size > TFT_WIDTH - 8) size = 1;
  display.setTextSize(size * 2);
  display.setCursor((TFT_WIDTH - int(strlen(text) * 12 * size)) / 2, y * 2);
  display.print(text);
}
void beginScreen(const char *title) {
  display.clearDisplay(); // RAM only; one TFT update at finishScreen().
  display.setTextColor(COLOR_TEXT);
  centered(5, title, 2);
}
void finishScreen() {
  displayedAvailable = journalAvailable();
  displayedChecksum = buddySaveChecksum(getBuddySave());
  lastRefresh = millis();
  display.display();
}
bool unavailable() {
  if (journalAvailable()) return false;
  centered(46, "JOURNAL UNAVAILABLE");
  centered(65, "SAVE PRESERVED");
  centered(111, "C:MENU");
  return true;
}
void formatDate(int64_t timestamp, char *text, size_t length, bool withTime = false) {
  time_t epoch = static_cast<time_t>(timestamp);
  struct tm local = {};
  if (timestamp <= 0 || !localtime_r(&epoch, &local) ||
      strftime(text, length, withTime ? "%Y-%m-%d %H:%M" : "%Y-%m-%d", &local) == 0) {
    snprintf(text, length, "WAITING FOR TIME");
  }
}
void formatTemperature(int32_t deciF, char *text, size_t length) {
  int64_t magnitude = deciF;
  if (magnitude < 0) magnitude = -magnitude;
  snprintf(text, length, "%s%lld.%lld F", deciF < 0 ? "-" : "",
           static_cast<long long>(magnitude / 10), static_cast<long long>(magnitude % 10));
}
void noObservations() {
  centered(44, "NO OBSERVATIONS YET");
  centered(67, "Waiting for a");
  centered(80, "live weather fetch");
}
void drawJournal() {
  beginScreen("JOURNAL");
  if (!unavailable()) {
    const BuddySaveData &data = getBuddySave();
    char text[32];
    if (journalPage == 0) {
      textAt(4, 27, "SUMMARY 1/4");
      centered(39, "OBSERVATIONS");
      snprintf(text, sizeof(text), "%llu", static_cast<unsigned long long>(data.totalObservations));
      centered(50, text, 2);
      snprintf(text, sizeof(text), "DAYS %lu", static_cast<unsigned long>(data.uniqueDaysObserved));
      textAt(4, 71, text);
      unsigned discovered = 0;
      for (uint8_t i = 0; i < WEATHER_CATEGORY_COUNT; ++i) if (data.discoveredWeather & (1UL << i)) ++discovered;
      snprintf(text, sizeof(text), "WEATHER TYPES %u/8", discovered);
      textAt(4, 80, text);
      textAt(4, 89, "FIELD RESEARCH BEGAN");
      formatDate(data.createdAt, text, sizeof(text));
      textAt(4, 99, text);
    } else if (journalPage == 1) {
      textAt(4, 27, "LATEST 2/4");
      if (data.totalObservations == 0) noObservations();
      else {
        formatDate(data.latestObservationAt, text, sizeof(text), true);
        centered(41, text);
        formatTemperature(data.latestTemperatureDeciF, text, sizeof(text));
        centered(61, text, 2);
        uint8_t category = static_cast<uint8_t>(data.latestCategory);
        centered(85, category < WEATHER_CATEGORY_COUNT ? categoryLabels[category] : "UNKNOWN");
        snprintf(text, sizeof(text), "CODE %ld", static_cast<long>(data.latestWeatherCode));
        centered(100, text);
      }
    } else {
      snprintf(text, sizeof(text), "WEATHER %u/4", unsigned(journalPage + 1));
      textAt(4, 25, text);
      uint8_t first = (journalPage - 2) * 4;
      for (uint8_t row = 0; row < 4; ++row) {
        uint8_t category = first + row;
        int y = 37 + row * 19;
        textAt(4, y, categoryLabels[category]);
        if (data.discoveredWeather & (1UL << category)) display.fillCircle(224, y * 2 + 6, 3, COLOR_COOL);
        else display.drawCircle(224, y * 2 + 6, 3, COLOR_MUTED);
        snprintf(text, sizeof(text), "%llu", static_cast<unsigned long long>(data.weatherCounts[category]));
        centered(y + 9, text);
      }
    }
    centered(111, "A:NEXT C:MENU");
  }
  finishScreen();
}
void drawRecords() {
  beginScreen("RECORDS");
  if (!unavailable()) {
    const BuddySaveData &data = getBuddySave();
    if (data.totalObservations == 0) noObservations();
    else {
      char text[32];
      centered(29, "HIGHEST");
      formatTemperature(data.highestTemperatureDeciF, text, sizeof(text));
      centered(40, text, 2);
      formatDate(data.highestTemperatureAt, text, sizeof(text), true);
      centered(59, text);
      centered(73, "LOWEST");
      formatTemperature(data.lowestTemperatureDeciF, text, sizeof(text));
      centered(84, text, 2);
      formatDate(data.lowestTemperatureAt, text, sizeof(text), true);
      centered(101, text);
    }
    centered(111, "C:MENU");
  }
  finishScreen();
}
void gearRequirement(GearId gear) {
  switch (gear) {
    case GearId::NONE: centered(79, "Remove equipped gear"); break;
    case GearId::FIELD_CAP: centered(79, "First observation"); break;
    case GearId::SUNGLASSES: centered(79, "First clear or"); centered(91, "mainly clear weather"); break;
    case GearId::UMBRELLA: centered(79, "First rain"); centered(91, "observation"); break;
    case GearId::RAINCOAT: centered(79, "10 rain observations"); break;
    case GearId::WINTER_SCARF: centered(79, "First snow"); centered(91, "observation"); break;
    case GearId::WINTER_COAT: centered(79, "5 snow observations"); break;
    case GearId::BOOTS: centered(79, "First below-freezing"); centered(91, "observation"); break;
  }
}
bool selectable(GearId gear) {
  return gearFitsSlot(gear, selectedSlot) &&
         (gear == GearId::NONE || (getBuddySave().unlockedGear & gearFlag(gear)));
}
void drawGear() {
  beginScreen("GEAR");
  if (!unavailable()) {
    const BuddySaveData &data = getBuddySave();
    if (!choosingItem) {
      for (uint8_t i = 0; i < GEAR_SLOT_COUNT; ++i) {
        char row[22];
        snprintf(row, sizeof(row), "%s %-4s %s", i == static_cast<uint8_t>(selectedSlot) ? ">" : " ",
                 gearSlotName(static_cast<GearSlot>(i)), gearLabels[static_cast<uint8_t>(data.equippedSlots[i])]);
        textAt(0, 29 + i * 12, row);
      }
      centered(99, "A:NEXT B:OPEN");
      centered(111, "C:MENU");
    } else {
      if (!selectable(selectedGear)) selectedGear = GearId::NONE;
      centered(27, gearSlotName(selectedSlot));
      centered(40, gearLabels[static_cast<uint8_t>(selectedGear)], 2);
      centered(61, equipmentSaveFailed ? "SAVE FAILED" :
                   selectedGear == data.equippedSlots[static_cast<uint8_t>(selectedSlot)] ? "EQUIPPED" : "UNLOCKED");
      gearRequirement(selectedGear);
      centered(99, "A:NEXT B:EQUIP");
      centered(111, "C:SLOTS");
    }
  }
  finishScreen();
}
void drawCurrent() {
  if (currentScreen == JOURNAL_SCREEN) drawJournal();
  else if (currentScreen == RECORDS_SCREEN) drawRecords();
  else if (currentScreen == GEAR_SCREEN) drawGear();
}
} // namespace

bool isJournalScreen() {
  return currentScreen == JOURNAL_SCREEN || currentScreen == RECORDS_SCREEN || currentScreen == GEAR_SCREEN;
}
void openJournalScreen(ScreenMode screen) {
  if (screen != JOURNAL_SCREEN && screen != RECORDS_SCREEN && screen != GEAR_SCREEN) return;
  currentScreen = screen;
  journalPage = 0;
  selectedSlot = GearSlot::HEAD;
  choosingItem = false;
  selectedGear = GearId::NONE;
  equipmentSaveFailed = false;
  drawCurrent();
}
bool handleJournalButtons(bool aPressed, bool bPressed, bool cPressed) {
  if (!isJournalScreen()) return false;
  bool redraw = false;
  if (aPressed) {
    if (currentScreen == JOURNAL_SCREEN) { journalPage = (journalPage + 1) % 4; redraw = true; }
    if (currentScreen == GEAR_SCREEN && journalAvailable()) {
      if (!choosingItem) selectedSlot = static_cast<GearSlot>((static_cast<uint8_t>(selectedSlot) + 1) % GEAR_SLOT_COUNT);
      else {
        do { selectedGear = static_cast<GearId>((static_cast<uint8_t>(selectedGear) + 1) % 8); }
        while (!selectable(selectedGear)); // NONE always terminates the search.
      }
      equipmentSaveFailed = false; redraw = true;
    }
  }
  if (bPressed && currentScreen == GEAR_SCREEN && journalAvailable()) {
    if (!choosingItem) {
      choosingItem = true;
      selectedGear = getBuddySave().equippedSlots[static_cast<uint8_t>(selectedSlot)];
    } else if (selectable(selectedGear)) equipmentSaveFailed = !equipGear(selectedSlot, selectedGear);
    redraw = true;
  }
  if (cPressed) {
    if (currentScreen == GEAR_SCREEN && choosingItem) {
      choosingItem = false; equipmentSaveFailed = false; drawGear();
    } else { currentScreen = MENU; drawMenu(); }
  }
  else if (redraw) drawCurrent();
  return true;
}
void updateJournalScreens() {
  if (!isJournalScreen() || millis() - lastRefresh < 250) return;
  lastRefresh = millis();
  if (displayedAvailable != journalAvailable() || displayedChecksum != buddySaveChecksum(getBuddySave())) {
    equipmentSaveFailed = false;
    drawCurrent(); // Imports/creation-date capture can change an open screen.
  }
}
