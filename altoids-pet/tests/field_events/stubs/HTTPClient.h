#pragma once
#include "Arduino.h"
#include <map>
#include <vector>
#include <assert.h>
constexpr int HTTPC_STRICT_FOLLOW_REDIRECTS=1;
struct MockReply {int code;std::string body;int length=-2;};
inline std::deque<MockReply> httpReplies;
inline std::vector<std::string> httpUrls;
inline std::vector<std::map<std::string,std::string>> httpHeaders;
struct MockStream:Stream {
 std::string body;size_t position=0;
 int available()override{return body.size()-position;}
 int read()override{return position<body.size()?body[position++]:-1;}
 int peek()override{return position<body.size()?body[position]:-1;}
 void flush()override{}
 size_t write(uint8_t)override{return 0;}
};
struct HTTPClient {
 MockReply reply;MockStream stream;
 void useHTTP10(bool){}
 void setConnectTimeout(int){}
 void setTimeout(int){}
 void setFollowRedirects(int){}
 bool begin(WiFiClientSecure&,const String &url){httpUrls.push_back(url);httpHeaders.emplace_back();return true;}
 void addHeader(const char *key,const String &value){httpHeaders.back()[key]=value;}
 int GET(){assert(!httpReplies.empty());reply=httpReplies.front();httpReplies.pop_front();stream.body=reply.body;return reply.code;}
 int getSize(){return reply.length==-2?reply.body.size():reply.length;}
 Stream &getStream(){return stream;}
 String getString(){return reply.body;}
 void end(){}
};
