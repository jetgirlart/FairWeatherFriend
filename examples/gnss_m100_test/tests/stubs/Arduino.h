#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <stdarg.h>
constexpr int D7=44;
constexpr unsigned SERIAL_8N1=0x800001c;
extern unsigned long fakeClock;
inline unsigned long millis(){return fakeClock;}
inline void yield(){}
struct UsbSerial {
  std::string output;
  void begin(unsigned baud){if(baud!=115200)__builtin_trap();}
  explicit operator bool() const{return true;}
  void println(const char *s){output+=s;output+='\n';}
  void printf(const char *format,...){char buffer[256];va_list args;va_start(args,format);vsnprintf(buffer,sizeof(buffer),format,args);va_end(args);output+=buffer;}
  void flush(){}
};
extern UsbSerial Serial;
