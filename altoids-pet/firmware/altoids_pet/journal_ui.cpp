#include "journal_ui.h"
#include "journal.h"
#include "gear.h"
#include "units.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

namespace {
uint8_t journalPage = 0;
uint8_t recordsPage = 0;
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
  formatBuddyTemperature(deciF, text, length);
}
void latestMetricRow(const BuddySaveData &data, MetricId metric, const char *label, int y) {
  char value[32], row[48];
  if (data.latestMetrics.has(metric)) formatBuddyMetric(metric, data.latestMetrics.values[uint8_t(metric)], value, sizeof(value));
  else snprintf(value, sizeof(value), "--");
  snprintf(row, sizeof(row), "%s %s", label, value); centered(y, row);
}
void noObservations() {
  centered(44, "NO OBSERVATIONS YET");
  centered(67, "Waiting for a");
  centered(80, "live weather fetch");
}
void drawJournal() {
  beginScreen(journalPage >= 5 ? "SEVERE WEATHER" : "JOURNAL");
  if (!unavailable()) {
    const BuddySaveData &data = getBuddySave();
    char text[32];
    if (journalPage == 0) {
      textAt(4, 27, "SUMMARY 1/10");
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
    } else if (journalPage == 1 || journalPage == 2) {
      textAt(4, 27, journalPage == 1 ? "CONDITIONS 2/10" : "AIR & WIND 3/10");
      if (data.totalObservations == 0) noObservations();
      else {
        if (journalPage == 1) {
          uint8_t category = static_cast<uint8_t>(data.latestCategory);
          centered(39, category < WEATHER_CATEGORY_COUNT ? categoryLabels[category] : "UNKNOWN");
          formatTemperature(data.latestTemperatureMilliC, text, sizeof(text)); centered(51, text, 2);
          latestMetricRow(data, MetricId::HUMIDITY, "HUM", 75);
          latestMetricRow(data, MetricId::PRECIPITATION, "PRECIP", 90);
        } else {
          latestMetricRow(data, MetricId::WIND, "WIND", 39);
          latestMetricRow(data, MetricId::GUST, "GUST", 57);
          latestMetricRow(data, MetricId::PRESSURE, "PRESSURE", 75);
          formatDate(data.latestObservationAt, text, sizeof(text), true); centered(98, text);
        }
      }
    } else if (journalPage < 5) {
      snprintf(text, sizeof(text), "WEATHER %u/10", unsigned(journalPage + 1));
      textAt(4, 25, text);
      uint8_t first = (journalPage - 3) * 4;
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
    if (journalPage >= 5) {
      // Three events/page, labels split into two lines at a word boundary.
      snprintf(text, sizeof(text), "FIELD EVENTS %u/10", journalPage + 1); centered(20, text);
      for (uint8_t row = 0; row < 3; ++row) {
        uint8_t id = (journalPage - 5) * 3 + row;
        const char *name = fieldEventName(FieldEventId(id));
        char label[40]; snprintf(label, sizeof(label), "%s", name);
        char *split = strrchr(label, ' '); if (split) *split++ = 0;
        int y = 33 + row * 25;
        centered(y, label); if (split) centered(y + 8, split);
        const auto &event = data.fieldEvents[id];
        if (event.count) snprintf(text, sizeof(text), "%lu", (unsigned long)event.count);
        else snprintf(text, sizeof(text), "--");
        centered(y + 16, text);
      }
    }
    centered(111, "A:NEXT C:MENU");
  }
  finishScreen();
}
void recordRow(const char *label, const WeatherRecord &record, MetricId metric, int y) {
  char value[32], date[32]; textAt(4, y, label);
  if (record.timestamp == 0) { centered(y + 12, "NOT OBSERVED"); return; }
  formatBuddyMetric(metric, record.value, value, sizeof(value)); centered(y + 9, value);
  formatDate(record.timestamp, date, sizeof(date)); centered(y + 18, date);
}
void drawRecords() {
  beginScreen("RECORDS");
  if (!unavailable()) {
    const BuddySaveData &data = getBuddySave();
    const char *titles[] = {"TEMPERATURE 1/4", "WIND 2/4", "ATMOSPHERE 3/4", "PRECIPITATION 4/4"};
    centered(24, titles[recordsPage]);
    if (data.totalObservations == 0) noObservations();
    else if (recordsPage == 0) {
      char text[32]; centered(34, "HIGHEST");
      formatTemperature(data.highestTemperatureMilliC, text, sizeof(text)); centered(44, text, 2);
      formatDate(data.highestTemperatureAt, text, sizeof(text)); centered(62, text);
      centered(72, "LOWEST"); formatTemperature(data.lowestTemperatureMilliC, text, sizeof(text)); centered(82, text, 2);
      formatDate(data.lowestTemperatureAt, text, sizeof(text)); centered(100, text);
    } else if (recordsPage == 1) {
      recordRow("STRONGEST WIND", data.records[0], MetricId::WIND, 37);
      recordRow("STRONGEST GUST", data.records[1], MetricId::GUST, 74);
    } else if (recordsPage == 2) {
      recordRow("HIGHEST HUMIDITY", data.records[2], MetricId::HUMIDITY, 31);
      recordRow("LOWEST PRESSURE", data.records[3], MetricId::PRESSURE, 57);
      recordRow("HIGHEST PRESSURE", data.records[4], MetricId::PRESSURE, 83);
    } else recordRow("WETTEST OBSERVATION", data.records[5], MetricId::PRECIPITATION, 49);
    centered(111, "A:NEXT C:MENU");
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
  journalPage = 0; recordsPage = 0;
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
    if (currentScreen == RECORDS_SCREEN) { recordsPage = (recordsPage + 1) % 4; redraw = true; }
    if (currentScreen == JOURNAL_SCREEN) { journalPage = (journalPage + 1) % 10; redraw = true; }
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

// Called inside the existing HOME frame scheduler; no separate display update,
// polling, sleep override, or input mode. Timer/setup/menu screens take priority.
bool drawFieldEventNotification() {
  static FieldEventId showing = FieldEventId::COUNT;
  static unsigned long started = 0;
  if (showing != FieldEventId::COUNT && millis() - started >= 2500) showing = FieldEventId::COUNT;
  if (showing == FieldEventId::COUNT) {
    showing = takeNewFieldEvent(); started = millis();
    if (showing == FieldEventId::COUNT) return false;
  }
  display.clearDisplay(); display.setTextColor(COLOR_TEXT);
  centered(20, "NEW FIELD EVENT!");
  char label[40]; snprintf(label, sizeof(label), "%s", fieldEventName(showing));
  char *split = strrchr(label, ' '); if (split) *split++ = 0;
  centered(48, label); if (split) centered(61, split);
  centered(85, "RECORDED");
  return true;
}
