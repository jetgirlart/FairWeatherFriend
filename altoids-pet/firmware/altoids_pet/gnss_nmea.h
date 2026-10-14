#ifndef FWF_GNSS_TEST_NMEA_GGA_H
#define FWF_GNSS_TEST_NMEA_GGA_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

namespace gnss_test {
struct Fix {
  double latitude = 0, longitude = 0, hdop = 0, altitude = 0;
  unsigned quality = 0, satellites = 0;
  bool coordinatesValid = false, qualityAvailable = false;
  bool satellitesAvailable = false, hdopAvailable = false, altitudeAvailable = false;
  bool acceptable(unsigned minSatellites = 4, double maxHdop = 3.0) const {
    return coordinatesValid && (latitude != 0 || longitude != 0) &&
           qualityAvailable && (quality == 1 || quality == 2 || quality == 4 || quality == 5) &&
           satellitesAvailable && satellites >= minSatellites && hdopAvailable && hdop > 0 && hdop <= maxHdop;
  }
};
inline int hexDigit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}
inline bool decimal(const char *s, double &result, bool signedValue = false) {
  if (!s || !*s) return false;
  const char *p = s;
  if (signedValue && (*p == '-' || *p == '+')) ++p;
  bool digit = false, dot = false;
  for (; *p; ++p) {
    if (*p == '.' && !dot) dot = true;
    else if (*p >= '0' && *p <= '9') digit = true;
    else return false;
  }
  if (!digit) return false;
  char *end = nullptr; result = strtod(s, &end);
  return *end == 0 && isfinite(result);
}
inline bool integer(const char *s, unsigned &result) {
  if (!s || !*s) return false;
  result = 0;
  for (; *s; ++s) {
    if (*s < '0' || *s > '9') return false;
    result = result * 10 + unsigned(*s - '0');
    if (result > 99) return false;
  }
  return true;
}
inline bool coordinate(const char *s, const char *hemisphere, bool latitude, double &result) {
  const size_t wholeDigits = latitude ? 4 : 5;
  const char *dot = strchr(s, '.');
  if ((dot ? size_t(dot - s) : strlen(s)) != wholeDigits || strlen(hemisphere) != 1) return false;
  double value;
  if (!decimal(s, value)) return false;
  char h = hemisphere[0];
  if (latitude ? (h != 'N' && h != 'S') : (h != 'E' && h != 'W')) return false;
  int degrees = int(value / 100);
  double minutes = value - degrees * 100;
  const int limit = latitude ? 90 : 180;
  if (degrees > limit || minutes < 0 || minutes >= 60 || (degrees == limit && minutes != 0)) return false;
  result = degrees + minutes / 60;
  if (h == 'S' || h == 'W') result = -result;
  return true;
}
// Fixed-size sentence collector. Unknown sentences, bad checksums and truncated
// lines never become fixes. GP/GN/GL/GA/GB/BD talkers use the same GGA fields.
class Parser {
public:
  bool feed(char c, Fix &fix) {
    if (c == '$') { used = 0; collecting = true; }
    if (c == '\r' || c == '\n') {
      if (!collecting) return false;
      collecting = false; line[used] = 0;
      return parse(fix);
    }
    if (!collecting) return false;
    if (c < 32 || c > 126 || used >= sizeof(line) - 1) { collecting = false; return false; }
    line[used++] = c; return false;
  }
private:
  char line[160] = {};
  size_t used = 0;
  bool collecting = false;
  bool parse(Fix &fix) {
    char *star = strchr(line, '*');
    if (!star || strlen(star) != 3) return false;
    int high = hexDigit(star[1]), low = hexDigit(star[2]);
    if (high < 0 || low < 0) return false;
    uint8_t checksum = 0;
    for (char *p = line + 1; p < star; ++p) checksum ^= uint8_t(*p);
    if (checksum != (high * 16 + low)) return false;
    *star = 0;
    char *fields[15] = {line}; size_t count = 1;
    for (char *p = line; *p; ++p) if (*p == ',') {
      *p = 0;
      if (count >= 15) return false;
      fields[count++] = p + 1;
    }
    if (count < 11 || strlen(fields[0]) != 6 || strcmp(fields[0] + 3, "GGA") != 0 ||
        fields[0][1] < 'A' || fields[0][1] > 'Z' || fields[0][2] < 'A' || fields[0][2] > 'Z') return false;
    Fix next;
    next.coordinatesValid = coordinate(fields[2], fields[3], true, next.latitude) &&
                            coordinate(fields[4], fields[5], false, next.longitude);
    next.qualityAvailable = integer(fields[6], next.quality);
    next.satellitesAvailable = integer(fields[7], next.satellites);
    next.hdopAvailable = decimal(fields[8], next.hdop);
    next.altitudeAvailable = strcmp(fields[10], "M") == 0 && decimal(fields[9], next.altitude, true);
    fix = next; return true;
  }
};
} // namespace gnss_test
#endif
