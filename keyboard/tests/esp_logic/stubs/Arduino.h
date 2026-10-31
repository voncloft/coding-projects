#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <vector>
using std::min;
class String:public std::string {public:using std::string::string;String(int x):std::string(std::to_string(x)){}String(const std::string&s):std::string(s){} };
static uint32_t clock_ms;static int pins[32];static unsigned pwm[2];
inline uint32_t millis(){return clock_ms;}inline void delay(unsigned n){clock_ms+=n;}
inline int digitalRead(int p){return pins[p];}inline void pinMode(int,int){}
inline void ledcSetup(int,int,int){}inline void ledcAttachPin(int,int){}
inline void ledcWrite(int c,unsigned v){pwm[c]=v;}
#define INPUT 0
#define SERIAL_8N1 0
class HardwareSerial {public:std::vector<uint8_t> out;std::deque<uint8_t> in;HardwareSerial(int=0){}void begin(int,int,int,int){}void write(const uint8_t*p,size_t n){out.insert(out.end(),p,p+n);}int available(){return in.size();}uint8_t read(){auto v=in.front();in.pop_front();return v;}};
static HardwareSerial Serial;
