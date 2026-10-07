#pragma once
#include <stdint.h>

// Framework-independent, debounced one-shot gestures. Call on every loop tick.
class Gesture {
 public:
  Gesture(uint32_t debounce, uint32_t doubleMs, uint32_t longMs,
          uint8_t single, uint8_t dbl, uint8_t hold)
      : debounce_(debounce), doubleMs_(doubleMs), longMs_(longMs),
        single_(single), double_(dbl), hold_(hold) {}

  // Reset when connection readiness changes. A button already down must first
  // be released; it cannot leak a stale gesture into the next connection.
  void reset(bool rawPressed, uint32_t now) {
    raw_ = pressed_ = rawPressed;
    changedAt_ = pressedAt_ = releasedAt_ = now;
    blocked_ = rawPressed;
    held_ = pending_ = second_ = false;
  }

  uint8_t update(bool rawPressed, uint32_t now) {
    if (rawPressed != raw_) {
      raw_ = rawPressed;
      changedAt_ = now;
    }
    uint8_t action = 0;
    // Once the second press is debounced within the window, await its release
    // or hold even if that takes longer than the double-click window.
    if (pending_ && !second_ && elapsed(now, releasedAt_) >= doubleMs_) {
      pending_ = false;
      action = single_;
    }
    if (raw_ != pressed_ && elapsed(now, changedAt_) >= debounce_) {
      pressed_ = raw_;
      if (pressed_) {
        pressedAt_ = now;
        held_ = false;
        second_ = pending_;
      } else if (blocked_) {
        blocked_ = false;
      } else if (held_) {
        held_ = false;
      } else if (second_) {
        pending_ = second_ = false;
        action = double_;
      } else if (double_) {
        pending_ = true;
        releasedAt_ = now;
      } else {
        action = single_;
      }
    }
    if (pressed_ && raw_ && !blocked_ && !held_ && hold_ &&
        elapsed(now, pressedAt_) >= longMs_) {
      held_ = true;
      pending_ = second_ = false;
      action = hold_;
    }
    return action;
  }

 private:
  static uint32_t elapsed(uint32_t now, uint32_t since) { return now - since; }
  uint32_t debounce_, doubleMs_, longMs_;
  uint8_t single_, double_, hold_;
  uint32_t changedAt_ = 0, pressedAt_ = 0, releasedAt_ = 0;
  bool raw_ = false, pressed_ = false, blocked_ = false;
  bool held_ = false, pending_ = false, second_ = false;
};
