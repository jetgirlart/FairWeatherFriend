#pragma once
#include <map>
#include <vector>
#include <string>
#include <stdint.h>
#include <string.h>
using Entries=std::map<std::string,std::vector<uint8_t>>;
extern std::map<std::string,Entries> storage;
extern unsigned nvsWrites;
extern bool failOpen,failWrite,corruptWrite;
struct Preferences {
  std::string ns;
  bool readonly=false;
  bool begin(const char *name,bool ro=false) { ns=name;readonly=ro; if(failOpen)return false;storage[ns];return true; }
  void end() {}
  bool isKey(const char *key) { return storage[ns].count(key); }
  size_t getBytesLength(const char *key) { return isKey(key)?storage[ns][key].size():0; }
  size_t getBytes(const char *key,void *p,size_t n) { if(!isKey(key)||n<getBytesLength(key))return 0;auto &v=storage[ns][key];memcpy(p,v.data(),v.size());return v.size(); }
  size_t putBytes(const char *key,const void *p,size_t n) { if(failWrite||readonly)return 0;auto s=(const uint8_t*)p; storage[ns][key]=std::vector<uint8_t>(s,s+n);nvsWrites++;if(corruptWrite)storage[ns][key][0]^=1;return n; }
};
