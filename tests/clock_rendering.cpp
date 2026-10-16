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
ClockValues values[] = {{23,59,59,9,30,4},{0,0,0,10,1,5}};
time_t stamp = 0;
time_t now() { return stamp; }
#define CLOCK_ACCESSOR(field) int field(time_t t) { return values[t].field; } int field() { return field(now()); }
CLOCK_ACCESSOR(hour)
CLOCK_ACCESSOR(minute)
CLOCK_ACCESSOR(second)
CLOCK_ACCESSOR(month)
CLOCK_ACCESSOR(day)
CLOCK_ACCESSOR(weekday)
uint8_t Hour_sign=60, Minute_sign=60, Second_sign=60;
uint16_t bgColor=0, TFT_WHITE=0xffff;
std::vector<int> digits;
std::vector<std::string> labels;
unsigned clears = 0;
struct {
  void fillRect(int,int,int,int,uint16_t) { ++clears; }
} tft;
struct Dig {
  void record(int n) { digits.push_back(n); stamp=1; } // Time crosses midnight while a JPEG is decoded.
  void printfW3660(int,int,int n) { record(n); }
  void printfO3660(int,int,int n) { record(n); }
  void printfW1830(int,int,int n) { record(n); }
} dig;
int Text_width(const char*, int) { return 48; }
void Text_draw(int,int,const char* text,int,uint16_t,uint16_t,int) { labels.push_back(text); }
String week();
String monthDay();
String week(time_t);
String monthDay(time_t);
#include "ClockNtp.ino"
int main() {
  digitalClockDisplay(1);
  assert((digits == std::vector<int>{2,3,5,9,5,9}));
  assert(labels.size()==2 && labels[0]==u8"9月 30日" && labels[1]==u8"星期三");
  digits.clear(); labels.clear();
  digitalClockDisplay(0);
  assert((digits == std::vector<int>{0,0,0,0,0,0}));
  assert(labels.size()==2 && labels[0]==u8"10月 1日" && labels[1]==u8"星期四");
  assert(clears==2); // Both date fields replace their old complete region.
  digits.clear(); labels.clear(); digitalClockDisplay(0);
  assert(digits.empty() && labels.empty() && clears==2);
}
