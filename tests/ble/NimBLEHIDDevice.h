#pragma once
#include "NimBLEDevice.h"
constexpr int HID_KEYBOARD = 0x03c1;
struct NimBLEHIDDevice {
  inline static NimBLEHIDDevice* last = nullptr;
  NimBLECharacteristic input;
  NimBLECharacteristic output;
  NimBLEService service;
  uint8_t battery = 100;
  explicit NimBLEHIDDevice(NimBLEServer*) { last = this; }
  NimBLECharacteristic* getInputReport(uint8_t) { return &input; }
  NimBLECharacteristic* getOutputReport(uint8_t) { return &output; }
  void setManufacturer(const char*) {}
  void setPnp(uint8_t, uint16_t, uint16_t, uint16_t) {}
  void setHidInfo(uint8_t, uint8_t) {}
  void setReportMap(uint8_t*, size_t) {}
  void setBatteryLevel(uint8_t level, bool = false) { battery = level; }
  NimBLEService* getHidService() { return &service; }
};
