#include "Gesture.h"
#include <cassert>
#include <cstdint>
#include <iostream>

static constexpr uint8_t TAP = 79, DOUBLE = 44, HOLD = 41;

// Times supplied to press/release denote raw electrical edges; debounce is 25ms.
static void press(Gesture& g, uint32_t t) {
  assert(g.update(true, t) == 0);
  assert(g.update(true, t + 25) == 0);
}
static uint8_t release(Gesture& g, uint32_t t) {
  assert(g.update(false, t) == 0);
  return g.update(false, t + 25);
}

int main() {
  {
    Gesture g(25, 250, 650, TAP, 0, 0);
    assert(g.update(true, 10) == 0);
    assert(g.update(false, 20) == 0); // noise never becomes a press
    assert(g.update(false, 45) == 0);
    press(g, 100);
    assert(release(g, 150) == TAP);
    press(g, 180);
    assert(release(g, 230) == TAP); // no double assignment: two immediate taps
    press(g, 300);
    assert(g.update(true, 2000) == 0);
    assert(release(g, 2100) == TAP); // unassigned hold is ordinary tap
  }
  {
    Gesture g(25, 250, 650, TAP, 0, HOLD);
    press(g, 10);
    assert(g.update(true, 684) == 0);
    assert(g.update(true, 685) == HOLD);
    assert(g.update(true, 2000) == 0);
    assert(release(g, 2100) == 0);
    press(g, 2200);
    assert(release(g, 2300) == TAP);
  }
  {
    Gesture g(25, 250, 650, TAP, 0, HOLD);
    press(g, 10);
    assert(g.update(false, 680) == 0);
    assert(g.update(false, 685) == 0); // release before threshold cannot hold
    assert(g.update(false, 705) == TAP);
  }
  {
    Gesture g(25, 250, 650, TAP, DOUBLE, HOLD);
    press(g, 10);
    assert(release(g, 60) == 0);
    assert(g.update(false, 334) == 0);
    assert(g.update(false, 335) == TAP);
    assert(g.update(false, 500) == 0);
    press(g, 600);
    assert(release(g, 650) == 0);
    press(g, 700);
    assert(release(g, 800) == DOUBLE);
    assert(g.update(false, 1100) == 0);
  }
  {
    Gesture g(25, 250, 650, TAP, DOUBLE, HOLD);
    press(g, 10);
    assert(release(g, 60) == 0);
    press(g, 100);
    assert(g.update(true, 400) == 0); // second press locks arbitration
    assert(g.update(true, 775) == HOLD); // second hold wins
    assert(release(g, 900) == 0);
    assert(g.update(false, 1500) == 0);
  }
  {
    Gesture g(25, 250, 650, TAP, DOUBLE, HOLD);
    press(g, 10);
    assert(release(g, 60) == 0);
    g.reset(false, 100); // pending action discarded on disconnect
    assert(g.update(false, 500) == 0);
    g.reset(true, 600); // held button across reconnect must release first
    assert(g.update(true, 2000) == 0);
    assert(release(g, 2100) == 0);
    press(g, 2200);
    assert(g.update(true, 2875) == HOLD);
    assert(release(g, 3000) == 0);
  }
  {
    Gesture g(25, 250, 650, TAP, DOUBLE, HOLD);
    const uint32_t start = UINT32_MAX - 100;
    g.reset(false, start);
    press(g, start + 10);
    assert(release(g, start + 60) == 0);
    assert(g.update(false, start + 335) == TAP); // wrapped unsigned clock
    press(g, start + 400);
    assert(g.update(true, start + 1075) == HOLD);
  }
  {
    Gesture a(25, 250, 650, TAP, 0, 0), b(25, 250, 650, 80, 0, HOLD);
    press(a, 10);
    press(b, 10);
    assert(release(a, 100) == TAP);
    assert(b.update(true, 685) == HOLD);
    assert(release(b, 700) == 0);
  }
  std::cout << "Gesture tests passed\n";
}
