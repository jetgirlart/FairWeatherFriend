#pragma once
#include "field_locations.h"
// Minimal temporary 7x7 pixel icons; not final artwork. No opaque background.
template<class Surface> void drawLocationSourceIcon(Surface &target, int x, int y, LocationSource source, uint16_t color) {
  static const uint8_t dish[] = {0x04,0x12,0x39,0x1A,0x0C,0x08,0x3E};
  static const uint8_t pin[] = {0x1C,0x22,0x2A,0x22,0x14,0x08,0x08};
  static const uint8_t unknown[] = {0x1C,0x22,0x02,0x0C,0x08,0x00,0x08};
  const uint8_t *rows = source == LOCATION_SOURCE_GNSS ? dish : source == LOCATION_SOURCE_FIELD_SITE ? pin : unknown;
  for (int row=0;row<7;++row) for(int col=0;col<7;++col)
    if(rows[row] & (1 << (6-col))) target.drawPixel(x+col,y+row,color);
}
