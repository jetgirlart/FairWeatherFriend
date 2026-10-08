#include "display.h"
#include "journal_ui.h"
#include "buddy_setup.h"
#include "pet.h"
#include "sprites.h"
#include "weather.h"
#include "timer.h"
// ==================================================
// SCREEN STATE
// ==================================================

ScreenMode currentScreen = HOME;

// ==================================================
// MENU
// ==================================================

const char* menuItems[] = {
  "WEATHER",
  "TIMER",
  "JOURNAL",
  "RECORDS",
  "GEAR",
  "SETTINGS"
};

const int menuCount = sizeof(menuItems) / sizeof(menuItems[0]);
int menuIndex = 0;

// ==================================================
// SUN
// ==================================================

void drawSun(int x, int y) {

  display.drawCircle(
    x,
    y,
    5,
    COLOR_WARM
  );

  display.drawLine(
    x, y - 9,
    x, y - 7,
    COLOR_WARM
  );

  display.drawLine(
    x, y + 7,
    x, y + 9,
    COLOR_WARM
  );

  display.drawLine(
    x - 9, y,
    x - 7, y,
    COLOR_WARM
  );

  display.drawLine(
    x + 7, y,
    x + 9, y,
    COLOR_WARM
  );
}

// ==================================================
// MOON
// ==================================================

void drawMoonPhase(
  int x,
  int y,
  MoonPhase phase
) {

  const int radius = 7;

  display.fillCircle(
    x,
    y,
    radius,
    COLOR_WARM
  );

  switch (phase) {

    case MOON_NEW:

      display.fillCircle(
        x,
        y,
        radius - 1,
        COLOR_BACKGROUND
      );

      break;

    case MOON_WAXING_CRESCENT:

      display.fillCircle(
        x - 3,
        y,
        radius,
        COLOR_BACKGROUND
      );

      break;

    case MOON_FIRST_QUARTER:

      display.fillRect(
        x - radius,
        y - radius,
        radius,
        radius * 2 + 1,
        COLOR_BACKGROUND
      );

      break;

    case MOON_WAXING_GIBBOUS:

      display.fillCircle(
        x - 7,
        y,
        radius,
        COLOR_BACKGROUND
      );

      break;

    case MOON_FULL:

      break;

    case MOON_WANING_GIBBOUS:

      display.fillCircle(
        x + 7,
        y,
        radius,
        COLOR_BACKGROUND
      );

      break;

    case MOON_LAST_QUARTER:

      display.fillRect(
        x,
        y - radius,
        radius + 1,
        radius * 2 + 1,
        COLOR_BACKGROUND
      );

      break;

    case MOON_WANING_CRESCENT:

      display.fillCircle(
        x + 3,
        y,
        radius,
        COLOR_BACKGROUND
      );

      break;
  }
}

// ==================================================
// CLOUD
// ==================================================

void drawCloud(int x, int y) {

  display.fillCircle(
    x,
    y,
    5,
    COLOR_MUTED
  );

  display.fillCircle(
    x + 7,
    y - 3,
    6,
    COLOR_MUTED
  );

  display.fillCircle(
    x + 14,
    y,
    5,
    COLOR_MUTED
  );

  display.fillRect(
    x,
    y,
    15,
    5,
    COLOR_MUTED
  );
}

// ==================================================
// STATIC WEATHER ICON
// ==================================================

