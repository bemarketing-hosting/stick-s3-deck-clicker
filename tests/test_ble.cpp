// Compile the production transport against controllable BLE/clock substitutes.
#include "BleKeyboard.h"
#include <Arduino.h>
#include <cassert>
#include <iostream>

struct Fixture {
  ClickerKeyboard keyboard;
  NimBLEConnInfo peer;
  Fixture() { fakeMillis = 100; keyboard.begin("Test clicker"); }
  ~Fixture() { delete NimBLEHIDDevice::last; }
  NimBLECharacteristic& input() { return NimBLEHIDDevice::last->input; }
  void connect() { NimBLEDevice::server.callbacks->onConnect(&NimBLEDevice::server, peer); }
  void encrypt(bool value = true) {
    peer.encrypted = value;
    NimBLEDevice::server.callbacks->onAuthenticationComplete(peer);
  }
  void subscribe(uint16_t value = 1) { input().callbacks->onSubscribe(&input(), peer, value); }
  void disconnect() {
    NimBLEDevice::server.callbacks->onDisconnect(&NimBLEDevice::server, peer, 0);
  }
  void tick(uint32_t t) { fakeMillis = t; keyboard.update(t); }
  void ready() { connect(); encrypt(); subscribe(); tick(fakeMillis); }
  void expect(size_t index, uint8_t usage) {
    assert(index < input().reports.size());
    assert(input().reports[index] == std::vector<uint8_t>({0, 0, usage, 0, 0, 0, 0, 0}));
    assert(input().handles[index] == peer.handle);
  }
};

int main() {
  {
    Fixture f;
    assert(NimBLEDevice::bonding && NimBLEDevice::secureConnections);
    assert(NimBLEDevice::server.autoAdvertise && NimBLEDevice::advertising.started);
    assert(!f.keyboard.connected() && !f.keyboard.ready());
    assert(!f.keyboard.tap(0x4f));
    f.connect();
    assert(f.keyboard.connected() && !f.keyboard.ready());
    f.subscribe();
    assert(!f.keyboard.ready());
    f.encrypt();
    assert(f.keyboard.ready());
    f.tick(100);
    assert(f.input().reports.size() == 1);
    f.expect(0, 0);
    assert(!f.keyboard.tap(0) && !f.keyboard.tap(0x66));
    assert(f.keyboard.tap(0x4f));
    f.tick(100);
    f.expect(1, 0x4f);
    f.tick(124);
    assert(f.input().reports.size() == 2);
    f.tick(125);
    f.expect(2, 0);
    f.tick(200);
    assert(f.input().reports.size() == 3);
  }
  {
    Fixture f;
    f.ready();
    f.keyboard.tap(0x4f);
    f.keyboard.tap(0x50);
    f.tick(100);
    f.input().failCount = 2;
    f.tick(125);
    f.tick(126);
    assert(f.input().reports.size() == 2); // failed releases cannot advance queue
    f.tick(127);
    f.expect(2, 0);
    f.tick(128);
    f.expect(3, 0x50);
    f.tick(153);
    f.expect(4, 0);
  }
  {
    Fixture f;
    f.ready();
    for (int i = 0; i < 8; ++i) assert(f.keyboard.tap(0x4f));
    assert(!f.keyboard.tap(0x50));
    f.tick(401);
    assert(f.input().reports.size() == 1); // stale queue expires without key-down
    assert(f.keyboard.tap(0x50));
    f.tick(401);
    f.expect(1, 0x50);
  }
  {
    Fixture f;
    f.ready();
    f.keyboard.tap(0x4f);
    f.keyboard.tap(0x50);
    f.tick(100);
    f.disconnect();
    assert(!f.keyboard.ready() && !f.keyboard.tap(0x29));
    // Reconnect fully between loop iterations: generation must still clear work.
    f.connect(); f.encrypt(); f.subscribe(); f.tick(125);
    assert(f.input().reports.size() == 3);
    f.expect(2, 0);
    f.tick(150);
    assert(f.input().reports.size() == 3);
    f.keyboard.tap(0x4f);
    f.subscribe(0);
    f.subscribe(1);
    f.tick(151);
    assert(f.input().reports.size() == 4);
    f.expect(3, 0);
  }
  {
    Fixture f;
    f.ready();
    f.keyboard.tap(0x4f);
    f.tick(100);
    f.keyboard.clearBonds();
    f.expect(2, 0);
    assert(!f.keyboard.ready());
    assert(NimBLEDevice::bondsDeleted);
    assert(NimBLEDevice::server.disconnected.back() == f.peer.handle);
    f.tick(125);
    assert(f.input().reports.size() == 3);
  }
  {
    Fixture f;
    f.connect();
    f.encrypt(false);
    assert(!f.keyboard.ready());
    assert(NimBLEDevice::server.disconnected.back() == f.peer.handle);
    f.keyboard.setBatteryLevel(255);
    assert(NimBLEHIDDevice::last->battery == 100);
  }
  {
    Fixture f;
    f.ready();
    fakeMillis = UINT32_MAX - 10;
    assert(f.keyboard.tap(0x4f));
    f.tick(fakeMillis);
    f.tick(13);
    assert(f.input().reports.size() == 2);
    f.tick(14);
    f.expect(2, 0); // release deadline survives millis wraparound
  }
  std::cout << "BLE transport tests passed\n";
}
