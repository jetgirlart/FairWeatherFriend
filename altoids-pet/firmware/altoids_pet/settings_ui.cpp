#include "settings_ui.h"
#include "display.h"
#include "journal.h"
#include "weather.h"
#include "sound.h"
#include "version.h"
#include <stdio.h>
#include <string.h>

namespace settings_detail {
enum class Page : uint8_t { LIST, SOUND, UNITS, LOCATION, EXPORT, IMPORT, ABOUT };
Page settingsPage = Page::LIST;
uint8_t selection = 0;
bool draft = false, settingsSaveFailed = false;
uint32_t settingsChecksum = 0;
BuddyTransferStatus displayedTransfer = BuddyTransferStatus::NONE;
const char *items[] = {"SOUND", "UNITS", "LOCATION", "EXPORT BUDDY", "IMPORT BUDDY", "ABOUT"};

void text(int y, const char *value, uint8_t size = 2) {
  while (size > 1 && strlen(value) * 6 * size > TFT_WIDTH - 12) --size;
  int width = strlen(value) * 6 * size;
  display.setTextSize(size); display.setCursor((TFT_WIDTH - width) / 2, y); display.print(value);
}
void draw() {
  display.clearDisplay(); display.setTextColor(COLOR_TEXT);
  const auto &buddy = getBuddySave();
  BuddyTransferStatus transfer = buddyTransferStatus();
  if (settingsPage == Page::LIST) {
    text(12, "SETTINGS", 3);
    for (uint8_t i = 0; i < 6; ++i) {
      int y = 53 + i * 26;
      if (i == selection) display.drawRect(12, y - 5, 216, 25, COLOR_COOL);
      text(y, items[i]);
    }
    text(218, "A:NEXT B:OPEN C:MENU", 1);
  } else if (settingsPage == Page::SOUND || settingsPage == Page::UNITS) {
    text(12, settingsPage == Page::SOUND ? "SOUND" : "UNITS", 3);
    const char *choices[2] = {settingsPage == Page::SOUND ? "ON" : "US", settingsPage == Page::SOUND ? "OFF" : "METRIC"};
    for (uint8_t i = 0; i < 2; ++i) {
      int y = 70 + i * 40;
      if (i == uint8_t(draft)) display.drawRect(30, y - 7, 180, 32, COLOR_COOL);
      text(y, choices[i], 3);
    }
    if (settingsPage == Page::UNITS) text(165, draft ? "C / km/h / mm" : "F / mph / inches");
    if (settingsSaveFailed) text(192, "SAVE FAILED");
    text(220, "A:NEXT B:SAVE C:BACK", 1);
  } else if (settingsPage == Page::LOCATION) {
    text(12, "LOCATION", 3); char row[32];
    snprintf(row, sizeof(row), "LAT %.6f", configuredLatitude()); text(65, row);
    snprintf(row, sizeof(row), "LON %.6f", configuredLongitude()); text(94, row);
    text(137, buddy.locationConfigured ? "SAVED LOCATION" : "CONFIG LOCATION");
    text(171, "UPDATE VIA USB"); text(195, "SET_LOCATION lat lon", 1);
    text(220, "C:BACK", 1);
  } else if (settingsPage == Page::EXPORT || settingsPage == Page::IMPORT) {
    bool importing = settingsPage == Page::IMPORT;
    text(12, importing ? "IMPORT BUDDY" : "EXPORT BUDDY", 2);
    if (importing && transfer == BuddyTransferStatus::IMPORT_READY) {
      text(68, "REPLACE CURRENT"); text(96, "BUDDY?", 3);
      text(142, "Existing data will"); text(166, "be replaced.");
      text(217, "B:CONFIRM C:CANCEL", 1);
    } else if (importing && transfer == BuddyTransferStatus::IMPORT_COMPLETE) {
      text(90, "IMPORT COMPLETE"); text(217, "B:HOME C:HOME", 1);
    } else if (!importing && transfer == BuddyTransferStatus::EXPORT_COMPLETE) {
      text(90, "EXPORT COMPLETE"); text(217, "C:BACK", 1);
    } else {
      bool failed = importing ? transfer == BuddyTransferStatus::IMPORT_FAILED : transfer == BuddyTransferStatus::EXPORT_FAILED;
      text(65, failed ? (importing ? "IMPORT FAILED" : "EXPORT FAILED") : "CONNECT USB");
      text(93, failed ? "BUDDY PRESERVED" : "TO COMPUTER");
      text(137, "WAITING...");
      text(180, importing ? "Send IMPORT_BUDDY" : "Send EXPORT_BUDDY", 1);
      text(217, "C:BACK", 1);
    }
  } else {
    text(12, "ABOUT", 3); text(56, "FAIRWEATHER FRIEND");
    char row[32]; snprintf(row, sizeof(row), "FIRMWARE %s", FW_VERSION); text(88, row);
    snprintf(row, sizeof(row), "SAVE FORMAT %lu", (unsigned long)SAVE_VERSION); text(112, row);
    text(144, "XIAO ESP32-S3"); text(168, "ST7789 240x240");
    text(194, "OPEN-SOURCE PROJECT", 1); text(220, "C:BACK", 1);
  }
  settingsChecksum = buddySaveChecksum(buddy); displayedTransfer = transfer;
  display.display(); // One completed framebuffer; transport sends changed tiles.
}
void back() {
  endBuddyTransfer(); settingsPage = Page::LIST; settingsSaveFailed = false; draw();
}
} // namespace

