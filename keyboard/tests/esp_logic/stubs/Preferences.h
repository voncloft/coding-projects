#pragma once
#include "Arduino.h"
class Preferences {public:std::map<std::string,String> strings;std::map<std::string,uint8_t> bytes;void begin(const char*,bool){}void putUChar(const char*k,uint8_t v){bytes[k]=v;}uint8_t getUChar(const char*k,uint8_t d){return bytes.count(k)?bytes[k]:d;}void putString(const char*k,const String&v){strings[k]=v;}String getString(const char*k,const char*d){return strings.count(k)?strings[k]:String(d);}};
