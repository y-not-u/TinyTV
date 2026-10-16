#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
uint32_t clockMs;
int levels[17];
unsigned singles = 0, doubles = 0, longs = 0;
unsigned long millis() { return clockMs; }
int digitalRead(int pin) { return levels[pin]; }
void Touch_onSingleClick() { ++singles; }
void Touch_onDoubleClick() { ++doubles; }
void Touch_onLongPress() { ++longs; }
struct { void print(const char*) {} void println(int) {} } Serial;
#include "TouchPoll.inc"
static_assert(TOUCH_DOUBLE_CLICK_MS == 450, "Keep the original double-tap window");
static void tick(uint32_t elapsed, bool pressed) {
  clockMs += elapsed; levels[4] = pressed ? 0 : 1; Touch_poll();
}
static void press() { tick(1, true); tick(TOUCH_DEBOUNCE_MS, true); }
static void release() { tick(1, false); tick(TOUCH_DEBOUNCE_MS, false); }
int main(int argc, char** argv) {
  assert(argc == 2);
  levels[4] = levels[12] = levels[16] = 1;
  if(!strcmp(argv[1], "wrap")) clockMs = 0xffffff00;
  Touch_poll();
  if(!strcmp(argv[1], "bounce")) {
    tick(1, true); tick(10, false); tick(10, true); tick(10, false);
    tick(TOUCH_DOUBLE_CLICK_MS + 100, false);
    assert(!singles && !doubles && !longs); return 0;
  }
  press();
  if(!strcmp(argv[1], "long")) {
    tick(TOUCH_LONG_PRESS_MS, true); assert(!longs); release();
    assert(longs == 1 && !singles && !doubles);
    tick(1000, false); assert(!singles && !doubles); return 0;
  }
  release();
  if(!strcmp(argv[1], "double")) {
    tick(100, false); press(); release();
    tick(TOUCH_DOUBLE_CLICK_MS, false); assert(!doubles && !singles);
    tick(1, false); assert(doubles == 1 && !singles && !longs);
  }
  else {
    tick(TOUCH_DOUBLE_CLICK_MS, false); assert(!singles);
    tick(1, false); assert(singles == 1 && !doubles && !longs);
  }
}
