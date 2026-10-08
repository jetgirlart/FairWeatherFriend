#pragma once
#include <assert.h>
#include <string>
#include <vector>
#include "palette.h"
constexpr int TFT_WIDTH = 240;
constexpr int TFT_HEIGHT = 240;
struct TextRow { int x,y,size;std::string value; };
extern std::vector<TextRow> frameText;
extern unsigned frameClears,framePushes,discoveryDots;
struct DisplaySurface {
  int x=0,y=0,size=1;
  void clearDisplay() { frameText.clear();discoveryDots=0;frameClears++; }
  void setTextColor(int) {}
  void setTextSize(int value) { size=value; }
  void setCursor(int X,int Y) { x=X;y=Y; }
  void print(const char *text) {
    if(!(x>=0 && y>=0 && x+int(strlen(text))*6*size<=240 && y+8*size<=240)) fprintf(stderr,"Bounds: %s x%d y%d size%d\n",text,x,y,size);
    assert(x>=0 && y>=0 && x+int(strlen(text))*6*size<=240 && y+8*size<=240);
    frameText.push_back({x,y,size,text});
  }
  void display() { framePushes++;assert(framePushes==frameClears); }
  void drawRect(int x,int y,int w,int h,int) {assert(x>=0 && y>=0 && x+w<=240 && y+h<=240);}
  void fillCircle(int x,int y,int r,int) { assert(x-r>=0 && x+r<240 && y-r>=0 && y+r<240);discoveryDots++; }
  void drawCircle(int x,int y,int r,int) { assert(x-r>=0 && x+r<240 && y-r>=0 && y+r<240); }
};

extern DisplaySurface &display;
