#pragma once
#define WIFI_STA 1
#define WIFI_OFF 0
#define WL_CONNECTED 3
struct WiFiMock {
 int currentMode=WIFI_OFF;
 void mode(int mode){currentMode=mode;}
 void begin(const char*,const char*){}
 int status(){return WL_CONNECTED;}
 void disconnect(bool){}
 int RSSI(){return -45;}
};
inline WiFiMock WiFi;
