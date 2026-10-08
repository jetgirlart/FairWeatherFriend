#include "display.h"
#include "pet.h"
#include "weather.h"
#include "timer.h"
#include <Wire.h>

namespace {
// The stock SH110X display() sends a dirty rectangle through the remaining
// pages. Rebuilding a frame makes that rectangle full-screen. Compare final
// bytes instead, and send only changed spans using the same SH1107 page protocol.
class PartialSH1107 : public Adafruit_SH1107 {
public:
  PartialSH1107() : Adafruit_SH1107(128, 128, &Wire) {}

  void display() override {
    if (!buffer || !i2c_dev) return;
    size_t capacity = i2c_dev->maxBufferSize();
    if (capacity < 2) return;
    const uint8_t dataPrefix = 0x40;
    i2c_dev->setSpeed(i2c_preclk);

    for (uint8_t page = 0; page < 16; ++page) {
      uint8_t *pixels = buffer + page * 128;
      uint8_t *previous = sentFrame + page * 128;
      int first = 0;
      int last = 127;
      if (sentFrameValid) {
        while (first < 128 && pixels[first] == previous[first]) ++first;
        if (first == 128) continue;
        while (last > first && pixels[last] == previous[last]) --last;
      }

      uint8_t column = first + _page_start_offset;
      const uint8_t commands[] = {
        0x00, static_cast<uint8_t>(SH110X_SETPAGEADDR + page),
        static_cast<uint8_t>(0x10 | (column >> 4)),
        static_cast<uint8_t>(column & 0x0F)
      };
      bool success = i2c_dev->write(commands, sizeof(commands));
      size_t position = first;
      while (success && position <= static_cast<size_t>(last)) {
        size_t count = last - position + 1;
        if (count > capacity - 1) count = capacity - 1;
        success = i2c_dev->write(pixels + position, count, true, &dataPrefix, 1);
        position += count;
      }
      if (!success) {
        // A partial transfer may have changed OLED RAM. Resynchronize the
        // complete image next time without ever sending an intermediate blank.
        sentFrameValid = false;
        i2c_dev->setSpeed(i2c_postclk);
        return;
      }
    }

    memcpy(sentFrame, buffer, sizeof(sentFrame));
    sentFrameValid = true;
    window_x1 = window_y1 = 1024;
    window_x2 = window_y2 = -1;
    i2c_dev->setSpeed(i2c_postclk);
  }

private:
  uint8_t sentFrame[128 * 16] = {};
  bool sentFrameValid = false;
};

PartialSH1107 oled;
} // namespace

Adafruit_SH1107 &display = oled;

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
  "SETTINGS"
};

const int menuCount = 4;
int menuIndex = 0;

// ==================================================
// SUN
// ==================================================

