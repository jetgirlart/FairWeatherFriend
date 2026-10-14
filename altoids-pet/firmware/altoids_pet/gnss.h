#pragma once
#include "field_locations.h"
constexpr uint32_t GNSS_BAUD = 115200;
constexpr int GNSS_RX_GPIO = 44; // XIAO D7 receives M100 TX.
constexpr int GNSS_TX_GPIO = 6; // XIAO D5 transmits to M100 RX. D4 stays TFT reset.
constexpr unsigned long GNSS_WAKE_SETTLE_MS = 1500;
constexpr unsigned long GNSS_STANDBY_GRACE_MS = 1000;
constexpr unsigned long GNSS_STANDBY_QUIET_MS = 3000;
constexpr unsigned long GNSS_STANDBY_CHECK_MS = GNSS_STANDBY_GRACE_MS + GNSS_STANDBY_QUIET_MS;
constexpr unsigned long GNSS_TIMEOUT_MS = 120000;
constexpr unsigned long GNSS_RESULT_DISPLAY_MS = 600;
constexpr unsigned GNSS_MIN_SATELLITES = 4;
constexpr double GNSS_MAX_HDOP = 3.0;
void beginObservationLocation();
bool updateObservationLocation(); // Non-blocking step; true once acquisition is complete.
ObservationLocation liveObservationLocation();
void drawLocationAcquisitionStatus(const char *text, unsigned remaining);

void prepareGnssDeepSleep(); // Preserve UART TX idle HIGH; no acquisition on cached wakes.
