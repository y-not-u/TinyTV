// Native 16x16 Chinese glyphs, one bounded RGB565 transfer per glyph.
static int chineseFont_findIndex(uint16_t unicode) {
  int lo = 0, hi = CHINESE_FONT_CHAR_COUNT - 1;
  while(lo <= hi) {
    int mid = (lo + hi) >> 1;
    uint16_t cp = pgm_read_word(&chineseFontLookup[mid * 2]);
    if(cp == unicode) return pgm_read_word(&chineseFontLookup[mid * 2 + 1]);
    if(cp < unicode) lo = mid + 1; else hi = mid - 1;
  }
  return -1;
}
void ChineseFont_drawChar(int x, int y, uint16_t unicode, uint16_t fg, uint16_t bg) {
  uint16_t pixels[16 * 16];
  int idx = chineseFont_findIndex(unicode);
  for(int row = 0; row < 16; ++row) {
    uint16_t bits = idx < 0 ? (row == 0 || row == 15 ? 0xffff : 0x8001) :
      ((uint16_t)pgm_read_byte(&chineseFontBitmaps[idx * 32 + row * 2]) << 8) |
      pgm_read_byte(&chineseFontBitmaps[idx * 32 + row * 2 + 1]);
    for(int col = 0; col < 16; ++col) pixels[row * 16 + col] = bits & (0x8000 >> col) ? fg : bg;
  }
  bool swap = tft.getSwapBytes();
  tft.setSwapBytes(true); // Glyph expansion produces native-endian RGB565.
  tft.pushImage(x, y, 16, 16, pixels);
  tft.setSwapBytes(swap);
}
int Text_glyphWidth(uint32_t cp, uint8_t font) {
  if(cp >= 128) return 16;
  char glyph[] = {(char)cp, 0};
  return tft.textWidth(glyph, font);
}
int Text_width(const char* text, uint8_t font) {
  int width = 0;
  while(*text) width += Text_glyphWidth(TinyTV::nextCodepoint(text), font);
  return width;
}
String Text_fit(const char* text, int width, uint8_t font) {
  if(width <= 0) return "";
  if(Text_width(text, font) <= width) return String(text);
  int dots = Text_width("...", font);
  size_t bytes = TinyTV::fittingPrefix(text, width >= dots ? width - dots : width,
    [font](uint32_t cp) { return Text_glyphWidth(cp, font); });
  String result;
  result.reserve(bytes + 3);
  result.concat(text, bytes);
  if(width >= dots) result += "...";
  return result;
}
void Text_draw(int x, int y, const char* text, uint8_t font, uint16_t fg, uint16_t bg, int width) {
  String fitted = Text_fit(text, width, font);
  const char* cursor = fitted.c_str();
  tft.setTextSize(1);
  tft.setTextWrap(false, false);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(fg, bg);
  int height = max(16, (int)tft.fontHeight(font));
  while(*cursor) {
    uint32_t cp = TinyTV::nextCodepoint(cursor);
    if(cp < 128) tft.drawChar((uint16_t)cp, x, y + (height - tft.fontHeight(font)) / 2, font);
    else ChineseFont_drawChar(x, y + (height - 16) / 2, cp <= 0xffff ? cp : 0xffff, fg, bg);
    x += Text_glyphWidth(cp, font);
  }
}
void ChineseFont_drawString(int x, int y, const char* text, uint16_t fg, uint16_t bg) {
  Text_draw(x, y, text, 2, fg, bg, tft.width() - x);
}
void ChineseFont_drawStringSmall(int x, int y, const char* text, uint16_t fg, uint16_t bg) {
  Text_draw(x, y, text, 1, fg, bg, tft.width() - x);
}
