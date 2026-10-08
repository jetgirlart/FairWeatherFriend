#pragma once
#include <assert.h>
#include <string.h>
#include <vector>
#define SH110X_WHITE 1
#define SH110X_BLACK 0
struct DrawCall {int x,y,h,color;};
extern uint8_t pixels[128*128];
extern std::vector<DrawCall> bitmapCalls;
extern unsigned pushes,clears,umbrellas,scarves;
struct Adafruit_SH1107 {
  void clearDisplay() {memset(pixels,0,128*128);clears++;}
  void display() {pushes++;}
  void drawBitmap(int x,int y,uint8_t *bits,int w,int h,int color) {
    assert(w==48 && (h==48 || h==45));bitmapCalls.push_back({x,y,h,color});
    for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++)if(bits[yy*6+xx/8]&(0x80>>(xx%8))) {
      int X=x+xx,Y=y+yy;assert(X>=0 && X<128 && Y>=0 && Y<128);pixels[Y*128+X]=color;
    }
  }
  void fillTriangle(int,int,int,int,int,int,int) {umbrellas++;}
  void drawLine(int,int,int,int,int) {}
  void fillRect(int,int,int,int,int) {scarves++;}
  void fillCircle(int,int,int,int) {}
};
