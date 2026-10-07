#include "BleKeyboard.h"
#include <Arduino.h>

namespace {
// Keyboard application collection: modifiers, reserved byte, six key usages.
// Report ID is carried by the GATT report-reference descriptor, not payload.
uint8_t reportMap[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01,
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00,
    0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
    0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01,
    0x29, 0x05, 0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x01,
    0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,
    0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xC0};
constexpr uint32_t kPressMs = 25;
constexpr uint32_t kMaxQueueAgeMs = 300;
}

void ClickerKeyboard::begin(const char* name) {
  NimBLEDevice::init(name);
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
  NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  server_ = NimBLEDevice::createServer();
  server_->setCallbacks(this, false);
  server_->advertiseOnDisconnect(true);
  hid_ = new NimBLEHIDDevice(server_);
  input_ = hid_->getInputReport(1);
  input_->setCallbacks(this);
  hid_->getOutputReport(1);  // Keyboard LED report; no physical LEDs to update.
  hid_->setManufacturer("Deck Clicker");
  // Unassigned USB vendor/product IDs for this private prototype.
  hid_->setPnp(2, 0, 0, 0x0100);
  hid_->setHidInfo(0, 0x02);
  hid_->setReportMap(reportMap, sizeof(reportMap));
  uint8_t released[8]{};
  input_->setValue(released, sizeof(released));
  hid_->setBatteryLevel(100);
  server_->start();
  auto* advertising = NimBLEDevice::getAdvertising();
  advertising->setAppearance(HID_KEYBOARD);
  advertising->addServiceUUID(hid_->getHidService()->getUUID());
  advertising->enableScanResponse(true);
  advertising->setName(name);
  advertising->setPreferredParams(12, 24);
  advertising->start();
  Serial.printf("BLE advertising: %s\n", name);
}

bool ClickerKeyboard::connected() const {
  return connection_.load() != BLE_HS_CONN_HANDLE_NONE;
}

bool ClickerKeyboard::ready() const {
  return connected() && encrypted_.load() && subscribed_.load();
}

void ClickerKeyboard::resetPending() {
  head_ = count_ = 0;
  keyDown_ = false;
  baselinePending_ = true;
}

bool ClickerKeyboard::tap(uint8_t usage) {
  const uint32_t generation = generation_.load();
  if (generation != seenGeneration_) {
    resetPending();
    seenGeneration_ = generation;
  }
  if (!ready() || usage == 0 || usage > 0x65 || count_ == kQueueSize) return false;
  queue_[(head_ + count_) % kQueueSize] = {usage, millis()};
  ++count_;
  return true;
}

bool ClickerKeyboard::send(uint8_t usage) {
  if (!ready()) return false;
  const uint16_t handle = connection_.load();
  uint8_t report[8] = {0, 0, usage, 0, 0, 0, 0, 0};
  input_->setValue(report, sizeof(report));
  return input_->notify(report, sizeof(report), handle);
}

void ClickerKeyboard::update(uint32_t now) {
  const uint32_t generation = generation_.load();
  if (generation != seenGeneration_) {
    resetPending();
    seenGeneration_ = generation;
  }
  if (!ready()) {
    resetPending();
    return;
  }
  // Establish a neutral report after every connection/subscription transition.
  if (baselinePending_) {
    if (!send(0)) return;
    baselinePending_ = false;
  }
  if (keyDown_) {
    if (uint32_t(now - sentAt_) < kPressMs) return;
    // Retry a failed release on subsequent loops, never advance to another key.
    if (!send(0)) return;
    keyDown_ = false;
    return;
  }
  while (count_ && uint32_t(now - queue_[head_].queuedAt) > kMaxQueueAgeMs) {
    head_ = (head_ + 1) % kQueueSize;
    --count_;
  }
  if (!count_) return;
  if (send(queue_[head_].usage)) {
    head_ = (head_ + 1) % kQueueSize;
    --count_;
    keyDown_ = true;
    sentAt_ = now;
  }
}

void ClickerKeyboard::setBatteryLevel(uint8_t level) {
  if (hid_) hid_->setBatteryLevel(level > 100 ? 100 : level, ready());
}

void ClickerKeyboard::clearBonds() {
  // Intended for startup, before accepting button gestures.
  const uint16_t handle = connection_.load();
  if (ready()) send(0);
  subscribed_.store(false);
  encrypted_.store(false);
  generation_.fetch_add(1);
  resetPending();
  if (server_ && handle != BLE_HS_CONN_HANDLE_NONE) server_->disconnect(handle);
  NimBLEDevice::deleteAllBonds();
}

void ClickerKeyboard::onConnect(NimBLEServer* server, NimBLEConnInfo& info) {
  if (connected()) {
    server->disconnect(info.getConnHandle());
    return;
  }
  encrypted_.store(false);
  subscribed_.store(false);
  connection_.store(info.getConnHandle());
  generation_.fetch_add(1);
  server->updateConnParams(info.getConnHandle(), 12, 24, 0, 400);
  NimBLEDevice::startSecurity(info.getConnHandle());
}

void ClickerKeyboard::onDisconnect(NimBLEServer*, NimBLEConnInfo& info, int) {
  if (connection_.load() != info.getConnHandle()) return;
  connection_.store(BLE_HS_CONN_HANDLE_NONE);
  encrypted_.store(false);
  subscribed_.store(false);
  generation_.fetch_add(1);
}

void ClickerKeyboard::onAuthenticationComplete(NimBLEConnInfo& info) {
  if (connection_.load() != info.getConnHandle()) return;
  encrypted_.store(info.isEncrypted());
  if (!info.isEncrypted()) {
    generation_.fetch_add(1);
    server_->disconnect(info.getConnHandle());
  }
}

void ClickerKeyboard::onSubscribe(NimBLECharacteristic*, NimBLEConnInfo& info,
                                  uint16_t value) {
  if (connection_.load() != info.getConnHandle()) return;
  subscribed_.store((value & 1) != 0);
  generation_.fetch_add(1);
}
