#include <ctime>
#include <string>
#include <vector>
#include <assert.h>
#include <stdint.h>
class String {
  std::string value;
public:
  String(const char* text = "") : value(text) {}
  String(int number) : value(std::to_string(number)) {}
  const char* c_str() const { return value.c_str(); }
  friend String operator+(String a, const String &b) { a.value += b.value; return a; }
};
struct ClockValues { int hour, minute, second, month, day, weekday; };
ClockValues values[] = {{23,59,59,9,30,4},{0,0,0,10,1,5},{0,0,1,10,1,5}};
time_t stamp = 0;
time_t now() { return stamp; }
#define CLOCK_ACCESSOR(field) int field(time_t t) { return values[t].field; } int field() { return field(now()); }
CLOCK_ACCESSOR(hour)
CLOCK_ACCESSOR(minute)
CLOCK_ACCESSOR(second)
CLOCK_ACCESSOR(month)
CLOCK_ACCESSOR(day)
CLOCK_ACCESSOR(weekday)
uint16_t bgColor=0;
std::vector<std::string> labels;
struct {
  uint16_t dots[240][240] = {};
  bool writing = false;
  unsigned clears = 0;
  unsigned writes = 0;
  unsigned transactions = 0;
  void fillRect(int,int,int,int,uint16_t) { assert(!writing); ++clears; }
  void startWrite() { assert(!writing); writing = true; ++transactions; }
  void endWrite() { assert(writing); writing = false; }
  void fillCircle(int x, int y, int radius, uint16_t color) {
    assert(x >= radius && x + radius < 240 && y >= radius && y + radius < 240);
    dots[y][x] = color;
    ++writes;
    if(stamp == 0) stamp = 1; // Time crosses midnight during the first draw.
  }
} tft;
int Text_width(const char*, int) { return 48; }
void Text_draw(int,int,const char* text,int,uint16_t,uint16_t,int) { labels.push_back(text); }
String week(time_t);
String monthDay(time_t);
#include "ClockNtp.ino"

static void checkDigits(const int (&digits)[6]) {
  const int positions[] = {19, 69, 135, 185, 100, 124};
  for(int i = 0; i < 6; ++i) {
    const int pitch = i < 4 ? 9 : 4;
    const int y = i < 4 ? 92 : 175;
    const uint16_t color = i < 4 ? TinyTV::Theme::text : TinyTV::Theme::red;
    for(int row = 0; row < 7; ++row) {
      for(int column = 0; column < 5; ++column) {
        const bool lit = TinyTV::pixelDigits[digits[i]][row] & (1 << (4 - column));
        assert(tft.dots[y + row * pitch][positions[i] + column * pitch] == (lit ? color : bgColor));
      }
    }
  }
}

int main() {
  digitalClockDisplay(true);
  checkDigits({2,3,5,9,5,9});
  assert(tft.dots[110][120] == TinyTV::Theme::red && tft.dots[128][120] == TinyTV::Theme::red);
  assert(labels.size()==2 && labels[0]==u8"9月 30日" && labels[1]==u8"星期三");
  labels.clear();
  digitalClockDisplay(false);
  checkDigits({0,0,0,0,0,0});
  assert(labels.size()==2 && labels[0]==u8"10月 1日" && labels[1]==u8"星期四");
  assert(tft.clears==2); // Both date fields replace their old complete region.

  labels.clear();
  unsigned writes = tft.writes;
  digitalClockDisplay(false);
  assert(tft.writes==writes && labels.empty() && tft.clears==2);

  unsigned transactions = tft.transactions;
  stamp = 2;
  digitalClockDisplay(false);
  checkDigits({0,0,0,0,0,1});
  assert(tft.transactions==transactions+1 && labels.empty() && tft.clears==2);
}
