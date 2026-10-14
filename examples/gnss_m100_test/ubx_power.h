#pragma once
#include <stdint.h>
#include <stddef.h>
namespace ubx_test {
constexpr uint32_t STANDBY_DURATION_MS = 10000;
// Version 0, 10-second duration, backup+force flags (bits 1/2), UART RX wake (bit 3).
inline size_t standbyPacket(uint8_t (&packet)[24]) {
  const uint8_t data[22]={0xB5,0x62,0x02,0x41,0x10,0x00,
    0,0,0,0,0x10,0x27,0,0,0x06,0,0,0,0x08,0,0,0};
  uint8_t a=0,b=0;
  for(size_t i=0;i<22;++i){packet[i]=data[i];if(i>=2){a+=data[i];b+=a;}}
  packet[22]=a;packet[23]=b;return sizeof(packet);
}
constexpr uint8_t MON_VER_POLL[]={0xB5,0x62,0x0A,0x04,0,0,0x0E,0x34};
// Checksum-valid UBX observer: retain only 40 bytes (MON-VER base/ACK), skip
// extensions without allocation. Never mistake silence for an ACK.
class Parser {
public:
  uint8_t cls=0,id=0,payload[40]={};uint16_t length=0;
  bool feed(uint8_t c) {
    if(state==0){if(c==0xB5)state=1;return false;}
    if(state==1){state=c==0x62?2:c==0xB5?1:0;a=b=0;return false;}
    if(state<6){a+=c;b+=a;}
    switch(state){
      case 2:cls=c;state=3;break;
      case 3:id=c;state=4;break;
      case 4:length=c;state=5;break;
      case 5:length|=uint16_t(c)<<8;pos=0;state=length>1024?0:length?6:7;break;
      case 6:if(pos<sizeof(payload))payload[pos]=c;++pos;a+=c;b+=a;if(pos==length)state=7;break;
      case 7:receivedA=c;state=8;break;
      case 8:state=0;return receivedA==a && c==b;
    }
    return false;
  }
private:
  uint8_t state=0,a=0,b=0,receivedA=0;uint16_t pos=0;
};
}
