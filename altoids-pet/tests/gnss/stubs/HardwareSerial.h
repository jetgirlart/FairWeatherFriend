#pragma once
#include <string>
#ifndef SERIAL_8N1
#define SERIAL_8N1 0
#endif
inline unsigned gnssStarts=0;
inline std::string gnssInput;
struct HardwareSerial {
 explicit HardwareSerial(int){}
 void setRxBufferSize(unsigned){}
 void begin(unsigned baud,unsigned,int rx,int tx){if(baud!=115200 || rx!=44 || tx!=-1)__builtin_trap();++gnssStarts;}
 int available(){return gnssInput.size();}
 int read(){if(gnssInput.empty())return -1;unsigned char c=gnssInput[0];gnssInput.erase(0,1);return c;}
 void end(){}
};
