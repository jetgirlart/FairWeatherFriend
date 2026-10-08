#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <string>
#include <deque>
#include <algorithm>
#define ARDUINO 100
#define PROGMEM
#define pgm_read_byte(p) (*(const uint8_t *)(p))
#define pgm_read_word(p) (*(const uint16_t *)(p))
#define pgm_read_dword(p) (*(const uint32_t *)(p))
#define D0 1
#define D1 2
#define D2 3
#define D3 4
#define D4 5
#define D5 6
#define D6 43
#define D7 44
#define D8 7
#define D9 8
#define D10 9
#define HIGH 1
#define LOW 0
#define OUTPUT 1
using String=std::string;
using std::min;
using std::max;
using boolean=bool;
class __FlashStringHelper;
struct Print {
  virtual size_t write(uint8_t)=0;
  virtual ~Print()=default;
  size_t write(const uint8_t *p,size_t n){for(size_t i=0;i<n;++i)write(p[i]);return n;}
  size_t print(const char *p){return write((const uint8_t*)p,strlen(p));}
  void println(const char *p){print(p);write('\n');}
};
struct SerialType:Print {
 std::string output;
 size_t write(uint8_t c) override {output+=char(c);return 1;}
 template<class... T>void printf(const char *fmt,T...args){char s[1024];snprintf(s,sizeof(s),fmt,args...);output+=s;}
};
extern SerialType Serial;
extern unsigned long fakeMillis;
inline unsigned long millis(){return fakeMillis;}
void digitalWrite(int,int);
void pinMode(int,int);
inline void yield() {}
inline float radians(float degrees){return degrees * 3.14159265358979323846f / 180.0f;}
