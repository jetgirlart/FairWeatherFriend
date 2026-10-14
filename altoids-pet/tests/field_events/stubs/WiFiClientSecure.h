#pragma once
inline int mockTlsError = 0;
inline unsigned mockTlsConnections = 0, mockPeakTlsConnections = 0;
struct WiFiClientSecure {
 bool connected=false;
 ~WiFiClientSecure(){stop();}
 void connectForTest(){if(!connected){connected=true;++mockTlsConnections;if(mockTlsConnections>mockPeakTlsConnections)mockPeakTlsConnections=mockTlsConnections;}}
 void stop(){if(connected){connected=false;--mockTlsConnections;}}
 void setInsecure(){}
 int lastError(char *buffer,size_t size){snprintf(buffer,size,"mock TLS failure");return mockTlsError;}
};
