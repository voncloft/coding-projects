#pragma once
#include "NimBLEDevice.h"
class NimBLEHIDDevice {public:NimBLECharacteristic input[5],output[2],bootIn,bootOut;NimBLEService service;unsigned battery=255;
 NimBLEHIDDevice(NimBLEServer*){}void setManufacturer(const char*){}void setPnp(int,int,int,int){}void setHidInfo(int,int){}void setReportMap(uint8_t*,size_t){}void startServices(){}void setBatteryLevel(unsigned v){battery=v;}
 NimBLECharacteristic*getInputReport(int i){return &input[i];}NimBLECharacteristic*getOutputReport(int i){return &output[i];}NimBLECharacteristic*getBootInput(){return &bootIn;}NimBLECharacteristic*getBootOutput(){return &bootOut;}NimBLEService*getHidService(){return &service;}
};
