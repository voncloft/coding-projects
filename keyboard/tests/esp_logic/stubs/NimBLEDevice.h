#pragma once
#include "Arduino.h"
class NimBLEAddress {public:std::string text;uint8_t type;NimBLEAddress(const char*s="",uint8_t t=0):text(s),type(t){}bool operator!=(const NimBLEAddress&a)const{return text!=a.text||type!=a.type;}uint8_t getType(){return type;}std::string toString(){return text;}};
class NimBLEConnInfo {public:uint16_t handle=1;NimBLEAddress addr;bool valid=true;uint16_t getConnHandle(){return handle;}NimBLEAddress getIdAddress(){return addr;}bool isEncrypted(){return valid;}bool isAuthenticated(){return valid;}bool isBonded(){return valid;}};
class NimBLEServer;class NimBLECharacteristic;
class NimBLEServerCallbacks {public:virtual ~NimBLEServerCallbacks()=default;virtual void onConnect(NimBLEServer*,NimBLEConnInfo&){}virtual void onDisconnect(NimBLEServer*,NimBLEConnInfo&,int){}virtual void onPassKeyEntry(NimBLEConnInfo&){}virtual void onAuthenticationComplete(NimBLEConnInfo&){} };
class NimBLECharacteristicCallbacks {public:virtual ~NimBLECharacteristicCallbacks()=default;virtual void onWrite(NimBLECharacteristic*,NimBLEConnInfo&){} };
class NimBLECharacteristic {public:std::vector<uint8_t> value;unsigned notifications=0;void setValue(const uint8_t*p,size_t n){value.assign(p,p+n);}void notify(){notifications++;}std::vector<uint8_t> getValue(){return value;}void setCallbacks(NimBLECharacteristicCallbacks*){} };
class NimBLEServer {public:std::vector<uint16_t> disconnected;void disconnect(uint16_t h){disconnected.push_back(h);}void setCallbacks(NimBLEServerCallbacks*){}NimBLEConnInfo getPeerInfoByHandle(uint16_t h){NimBLEConnInfo i;i.handle=h;return i;}};
class NimBLEAdvertising {public:bool active=false,filtered=false;unsigned duration=0;void stop(){active=false;}void setScanFilter(bool,bool f){filtered=f;}void start(unsigned d){active=true;duration=d;}void setAppearance(int){}void addServiceUUID(int){}void enableScanResponse(bool){}void setName(const char*){} };
class NimBLEService {public:int getUUID(){return 0;}};
class NimBLEDevice {public:inline static NimBLEAdvertising adv;inline static NimBLEServer srv;inline static uint32_t passkey=UINT32_MAX;inline static std::vector<std::string> whitelist;
 static NimBLEAdvertising*getAdvertising(){return &adv;}static NimBLEServer*createServer(){return &srv;}
 static void whiteListRemove(const NimBLEAddress&a){whitelist.erase(std::remove(whitelist.begin(),whitelist.end(),a.text),whitelist.end());}static void whiteListAdd(const NimBLEAddress&a){whitelist.push_back(a.text);}static void deleteBond(const NimBLEAddress&){}static void startSecurity(uint16_t){}static void injectPassKey(NimBLEConnInfo,uint32_t v){passkey=v;}
 static void init(const char*){}static void setSecurityAuth(bool,bool,bool){}static void setSecurityIOCap(int){}static void setMTU(int){}
};
#define BLE_HS_IO_KEYBOARD_ONLY 0