void drawSun(int x, int y) {

  display.drawCircle(
    x,
    y,
    5,
    SH110X_WHITE
  );

  display.drawLine(
    x, y - 9,
    x, y - 7,
    SH110X_WHITE
  );

  display.drawLine(
    x, y + 7,
    x, y + 9,
    SH110X_WHITE
  );

  display.drawLine(
    x - 9, y,
    x - 7, y,
    SH110X_WHITE
  );

  display.drawLine(
    x + 7, y,
    x + 9, y,
    SH110X_WHITE
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
    SH110X_WHITE
  );

  switch (phase) {

    case MOON_NEW:

      display.fillCircle(
        x,
        y,
        radius - 1,
        SH110X_BLACK
      );

      break;

    case MOON_WAXING_CRESCENT:

      display.fillCircle(
        x - 3,
        y,
        radius,
        SH110X_BLACK
      );

      break;

    case MOON_FIRST_QUARTER:

      display.fillRect(
        x - radius,
        y - radius,
        radius,
        radius * 2 + 1,
        SH110X_BLACK
      );

      break;

    case MOON_WAXING_GIBBOUS:

      display.fillCircle(
        x - 7,
        y,
        radius,
        SH110X_BLACK
      );

      break;

    case MOON_FULL:

      break;

    case MOON_WANING_GIBBOUS:

      display.fillCircle(
        x + 7,
        y,
        radius,
        SH110X_BLACK
      );

      break;

    case MOON_LAST_QUARTER:

      display.fillRect(
        x,
        y - radius,
        radius + 1,
        radius * 2 + 1,
        SH110X_BLACK
      );

      break;

    case MOON_WANING_CRESCENT:

      display.fillCircle(
        x + 3,
        y,
        radius,
        SH110X_BLACK
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
    SH110X_WHITE
  );

  display.fillCircle(
    x + 7,
    y - 3,
    6,
    SH110X_WHITE
  );

  display.fillCircle(
    x + 14,
    y,
    5,
    SH110X_WHITE
  );

  display.fillRect(
    x,
    y,
    15,
    5,
    SH110X_WHITE
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
        SH110X_WHITE
      );

      display.drawLine(
        x + 1, y + 7,
        x - 1, y + 11,
        SH110X_WHITE
      );

      display.drawLine(
        x + 7, y + 7,
        x + 5, y + 11,
        SH110X_WHITE
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
        SH110X_WHITE
      );

      display.drawPixel(
        x + 1,
        y + 11,
        SH110X_WHITE
      );

      display.drawPixel(
        x + 7,
        y + 9,
        SH110X_WHITE
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
        SH110X_WHITE
      );

      display.drawLine(
        x - 4,
        y + 12,
        x + 1,
        y + 12,
        SH110X_WHITE
      );

      display.drawLine(
        x + 1,
        y + 12,
        x - 3,
        y + 18,
        SH110X_WHITE
      );

      break;

    case WEATHER_FOG:

      display.drawLine(
        x - 10, y - 5,
        x + 10, y - 5,
        SH110X_WHITE
      );

      display.drawLine(
        x - 7, y,
        x + 13, y,
        SH110X_WHITE
      );

      display.drawLine(
        x - 10, y + 5,
        x + 10, y + 5,
        SH110X_WHITE
      );

      break;

    default:

      display.drawCircle(
        x,
        y,
        6,
        SH110X_WHITE
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

  // Keep the animated weather mostly around the
  // pet rather than covering text.

  switch (weatherState) {

    // ----------------------------------------------
    // CLEAR NIGHT - TWINKLING STARS
    // ----------------------------------------------

    case WEATHER_CLEAR:

      if (!isDaylight()) {

        display.drawPixel(
          15,
          34,
          SH110X_WHITE
        );

        display.drawPixel(
          112,
          44,
          SH110X_WHITE
        );

        display.drawPixel(
          22,
          70,
          SH110X_WHITE
        );

        if (
          animationFrame % 4 < 2
        ) {

          display.drawPixel(
            29,
            43,
            SH110X_WHITE
          );

          display.drawPixel(
            101,
            69,
            SH110X_WHITE
          );
        }

        if (
          animationFrame % 8 < 4
        ) {

          display.drawPixel(
            115,
            82,
            SH110X_WHITE
          );
        }
      }

      break;

    // ----------------------------------------------
    // RAIN
    // ----------------------------------------------

    case WEATHER_RAIN: {

      int shift =
        animationFrame * 4;

      for (
        int i = 0;
        i < 7;
        i++
      ) {

        int x =
          8 + i * 18;

        int y =
          27 +
          (
            i * 13 +
            shift
          ) % 58;

        display.drawLine(
          x,
          y,
          x - 2,
          y + 5,
          SH110X_WHITE
        );
      }

      break;
    }

    // ----------------------------------------------
    // SNOW
    // ----------------------------------------------

    case WEATHER_SNOW: {

      int fall =
        animationFrame * 2;

      for (
        int i = 0;
        i < 8;
        i++
      ) {

        int x =
          7 +
          (
            i * 17 +
            animationFrame
          ) % 116;

        int y =
          27 +
          (
            i * 11 +
            fall
          ) % 60;

        display.drawPixel(
          x,
          y,
          SH110X_WHITE
        );

        if (
          i % 3 == 0
        ) {

          display.drawPixel(
            x + 1,
            y,
            SH110X_WHITE
          );

          display.drawPixel(
            x,
            y + 1,
            SH110X_WHITE
          );
        }
      }

      break;
    }

    // ----------------------------------------------
    // FOG
    // ----------------------------------------------

    case WEATHER_FOG: {

      int shift =
        animationFrame % 12;

      display.drawLine(
        5 + shift,
        35,
        44 + shift,
        35,
        SH110X_WHITE
      );

      display.drawLine(
        70 - shift,
        51,
        119 - shift,
        51,
        SH110X_WHITE
      );

      display.drawLine(
        8 + shift,
        70,
        51 + shift,
        70,
        SH110X_WHITE
      );

      display.drawLine(
        76 - shift,
        84,
        122 - shift,
        84,
        SH110X_WHITE
      );

      break;
    }

    // ----------------------------------------------
    // STORM
    // ----------------------------------------------

    case WEATHER_STORM: {

      int shift =
        animationFrame * 5;

      for (
        int i = 0;
        i < 6;
        i++
      ) {

        int x =
          9 + i * 20;

        int y =
          29 +
          (
            i * 12 +
            shift
          ) % 54;

        display.drawLine(
          x,
          y,
          x - 2,
          y + 5,
          SH110X_WHITE
        );
      }

      // Lightning appears periodically.

      if (
        animationFrame % 24 == 0 ||
        animationFrame % 24 == 1
      ) {

        display.drawLine(
          102,
          32,
          94,
          48,
          SH110X_WHITE
        );

        display.drawLine(
          94,
          48,
          101,
          48,
          SH110X_WHITE
        );

        display.drawLine(
          101,
          48,
          92,
          65,
          SH110X_WHITE
        );
      }

      break;
    }

    default:

      break;
  }
}

// ==================================================
// TIME DISPLAY
// ==================================================

void drawTime() {

  display.setTextSize(1);

  display.setTextColor(
    SH110X_WHITE
  );

  if (!timeValid) {

    display.setCursor(
      5,
      5
    );

    display.print(
      "NO TIME"
    );

    return;
  }

  int hour12 =
    currentHour;

  bool pm =
    hour12 >= 12;

  if (hour12 == 0) {
    hour12 = 12;
  }

  if (hour12 > 12) {
    hour12 -= 12;
  }

  char buffer[10];

  snprintf(
    buffer,
    sizeof(buffer),
    "%d:%02d",
    hour12,
    currentMinute
  );

  display.setCursor(
    5,
    5
  );

  display.print(
    buffer
  );

  display.setCursor(
    5,
    15
  );

  display.print(
    pm ? "PM" : "AM"
  );
}

// ==================================================
// HOME SCREEN
// ==================================================

void drawHome() {

  display.clearDisplay();

  display.setTextColor(
    SH110X_WHITE
  );

  // Clock

  drawTime();

  // Weather icon

  drawWeatherIcon(
    110,
    11
  );

  // Animated background behind pet

  drawWeatherBackground();

  bool sleeping =
    isPetSleeping();

  int petY = 29;

  if (
    petReacting &&
    !sleeping
  ) {

    petY = 23;

    drawHeart(
      88,
      35
    );
  }

  // Pet

  drawPet(
    40,
    petY,
    sleeping,
    blinking
  );

  // Ground

  display.drawLine(
    20,
    95,
    108,
    95,
    SH110X_WHITE
  );

  // Temperature

  if (weatherValid) {

    char tempText[12];

    snprintf(
      tempText,
      sizeof(tempText),
      "%d F",
      temperatureF
    );

    display.setTextSize(1);

    int width =
      strlen(tempText) * 6;

    display.setCursor(
      64 - width / 2,
      100
    );

    display.print(
      tempText
    );
  }

  // Weather name

  const char* label =
    weatherValid
      ? weatherName()
      : "HELLO";

  int largeWidth =
    strlen(label) * 12;

  if (
    largeWidth <= 124
  ) {

    display.setTextSize(2);

    display.setCursor(
      64 - largeWidth / 2,
      112
    );

  } else {

    display.setTextSize(1);

    int smallWidth =
      strlen(label) * 6;

    display.setCursor(
      64 - smallWidth / 2,
      115
    );
  }

  display.print(
    label
  );

  // Transfer only bytes that differ from the last completed OLED frame.

  display.display();
}

// ==================================================
// WEATHER SCREEN
// ==================================================

void drawWeatherScreen() {

  display.clearDisplay();

  display.setTextColor(
    SH110X_WHITE
  );

  display.setTextSize(2);

  display.setCursor(
    22,
    4
  );

  display.print(
    "WEATHER"
  );

  drawWeatherIcon(
    64,
    34
  );

  if (weatherValid) {

    char tempText[12];

    snprintf(
      tempText,
      sizeof(tempText),
      "%d F",
      temperatureF
    );

    display.setTextSize(2);

    int width =
      strlen(tempText) * 12;

    display.setCursor(
      64 - width / 2,
      57
    );

    display.print(
      tempText
    );

    display.setTextSize(1);

    const char* name =
      weatherName();

    width =
      strlen(name) * 6;

    display.setCursor(
      64 - width / 2,
      78
    );

    display.print(
      name
    );
  }

  if (sunTimesValid) {

    char sunText[30];

    snprintf(
      sunText,
      sizeof(sunText),
      "RISE %02d:%02d",
      sunriseHour,
      sunriseMinute
    );

    display.setTextSize(1);

    display.setCursor(
      6,
      91
    );

    display.print(
      sunText
    );

    snprintf(
      sunText,
      sizeof(sunText),
      "SET  %02d:%02d",
      sunsetHour,
      sunsetMinute
    );

    display.setCursor(
      6,
      101
    );

    display.print(
      sunText
    );
  }

  if (
    !isDaylight()
  ) {

    display.setCursor(
      6,
      112
    );

    display.print(
      moonPhaseName()
    );
  }

  display.display();
}

// ==================================================
// MENU
// ==================================================

void drawMenu() {

  display.clearDisplay();

  display.setTextColor(
    SH110X_WHITE
  );

  display.setTextSize(2);

  display.setCursor(
    36,
    8
  );

  display.print(
    "MENU"
  );

  display.setTextSize(1);

  for (
    int i = 0;
    i < menuCount;
    i++
  ) {

    int y =
      40 +
      i * 18;

    if (
      i == menuIndex
    ) {

      display.setCursor(
        16,
        y
      );

      display.print(
        ">"
      );
    }

    display.setCursor(
      30,
      y
    );

    display.print(
      menuItems[i]
    );
  }

  display.display();
}

void initializeDisplayBus() {
  // OLED I2C.

  Wire.begin(
    D4,
    D5
  );

  Wire.setClock(
    400000
  );
}

void initializeDisplay() {
  // Display.

  if (
    !display.begin(
      0x3D,
      true
    )
  ) {

    Serial.println(
      "SH1107 not found."
    );

    while (1) {
    }
  }

  display.clearDisplay();

  display.display();
}

void handleButtons(bool aPressed, bool bPressed, bool cPressed) {
  if (handleTimerButtons(aPressed, bPressed, cPressed)) return;
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
      WEATHER_SCREEN
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
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(2);
  display.setCursor(34, 5);
  display.print("TIMER");
  char text[12];
  snprintf(text, sizeof(text), "%lu MIN", static_cast<unsigned long>(minutes));
  display.setTextSize(3);
  display.setCursor(64 - strlen(text) * 9, 43);
  display.print(text);
  display.setTextSize(1);
  display.setCursor(13, 91); display.print("A: NEXT");
  display.setCursor(13, 103); display.print("B: START");
  display.setCursor(13, 115); display.print("C: MENU");
  display.display();
}

void drawFocusTimer(uint32_t seconds) {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(2);
  display.setCursor(34, 4); display.print("FOCUS");
  char text[10];
  snprintf(text, sizeof(text), "%02lu:%02lu",
           static_cast<unsigned long>(seconds / 60), static_cast<unsigned long>(seconds % 60));
  display.setTextSize(3);
  display.setCursor(19, 27); display.print(text);
  bool sleeping = isPetSleeping();
  drawPet(40, 55, sleeping, false);
  if (!sleeping) {
    // A small open book in front of the pet; focus is a quiet static pose.
    display.fillRect(50, 101, 28, 13, SH110X_BLACK);
    display.drawRect(50, 101, 28, 13, SH110X_WHITE);
    display.drawLine(64, 101, 64, 113, SH110X_WHITE);
    display.drawLine(53, 105, 60, 105, SH110X_WHITE);
    display.drawLine(68, 105, 75, 105, SH110X_WHITE);
  }
  display.display();
}

void drawTimerDone(uint32_t frame) {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(3);
  display.setCursor(28, 5); display.print("DONE");
  bool sleeping = isPetSleeping();
  bool celebrate = !sleeping && frame < 6;
  drawPet(40, 40 - (celebrate && frame % 2 == 0 ? 2 : 0), sleeping, false);
  if (celebrate) drawHeart(88, 47);
  display.setTextSize(1);
  display.setCursor(31, 117); display.print("C: HOME");
  display.display();
}
