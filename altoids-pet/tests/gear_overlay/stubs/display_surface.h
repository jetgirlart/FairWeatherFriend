#pragma once
#include <assert.h>
#include <string.h>
#include <vector>
#include "palette.h"
#include "hardware.h"

struct DrawCall {int x,y,h,color;};
extern uint16_t pixels[240*240];
extern std::vector<DrawCall> bitmapCalls;
extern unsigned pushes,clears,umbrellas,scarves;
struct DisplaySurface {
  uint16_t *getBuffer() {return pixels;}
  void clearDisplay() {for(int i=0;i<240*240;++i)pixels[i]=COLOR_BACKGROUND;clears++;}
  void display() {pushes++;}
  void drawBitmap(int x,int y,uint8_t *bits,int w,int h,int color) {
    assert(w==48 && (h==48 || h==45));bitmapCalls.push_back({x,y,h,color});
    for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++)if(bits[yy*6+xx/8]&(0x80>>(xx%8))) {
      int X=x+xx,Y=y+yy;assert(X>=0 && X<240 && Y>=0 && Y<240);pixels[Y*240+X]=color;
    }
  }
  void fillTriangle(int,int,int,int,int,int,int) {umbrellas++;}
  void drawLine(int,int,int,int,int) {}
  void fillRect(int,int,int,int,int) {scarves++;}
  void fillCircle(int,int,int,int) {}
};

extern DisplaySurface &display;
