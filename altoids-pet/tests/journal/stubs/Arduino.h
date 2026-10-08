#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <string>
#include <deque>
struct Print {
  std::string output;
  size_t write(uint8_t c) { output+=char(c); return 1; }
  size_t write(const uint8_t *s,size_t n) { output.append((const char*)s,n); return n; }
  void println() { output+='\n'; }
  void println(const char *s) { output+=s; output+='\n'; }
};
struct SerialType: Print {
  std::deque<char> input;
  unsigned available() { return input.size(); }
  int read() { if(input.empty())return -1; char c=input.front();input.pop_front();return c; }
  template<class... T> void printf(const char *fmt,T...args) { char s[1024]; snprintf(s,sizeof(s),fmt,args...); output+=s; }
};
extern SerialType Serial;
extern unsigned long fakeMillis;
inline unsigned long millis() { return fakeMillis; }
