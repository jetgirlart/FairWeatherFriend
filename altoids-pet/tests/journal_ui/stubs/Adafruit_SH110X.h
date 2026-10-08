#pragma once
#include <assert.h>
#include <string>
#include <vector>
#define SH110X_WHITE 1
struct TextRow { int x,y,size;std::string value; };
extern std::vector<TextRow> frameText;
extern unsigned frameClears,framePushes,discoveryDots;
struct Adafruit_SH1107 {
  int x=0,y=0,size=1;
  void clearDisplay() { frameText.clear();discoveryDots=0;frameClears++; }
  void setTextColor(int) {}
  void setTextSize(int value) { size=value; }
  void setCursor(int X,int Y) { x=X;y=Y; }
  void print(const char *text) {
    assert(x>=0 && y>=0 && x+int(strlen(text))*6*size<=128 && y+8*size<=128);
    frameText.push_back({x,y,size,text});
  }
  void display() { framePushes++;assert(framePushes==frameClears); }
  void fillCircle(int x,int y,int r,int) { assert(x-r>=0 && x+r<128 && y-r>=0 && y+r<128);discoveryDots++; }
  void drawCircle(int x,int y,int r,int) { assert(x-r>=0 && x+r<128 && y-r>=0 && y+r<128); }
};