void drawWeatherIcon(
  int x,
  int y
) {

  bool daytime =
    isDaylight();

  switch (weatherState) {

    case WEATHER_CLEAR:

      if (daytime) {

        drawSun(x, y);

      } else {

        drawMoonPhase(
          x,
          y + 2,
          currentMoonPhase
        );
      }

      break;

    case WEATHER_MAINLY_CLEAR:

      if (daytime) {

        drawSun(
          x - 4,
          y - 3
        );

      } else {

        drawMoonPhase(
          x - 3,
          y - 2,
          currentMoonPhase
        );
      }

      drawCloud(
        x - 4,
        y + 5
      );

      break;

    case WEATHER_PARTLY_CLOUDY:

      if (daytime) {

        drawSun(
          x - 4,
          y - 4
        );

      } else {

        drawMoonPhase(
          x - 3,
          y - 2,
          currentMoonPhase
        );
      }

      drawCloud(
        x - 5,
        y + 5
      );

      break;

    case WEATHER_CLOUDY:

      drawCloud(
        x - 8,
        y
      );

      break;

    case WEATHER_RAIN:

      drawCloud(
        x - 8,
        y - 2
      );

      display.drawLine(
        x - 5, y + 7,
        x - 7, y + 11,
        COLOR_TEXT
      );

      display.drawLine(
        x + 1, y + 7,
        x - 1, y + 11,
        COLOR_TEXT
      );

      display.drawLine(
        x + 7, y + 7,
        x + 5, y + 11,
        COLOR_TEXT
      );

      break;

    case WEATHER_SNOW:

      drawCloud(
        x - 8,
        y - 2
      );

      display.drawPixel(
        x - 5,
        y + 9,
        COLOR_TEXT
      );

      display.drawPixel(
        x + 1,
        y + 11,
        COLOR_TEXT
      );

      display.drawPixel(
        x + 7,
        y + 9,
        COLOR_TEXT
      );

      break;

    case WEATHER_STORM:

      drawCloud(
        x - 8,
        y - 3
      );

      display.drawLine(
        x,
        y + 5,
        x - 4,
        y + 12,
        COLOR_TEXT
      );

      display.drawLine(
        x - 4,
        y + 12,
        x + 1,
        y + 12,
        COLOR_TEXT
      );

      display.drawLine(
        x + 1,
        y + 12,
        x - 3,
        y + 18,
        COLOR_TEXT
      );

      break;

    case WEATHER_FOG:

      display.drawLine(
        x - 10, y - 5,
        x + 10, y - 5,
        COLOR_TEXT
      );

      display.drawLine(
        x - 7, y,
        x + 13, y,
        COLOR_TEXT
      );

      display.drawLine(
        x - 10, y + 5,
        x + 10, y + 5,
        COLOR_TEXT
      );

      break;

    default:

      display.drawCircle(
        x,
        y,
        6,
        COLOR_TEXT
      );

      display.setTextSize(1);

      display.setCursor(
        x - 2,
        y - 3
      );

      display.print("?");

      break;
  }
}

// ==================================================
// WEATHER BACKGROUND EFFECTS
// ==================================================

void drawWeatherBackground() {
  // The same frame counters/cadence now surround the pet across the TFT field.
  if (weatherState == WEATHER_CLEAR && !isDaylight()) {
    const int stars[][2] = {{24,62},{204,78},{38,142},{54,92},{191,142},{219,156}};
    for (int i = 0; i < 6; ++i) {
      bool visible = i < 3 || (i < 5 ? animationFrame % 4 < 2 : animationFrame % 8 < 4);
      if (visible) display.fillRect(stars[i][0], stars[i][1], 2, 2, COLOR_WARM);
    }
  } else if (weatherState == WEATHER_RAIN || weatherState == WEATHER_STORM) {
    bool storm = weatherState == WEATHER_STORM;
    int count = storm ? 6 : 7, shift = animationFrame * (storm ? 5 : 4);
    for (int i = 0; i < count; ++i) {
      int x = 18 + i * 33, y = 62 + (i * 13 + shift) % 96;
      display.drawLine(x, y, x - 3, y + 8, COLOR_COOL);
    }
    if (storm && (animationFrame % 24 == 0 || animationFrame % 24 == 1)) {
      display.drawLine(199, 69, 185, 95, COLOR_WARM);
      display.drawLine(185, 95, 198, 95, COLOR_WARM);
      display.drawLine(198, 95, 180, 123, COLOR_WARM);
    }
  } else if (weatherState == WEATHER_SNOW) {
    for (int i = 0; i < 8; ++i) {
      int x = 14 + (i * 31 + animationFrame) % 212;
      int y = 62 + (i * 17 + animationFrame * 2) % 100;
      display.fillRect(x, y, 2, 2, COLOR_COOL);
      if (i % 3 == 0) { display.drawLine(x - 2, y, x + 3, y, COLOR_COOL); display.drawLine(x, y - 2, x, y + 3, COLOR_COOL); }
    }
  } else if (weatherState == WEATHER_FOG) {
    int shift = animationFrame % 12;
    display.drawFastHLine(12 + shift, 76, 69, COLOR_MUTED);
    display.drawFastHLine(142 - shift, 102, 80, COLOR_MUTED);
    display.drawFastHLine(16 + shift, 139, 75, COLOR_MUTED);
    display.drawFastHLine(148 - shift, 160, 76, COLOR_MUTED);
  }
  // Calm cloud banks are scenery; weather fetching/state is untouched.
  if (weatherState == WEATHER_CLOUDY || weatherState == WEATHER_PARTLY_CLOUDY ||
      weatherState == WEATHER_MAINLY_CLEAR || weatherState == WEATHER_RAIN || weatherState == WEATHER_STORM) {
    display.beginPet(42, 66); drawCloud(0, 0); display.endPet();
    display.beginPet(190, 72); drawCloud(0, 0); display.endPet();
  }
}

