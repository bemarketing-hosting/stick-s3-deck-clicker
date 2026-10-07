#pragma once

#include <atomic>
#include <cstdint>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>

// Call begin/update/tap/setBatteryLevel/clearBonds from the Arduino loop task.
// BLE callbacks only publish atomic connection state; they never touch the queue.
class ClickerKeyboard : private NimBLEServerCallbacks,
                        private NimBLECharacteristicCallbacks {
 public:
  void begin(const char* name);
  void update(uint32_t now);
  bool tap(uint8_t usage);
  bool connected() const;
  bool ready() const;
  void setBatteryLevel(uint8_t level);
  void clearBonds();

 private:
  void onConnect(NimBLEServer*, NimBLEConnInfo&) override;
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int reason) override;
  void onAuthenticationComplete(NimBLEConnInfo&) override;
  void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo&, uint16_t) override;
  bool send(uint8_t usage);
  void resetPending();

  NimBLEServer* server_ = nullptr;
  NimBLEHIDDevice* hid_ = nullptr;
  NimBLECharacteristic* input_ = nullptr;
  std::atomic<uint16_t> connection_{BLE_HS_CONN_HANDLE_NONE};
  std::atomic<bool> encrypted_{false};
  std::atomic<bool> subscribed_{false};
  std::atomic<uint32_t> generation_{0};
  uint32_t seenGeneration_ = 0;
  static constexpr uint8_t kQueueSize = 8;
  struct Pending { uint8_t usage; uint32_t queuedAt; };
  Pending queue_[kQueueSize]{};
  uint8_t head_ = 0;
  uint8_t count_ = 0;
  bool keyDown_ = false;
  bool baselinePending_ = true;
  uint32_t sentAt_ = 0;
};
