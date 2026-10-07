#include <Arduino.h>
#include <M5Unified.h>
#include "BleKeyboard.h"
#include "Gesture.h"
#include "generated_config.h"

namespace {
using namespace ClickerConfig;
ClickerKeyboard keyboard;
Gesture mainGesture(DEBOUNCE_MS, DOUBLE_CLICK_MS, LONG_PRESS_MS,
                    MAIN.single, MAIN.doubleClick, MAIN.longPress);
Gesture secondaryGesture(DEBOUNCE_MS, DOUBLE_CLICK_MS, LONG_PRESS_MS,
                         SECONDARY.single, SECONDARY.doubleClick, SECONDARY.longPress);
M5Canvas screen(&M5.Display);
int battery = -1;
bool charging = false;
bool wasReady = false;
uint32_t lastBattery = 0;
uint32_t lastScreen = 0;
int previousState = -1;

String keyName(uint8_t key) {
  switch (key) {
    case 0: return "-";
    case 0x28: return "Enter";
    case 0x29: return "Esc";
    case 0x2a: return "Bksp";
    case 0x2b: return "Tab";
    case 0x2c: return "Space";
    case 0x4a: return "Home";
    case 0x4b: return "PgUp";
    case 0x4d: return "End";
    case 0x4e: return "PgDn";
    case 0x4f: return "Right";
    case 0x50: return "Left";
    case 0x51: return "Down";
    case 0x52: return "Up";
  }
  if (key >= 4 && key <= 29) return String(char('A' + key - 4));
  if (key >= 0x1e && key <= 0x26) return String(char('1' + key - 0x1e));
  if (key == 0x27) return "0";
  if (key >= 0x3a && key <= 0x45) return String("F") + String(key - 0x39);
  return "?";
}

void drawStatus() {
  const int state = keyboard.ready() ? 2 : keyboard.connected() ? 1 : 0;
  screen.fillScreen(TFT_BLACK);
  screen.setTextColor(TFT_WHITE);
  screen.setTextSize(1);
  screen.setCursor(5, 5);
  screen.print(DEVICE_NAME);
  screen.setTextSize(2);
  screen.setTextColor(state == 2 ? TFT_GREEN : TFT_YELLOW);
  screen.setCursor(5, 22);
  screen.print(state == 2 ? "READY" : state == 1 ? "CONNECTING" : "PAIR / CONNECT");
  screen.setTextSize(1);
  screen.setTextColor(TFT_WHITE);
  screen.setCursor(5, 46);
  if (battery < 0) screen.print("Battery --");
  else screen.printf("Battery %d%%", battery);
  if (charging) screen.print("  USB charging");
  screen.setCursor(5, 64);
  screen.print("          TAP    DOUBLE  HOLD");
  screen.setCursor(5, 79);
  screen.printf("A Main    %-6s %-6s %s", keyName(MAIN.single).c_str(),
                keyName(MAIN.doubleClick).c_str(), keyName(MAIN.longPress).c_str());
  screen.setCursor(5, 94);
  screen.printf("B Side    %-6s %-6s %s", keyName(SECONDARY.single).c_str(),
                keyName(SECONDARY.doubleClick).c_str(), keyName(SECONDARY.longPress).c_str());
  screen.setTextColor(TFT_LIGHTGREY);
  screen.setCursor(5, 116);
  screen.print(state == 2 ? "Bluetooth keyboard ready" : "Mac: Settings > Bluetooth");
  screen.pushSprite(0, 0);
  if (state != previousState) {
    Serial.printf("Bluetooth state: %s\n", state == 2 ? "READY" : state == 1 ? "CONNECTING" : "ADVERTISING");
    previousState = state;
  }
}

void readBattery() {
  battery = M5.Power.getBatteryLevel();
  if (battery > 100) battery = 100;
  charging = M5.Power.isCharging() == m5::Power_Class::is_charging;
  if (battery >= 0) keyboard.setBatteryLevel(battery);
  Serial.printf("Battery: %d%%, %dmV, charging=%d\n", battery,
                M5.Power.getBatteryVoltage(), charging);
}

void startupBondReset() {
  // Only enter this maintenance action when both application buttons are held
  // at boot. No presentation input is processed until they are released.
  M5.update();
  if (!M5.BtnA.isPressed() || !M5.BtnB.isPressed()) return;
  uint32_t started = millis();
  while (M5.BtnA.isPressed() && M5.BtnB.isPressed()) {
    uint32_t elapsed = millis() - started;
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_YELLOW);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(5, 20);
    M5.Display.println("Reset Bluetooth");
    M5.Display.setTextSize(1);
    M5.Display.printf("Keep A+B held: %lus", (unsigned long)((3000 - min(elapsed, uint32_t(3000)) + 999) / 1000));
    if (elapsed >= 3000) {
      keyboard.clearBonds();
      Serial.println("Bluetooth bonds cleared by startup A+B hold");
      M5.Display.println("\nBonds cleared. Release A+B.\nForget device on Mac, then pair.");
      while (M5.BtnA.isPressed() || M5.BtnB.isPressed()) {
        M5.update();
        delay(10);
      }
      break;
    }
    delay(20);
    M5.update();
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  auto cfg = M5.config();
  cfg.internal_spk = false;
  cfg.internal_mic = false;
  cfg.internal_imu = false;
  cfg.internal_rtc = false;
  cfg.output_power = false;
  M5.begin(cfg);
  M5.Display.setRotation(1);
  M5.Display.setBrightness(BRIGHTNESS);
  // Gesture owns debounce, so M5Unified only supplies current button state.
  M5.BtnA.setDebounceThresh(0);
  M5.BtnB.setDebounceThresh(0);
  screen.setColorDepth(16);
  screen.createSprite(240, 135);
  Serial.printf("Stick-S3 Deck Clicker boot; board=%d (expected %d), flash=%u, PSRAM=%u\n",
                int(M5.getBoard()), int(m5::board_t::board_M5StickS3),
                ESP.getFlashChipSize(), ESP.getPsramSize());
  if (M5.getBoard() != m5::board_t::board_M5StickS3) {
    M5.Display.setTextColor(TFT_RED);
    M5.Display.println("Unexpected board. Check serial.");
    Serial.println("ERROR: expected StickS3; keyboard disabled");
    while (true) delay(1000);
  }
  keyboard.begin(DEVICE_NAME);
  startupBondReset();
  M5.update();
  mainGesture.reset(M5.BtnA.isPressed(), millis());
  secondaryGesture.reset(M5.BtnB.isPressed(), millis());
  readBattery();
  drawStatus();
}

void loop() {
  M5.update();
  const uint32_t now = millis();
  keyboard.update(now);
  const bool ready = keyboard.ready();
  if (!ready || ready != wasReady) {
    mainGesture.reset(M5.BtnA.isPressed(), now);
    secondaryGesture.reset(M5.BtnB.isPressed(), now);
  } else {
    const uint8_t actions[] = {
      mainGesture.update(M5.BtnA.isPressed(), now),
      secondaryGesture.update(M5.BtnB.isPressed(), now)
    };
    for (uint8_t action : actions) {
      if (action) Serial.printf("Key %s: %s\n", keyName(action).c_str(), keyboard.tap(action) ? "queued" : "discarded");
    }
  }
  wasReady = ready;
  if (uint32_t(now - lastBattery) >= 30000) {
    lastBattery = now;
    readBattery();
  }
  const int state = ready ? 2 : keyboard.connected() ? 1 : 0;
  if (state != previousState || uint32_t(now - lastScreen) >= 1000) {
    lastScreen = now;
    drawStatus();
  }
  delay(1);
}