void openSettings() {
  using namespace settings_detail;
  endBuddyTransfer(); currentScreen = SETTINGS_SCREEN;
  settingsPage = Page::LIST; selection = 0; settingsSaveFailed = false; draw();
}
bool handleSettingsButtons(bool a, bool b, bool c) {
  using namespace settings_detail;
  if (currentScreen != SETTINGS_SCREEN) return false;
  if (c) {
    if (settingsPage == Page::LIST) { endBuddyTransfer(); currentScreen = MENU; drawMenu(); }
    else if (settingsPage == Page::IMPORT && buddyTransferStatus() == BuddyTransferStatus::IMPORT_COMPLETE) {
      endBuddyTransfer(); currentScreen = HOME; drawHome();
    } else back();
    return true;
  }
  if (a) {
    if (settingsPage == Page::LIST) { selection = (selection + 1) % 6; draw(); }
    else if (settingsPage == Page::SOUND || settingsPage == Page::UNITS) { draft = !draft; settingsSaveFailed = false; draw(); }
  }
  if (b) {
    if (settingsPage == Page::LIST) {
      settingsPage = static_cast<Page>(selection + 1); settingsSaveFailed = false;
      draft = settingsPage == Page::SOUND ? !getBuddySave().soundEnabled : getBuddySave().units == UnitsId::METRIC;
      if (settingsPage == Page::EXPORT || settingsPage == Page::IMPORT) beginBuddyTransfer(settingsPage == Page::IMPORT);
      draw();
    } else if (settingsPage == Page::SOUND || settingsPage == Page::UNITS) {
      bool saved = settingsPage == Page::SOUND ? saveBuddySound(!draft) : saveBuddyUnits(draft ? UnitsId::METRIC : UnitsId::US);
      if (saved) { if (!getBuddySave().soundEnabled) stopSound(); back(); }
      else { settingsSaveFailed = true; draw(); }
    } else if (settingsPage == Page::IMPORT) {
      if (buddyTransferStatus() == BuddyTransferStatus::IMPORT_READY) {
        confirmBuddyImport(pendingBuddyImportChecksum()); draw();
      } else if (buddyTransferStatus() == BuddyTransferStatus::IMPORT_COMPLETE) {
        endBuddyTransfer(); currentScreen = HOME; drawHome();
      }
    }
  }
  return true;
}
void updateSettingsScreen() {
  using namespace settings_detail;
  if (currentScreen != SETTINGS_SCREEN) return;
  if (settingsChecksum != buddySaveChecksum(getBuddySave()) || displayedTransfer != buddyTransferStatus()) draw();
}
