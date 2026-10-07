#include "display.h"
#include "pet.h"
#include "weather.h"
#include <Wire.h>

Adafruit_SH1107 display = Adafruit_SH1107(128, 128, &Wire);

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
  "PET",
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

  // Push one complete frame.

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
