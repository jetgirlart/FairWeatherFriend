#pragma once
#include "../../journal/stubs/Arduino.h"
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
struct String : std::string {
 using std::string::string;
 String()=default;
 String(const std::string &s):std::string(s){}
 String(double value,int digits){char s[48];snprintf(s,sizeof(s),"%.*f",digits,value);assign(s);}
 size_t position=0;
 int read(){return position<size()?at(position++):-1;}
 size_t readBytes(char *p,size_t n){size_t i=0;for(;i<n && position<size();++i)p[i]=at(position++);return i;}
};
struct Stream {
 virtual ~Stream()=default;
 virtual int available()=0;
 virtual int read()=0;
 virtual int peek()=0;
 virtual void flush()=0;
 virtual size_t write(uint8_t)=0;
 void setTimeout(unsigned long){}
 size_t readBytes(char *p,size_t n){size_t i=0;for(;i<n;++i){int c=read();if(c<0)break;p[i]=c;}return i;}
};
inline void delay(unsigned long ms){fakeMillis+=ms;}
inline void configTime(long,int,const char*,const char*,const char*){}
inline bool getLocalTime(struct tm *t,unsigned long){time_t now=time(nullptr);return localtime_r(&now,t);}
