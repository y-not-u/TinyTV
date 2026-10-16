#include <assert.h>
#include <algorithm>
#include <string>
#include <vector>
#include "RuntimeCore.h"
#include "ChineseFont.h"
using std::max;
const int TL_DATUM = 0;
class String {
  std::string value;
public:
  String(const char* s = "") : value(s) {}
  void reserve(size_t n) { value.reserve(n); }
  void concat(const char* s, size_t n) { value.append(s, n); }
  String& operator+=(const char* s) { value += s; return *this; }
  const char* c_str() const { return value.c_str(); }
};
struct Region { int x, y, w, h; };
struct FakeTft {
  bool swap = false;
  std::vector<Region> regions;
  std::vector<uint16_t> pixels;
  int textWidth(const char* text, uint8_t font) {
    int result = 0;
    while(*text) {
      result += font == 1 ? 6 : *text == 'i' ? 4 : *text == 'M' ? 12 : 8;
      ++text;
    }
    return result;
  }
  bool getSwapBytes() { return swap; }
  void setSwapBytes(bool value) { swap = value; }
  void pushImage(int x, int y, int w, int h, uint16_t* data) {
    assert(swap); regions.push_back({x,y,w,h}); pixels.assign(data, data+w*h);
  }
  void setTextSize(int size) { assert(size == 1); }
  void setTextWrap(bool x, bool y) { assert(!x && !y); }
  void setTextDatum(int) {}
  void setTextColor(uint16_t, uint16_t) {}
  int fontHeight(uint8_t font) { return font == 1 ? 8 : 16; }
  void drawChar(uint16_t cp, int x, int y, uint8_t font) {
    char text[] = {(char)cp,0};
    regions.push_back({x,y,textWidth(text,font),fontHeight(font)});
  }
  int width() { return 240; }
} tft;
#include "ChineseFont.ino"
int main() {
  assert(Text_width(u8"i中M",2) == 4+16+12);
  Text_draw(14,11,u8"i中M",2,0xffff,0,82);
  assert(tft.regions.size() == 3);
  assert(tft.regions[0].x == 14 && tft.regions[1].x == 18 && tft.regions[2].x == 34);
  assert(tft.regions[1].w == 16 && tft.regions[1].h == 16);
  assert(!tft.swap);
  assert(std::count(tft.pixels.begin(),tft.pixels.end(),0xffff) > 0);
  for(int width = 0; width < 110; ++width) {
    tft.regions.clear();
    Text_draw(20,12,u8"超长中文测试名称iMABC",1,0xffff,0,width);
    for(const Region &region : tft.regions) {
      assert(region.x >= 20 && region.x + region.w <= 20 + width);
      if(region.w == 16) assert(region.h == 16);
    }
    String fitted = Text_fit(u8"超长中文测试名称iMABC",width,1);
    const char* cursor = fitted.c_str();
    while(*cursor) assert(TinyTV::nextCodepoint(cursor) != '?');
  }
  tft.regions.clear();
  ChineseFont_drawStringSmall(20,30,u8"中i",0xffff,0);
  assert(tft.regions[0].h == 16 && tft.regions[0].y == 30);
  assert(tft.regions[1].h == 8 && tft.regions[1].y == 34);
  tft.regions.clear();
  Text_draw(0,0,u8"😀中",2,0xffff,0,32);
  assert(tft.regions.size() == 2);
}
