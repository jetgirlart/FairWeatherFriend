#pragma once
#include "field_locations.h"
constexpr uint32_t GNSS_BAUD = 115200;
constexpr int GNSS_RX_GPIO = 44; // XIAO D7; M100 TX. Module RX is unconnected.
constexpr unsigned long GNSS_TIMEOUT_MS = 20000;
constexpr unsigned long GNSS_RESULT_DISPLAY_MS = 600;
constexpr unsigned GNSS_MIN_SATELLITES = 4;
constexpr double GNSS_MAX_HDOP = 3.0;
void beginObservationLocation();
bool updateObservationLocation(); // Non-blocking step; true once acquisition is complete.
ObservationLocation liveObservationLocation();
void drawLocationAcquisitionStatus(const char *text, unsigned remaining);
