#pragma once
#include <string>
struct HardwareSerial {
  int uart,rx=-2,tx=-2;unsigned baud=0,rxSize=0,ends=0;
  std::string input;
  explicit HardwareSerial(int id):uart(id){}
  void setRxBufferSize(unsigned size){rxSize=size;}
  void begin(unsigned speed,unsigned,int receive,int transmit){baud=speed;rx=receive;tx=transmit;}
  int available() const{return int(input.size());}
  int read(){if(input.empty())return -1;unsigned char c=input[0];input.erase(0,1);return c;}
  void end(){++ends;}
};
