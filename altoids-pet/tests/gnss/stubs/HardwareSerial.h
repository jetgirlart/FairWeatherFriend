#pragma once
#include <string>
#include <driver/gpio.h>
#ifndef SERIAL_8N1
#define SERIAL_8N1 0
#endif
inline unsigned gnssStarts=0;
inline std::string gnssInput,gnssOutput;
inline unsigned gnssEnds=0;
struct HardwareSerial {
 explicit HardwareSerial(int){}
 void setRxBufferSize(unsigned){}
 void begin(unsigned baud,unsigned,int rx,int tx){if(baud!=115200 || rx!=44 || tx!=6)__builtin_trap();++gnssStarts;}
 int available(){return gnssInput.size();}
 int read(){if(gnssInput.empty())return -1;unsigned char c=gnssInput[0];gnssInput.erase(0,1);return c;}
 size_t write(const uint8_t *bytes,size_t n){gnssOutput.append((const char*)bytes,n);return n;}
 size_t write(uint8_t c){gnssOutput+=char(c);return 1;}
 void flush(){}
 void end(){if(!gnssTxHeld)__builtin_trap();++gnssEnds;}
};