namespace {
void centeredText(int y, const char *text, uint8_t size = 2) {
  if (strlen(text) * 6 * size > TFT_WIDTH - 16) size = 1;
  display.setTextSize(size);
  display.setCursor((TFT_WIDTH - strlen(text) * 6 * size) / 2, y);
  display.print(text);
}
void petAt(int x, int y, bool sleeping, bool closed) {
  setSpriteOrigin(x, y);
  display.beginPet(x, y); drawPet(0, 0, sleeping, closed); display.endPet();
}
void iconAt(int x, int y) {
  display.beginPet(x, y); drawWeatherIcon(0, 0); display.endPet();
}
}
void drawTime() {
  display.setTextColor(COLOR_TEXT);
  display.setTextSize(3);
  display.setCursor(12, 12);
  if (!timeValid) { display.print("NO TIME"); return; }
  int hour = currentHour % 12; if (!hour) hour = 12;
  char text[10]; snprintf(text, sizeof(text), "%d:%02d", hour, currentMinute);
  display.print(text);
  display.setTextSize(2); display.setCursor(14, 41); display.print(currentHour >= 12 ? "PM" : "AM");
}
void drawHome() {
  display.clearDisplay();
  drawWeatherBackground();
  drawTime();
  iconAt(201, 28);
  bool sleeping = isPetSleeping();
  int y = petReacting && !sleeping ? 47 : 56;
  petAt(72, y, sleeping, blinking);
  if (petReacting && !sleeping) {
    display.beginPet(168, 69); drawHeart(0, 0); display.endPet();
  }
  display.drawFastHLine(26, 182, 180, COLOR_MUTED);
  if (weatherValid) {
    char text[16]; snprintf(text, sizeof(text), "%d F", temperatureF);
    display.setTextColor(COLOR_TEXT); centeredText(188, text, 2);
  }
  display.setTextColor(COLOR_COOL);
  centeredText(216, weatherValid ? weatherName() : "HELLO", 2);
  display.display();
}
void drawWeatherScreen() {
  display.clearDisplay(); display.setTextColor(COLOR_TEXT);
  centeredText(12, "WEATHER", 3);
  iconAt(120, 67);
  if (weatherValid) {
    char text[16]; snprintf(text, sizeof(text), "%d F", temperatureF);
    centeredText(98, text, 3); centeredText(135, weatherName(), 2);
  }
  if (sunTimesValid) {
    char text[24]; display.setTextColor(COLOR_WARM);
    snprintf(text, sizeof(text), "RISE %02d:%02d", sunriseHour, sunriseMinute); centeredText(167, text, 2);
    snprintf(text, sizeof(text), "SET  %02d:%02d", sunsetHour, sunsetMinute); centeredText(192, text, 2);
  }
  if (!isDaylight()) { display.setTextColor(COLOR_TEXT); centeredText(219, moonPhaseName(), 2); }
  display.display();
}
void drawMenu() {
  display.clearDisplay(); display.setTextColor(COLOR_TEXT);
  centeredText(12, "MENU", 3);
  display.setTextSize(2);
  for (int i = 0; i < menuCount; ++i) {
    int y = 58 + i * 28;
    if (i == menuIndex) {
      display.drawRect(12, y - 5, 216, 26, COLOR_COOL);
      display.setCursor(22, y); display.print(">");
    }
    display.setCursor(48, y); display.print(menuItems[i]);
  }
  display.display();
}

