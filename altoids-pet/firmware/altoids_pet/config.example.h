#pragma once

#include <Arduino.h>
#include "hardware.h"

// ==================================================
// USER SETTINGS
// ==================================================

const char* const WIFI_SSID = "YOUR_WIFI_NAME";
const char* const WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Example coordinates: replace these with your location in config.h.
// Approximate coordinates are fine.
const float LATITUDE = 32.0000;
const float LONGITUDE = -95.0000;

// US Central Time with automatic DST.
const char* const TIMEZONE =
  "CST6CDT,M3.2.0/2,M11.1.0/2";

// Pet routine.
const int PET_WAKE_HOUR = 7;
const int PET_SLEEP_HOUR = 22;

// Sleep after 30 seconds without a button press.
const unsigned long IDLE_SLEEP_MS = 30000;

// Refresh online weather every 3 hours.
const unsigned long WEATHER_REFRESH_SECONDS =
  3UL * 60UL * 60UL;

// Home-screen animation speed.
const unsigned long ANIMATION_INTERVAL_MS = 250;

// Optional small passive piezo: D3 (GPIO4) -> 220 ohm -> piezo +; piezo - -> GND.
#define PIEZO_PIN D3
// Default for new buddies / saves predating Settings; NVS preference takes precedence.
#define SOUND_ENABLED true
#define SOUND_STARTUP_CHIRP false
