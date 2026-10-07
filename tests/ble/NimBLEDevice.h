#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

constexpr uint16_t BLE_HS_CONN_HANDLE_NONE = 0xffff;
constexpr uint8_t BLE_HS_IO_NO_INPUT_OUTPUT = 3;
constexpr uint8_t BLE_SM_PAIR_KEY_DIST_ENC = 1;
constexpr uint8_t BLE_SM_PAIR_KEY_DIST_ID = 2;
struct NimBLEServer;
struct NimBLECharacteristic;
struct NimBLEConnInfo {
  uint16_t handle = 1;
  bool encrypted = false;
  uint16_t getConnHandle() const { return handle; }
  bool isEncrypted() const { return encrypted; }
};
struct NimBLEServerCallbacks {
  virtual ~NimBLEServerCallbacks() = default;
  virtual void onConnect(NimBLEServer*, NimBLEConnInfo&) {}
  virtual void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) {}
  virtual void onAuthenticationComplete(NimBLEConnInfo&) {}
};
struct NimBLECharacteristicCallbacks {
  virtual ~NimBLECharacteristicCallbacks() = default;
  virtual void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo&, uint16_t) {}
};
struct NimBLECharacteristic {
  NimBLECharacteristicCallbacks* callbacks = nullptr;
  std::vector<std::vector<uint8_t>> reports;
  std::vector<std::vector<uint8_t>> attempts;
  std::vector<uint8_t> value;
  std::vector<uint16_t> handles;
  int failCount = 0;
  void setCallbacks(NimBLECharacteristicCallbacks* c) { callbacks = c; }
  void setValue(const uint8_t* p, size_t n) { value.assign(p, p + n); }
  bool notify(const uint8_t* p, size_t n, uint16_t handle) {
    attempts.emplace_back(p, p + n);
    if (failCount) { --failCount; return false; }
    reports.emplace_back(p, p + n);
    handles.push_back(handle);
    return true;
  }
};
struct NimBLEService { int getUUID() { return 0x1812; } };
struct NimBLEAdvertising {
  bool started = false;
  void setAppearance(int) {}
  void addServiceUUID(int) {}
  void enableScanResponse(bool) {}
  void setName(const char*) {}
  void setPreferredParams(int, int) {}
  void start() { started = true; }
};
struct NimBLEServer {
  NimBLEServerCallbacks* callbacks = nullptr;
  bool autoAdvertise = false;
  std::vector<uint16_t> disconnected;
  void setCallbacks(NimBLEServerCallbacks* c, bool) { callbacks = c; }
  void advertiseOnDisconnect(bool b) { autoAdvertise = b; }
  void start() {}
  void updateConnParams(uint16_t, int, int, int, int) {}
  void disconnect(uint16_t h) { disconnected.push_back(h); }
};
struct NimBLEDevice {
  inline static NimBLEServer server;
  inline static NimBLEAdvertising advertising;
  inline static bool bondsDeleted = false;
  inline static bool bonding = false;
  inline static bool secureConnections = false;
  static void init(const char*) {
    server = NimBLEServer{};
    advertising = NimBLEAdvertising{};
    bondsDeleted = false;
  }
  static void setSecurityAuth(bool b, bool, bool sc) { bonding = b; secureConnections = sc; }
  static void setSecurityIOCap(uint8_t) {}
  static void setSecurityInitKey(uint8_t) {}
  static void setSecurityRespKey(uint8_t) {}
  static NimBLEServer* createServer() { return &server; }
  static NimBLEAdvertising* getAdvertising() { return &advertising; }
  static void startSecurity(uint16_t) {}
  static void deleteAllBonds() { bondsDeleted = true; }
};