void handleButtons(bool aPressed, bool bPressed, bool cPressed) {
  if (handleBuddySetupButtons(aPressed, bPressed, cPressed)) return;
  if (handleTimerButtons(aPressed, bPressed, cPressed)) return;
  if (handleJournalButtons(aPressed, bPressed, cPressed)) return;
  // ----------------------------------------------
  // A BUTTON
  // ----------------------------------------------

  if (
    aPressed
  ) {

    if (
      currentScreen ==
      HOME
    ) {

      currentScreen =
        MENU;

      menuIndex =
        0;

      blinking =
        false;

      drawMenu();

    } else if (
      currentScreen ==
      MENU
    ) {

      menuIndex++;

      if (
        menuIndex >=
        menuCount
      ) {

        menuIndex =
          0;
      }

      drawMenu();
    }
  }

  // ----------------------------------------------
  // B BUTTON
  // ----------------------------------------------

  if (
    bPressed
  ) {

    if (
      currentScreen ==
      HOME
    ) {

      if (interactWithPet()) {
        drawHome();
      }

    } else if (
      currentScreen ==
      MENU
    ) {

      if (
        menuIndex == 0
      ) {

        currentScreen =
          WEATHER_SCREEN;

        drawWeatherScreen();

      } else if (menuIndex == 1) {
        openTimerSetup();
      } else if (menuIndex == 2) {
        openJournalScreen(JOURNAL_SCREEN);
      } else if (menuIndex == 3) {
        openJournalScreen(RECORDS_SCREEN);
      } else if (menuIndex == 4) {
        openJournalScreen(GEAR_SCREEN);
      } else {

        Serial.print(
          "Selected: "
        );

        Serial.println(
          menuItems[
            menuIndex
          ]
        );
      }
    }
  }

  // ----------------------------------------------
  // C BUTTON
  // ----------------------------------------------

  if (
    cPressed
  ) {

    if (
      currentScreen ==
      WEATHER_SCREEN || isJournalScreen()
    ) {

      currentScreen =
        MENU;

      drawMenu();

    } else {

      currentScreen =
        HOME;

      petReacting =
        false;

      blinking =
        false;

      nextBlinkTime =
        millis() +
        3000;

      drawHome();
    }
  }
}

void drawTimerSetup(uint32_t minutes) {
  display.clearDisplay(); display.setTextColor(COLOR_TEXT);
  centeredText(12, "TIMER", 3);
  char text[16]; snprintf(text, sizeof(text), "%lu MIN", static_cast<unsigned long>(minutes));
  centeredText(82, text, 4);
  display.setTextSize(2);
  display.setCursor(32, 160); display.print("A: NEXT");
  display.setCursor(32, 184); display.print("B: START");
  display.setCursor(32, 208); display.print("C: MENU");
  display.display();
}
void drawFocusTimer(uint32_t seconds) {
  display.clearDisplay(); display.setTextColor(COLOR_TEXT);
  centeredText(12, "FOCUS", 3);
  char text[12]; snprintf(text, sizeof(text), "%02lu:%02lu", static_cast<unsigned long>(seconds / 60), static_cast<unsigned long>(seconds % 60));
  centeredText(55, text, 5);
  bool sleeping = isPetSleeping();
  petAt(72, 88, sleeping, false);
  if (!sleeping) {
    display.fillRect(94, 185, 58, 22, COLOR_BACKGROUND);
    display.drawRect(94, 185, 58, 22, COLOR_WARM);
    display.drawLine(123, 185, 123, 205, COLOR_WARM);
    display.drawLine(100, 192, 116, 192, COLOR_WARM);
    display.drawLine(130, 192, 146, 192, COLOR_WARM);
  }
  display.display();
}
void drawTimerDone(uint32_t frame) {
  display.clearDisplay(); display.setTextColor(COLOR_TEXT);
  centeredText(12, "DONE", 4);
  bool sleeping = isPetSleeping();
  bool celebrate = !sleeping && frame < 6;
  petAt(72, 62 - (celebrate && frame % 2 == 0 ? 3 : 0), sleeping, false);
  if (celebrate) { display.beginPet(168, 78); drawHeart(0, 0); display.endPet(); }
  centeredText(217, "C: HOME", 2);
  display.display();
}
