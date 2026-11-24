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
static uint16_t ChineseFont_rowBits(int index, int row) {
  if(index < 0) return row == 0 || row == 15 ? 0xffff : 0x8001;
  int offset = index * 32 + row * 2;
  return (uint16_t)pgm_read_byte(&chineseFontBitmaps[offset]) << 8 |
         pgm_read_byte(&chineseFontBitmaps[offset + 1]);
}

void ChineseFont_drawChar(int x, int y, uint16_t unicode, uint16_t fg, uint16_t bg) {
  uint16_t pixels[16 * 16];
  int idx = chineseFont_findIndex(unicode);
  for(int row = 0; row < 16; ++row) {
    uint16_t bits = ChineseFont_rowBits(idx, row);
    for(int col = 0; col < 16; ++col) pixels[row * 16 + col] = bits & (0x8000 >> col) ? fg : bg;
  }
  bool swap = tft.getSwapBytes();
  tft.setSwapBytes(true); // Glyph expansion produces native-endian RGB565.
  tft.pushImage(x, y, 16, 16, pixels);
  tft.setSwapBytes(swap);
}
static void ChineseFont_drawCharDouble(int x, int y, uint16_t unicode, uint16_t fg, uint16_t bg) {
  uint16_t pixels[32 * 2];
  int idx = chineseFont_findIndex(unicode);
  bool swap = tft.getSwapBytes();
  tft.setSwapBytes(true);
  tft.startWrite();
  for(int row = 0; row < 16; ++row) {
    uint16_t bits = ChineseFont_rowBits(idx, row);
    for(int col = 0; col < 16; ++col) {
      uint16_t color = bits & (0x8000 >> col) ? fg : bg;
      pixels[col * 2] = pixels[col * 2 + 1] = color;
      pixels[32 + col * 2] = pixels[33 + col * 2] = color;
    }
    tft.pushImage(x, y + row * 2, 32, 2, pixels);
  }
  tft.endWrite();
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
static void Text_drawFitted(int x, int y, const char* cursor, uint8_t font, uint16_t fg, uint16_t bg, int scale) {
  tft.setTextSize(scale);
  tft.setTextWrap(false, false);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(fg, bg);
  int fontHeight = tft.fontHeight(font);
  int chineseHeight = 16 * scale;
  int height = max(chineseHeight, fontHeight);
  while(*cursor) {
    uint32_t cp = TinyTV::nextCodepoint(cursor);
    if(cp < 128) {
      tft.drawChar((uint16_t)cp, x, y + (height - fontHeight) / 2, font);
      x += Text_glyphWidth(cp, font);
    }
    else {
      int glyphY = y + (height - chineseHeight) / 2;
      uint16_t glyph = cp <= 0xffff ? cp : 0xffff;
      if(scale == 2) ChineseFont_drawCharDouble(x, glyphY, glyph, fg, bg);
      else ChineseFont_drawChar(x, glyphY, glyph, fg, bg);
      x += chineseHeight;
    }
  }
  tft.setTextSize(1);
}

void Text_draw(int x, int y, const char* text, uint8_t font, uint16_t fg, uint16_t bg, int width) {
  tft.setTextSize(1);
  String fitted = Text_fit(text, width, font);
  Text_drawFitted(x, y, fitted.c_str(), font, fg, bg, 1);
}

void Text_drawCenteredDouble(int centerX, int y, const char* text, uint16_t fg, uint16_t bg, int width) {
  tft.setTextSize(1);
  String fitted = Text_fit(text, width / 2, 2);
  Text_drawFitted(centerX - Text_width(fitted.c_str(), 2), y, fitted.c_str(), 2, fg, bg, 2);
}
void ChineseFont_drawString(int x, int y, const char* text, uint16_t fg, uint16_t bg) {
  Text_draw(x, y, text, 2, fg, bg, tft.width() - x);
}
void ChineseFont_drawStringSmall(int x, int y, const char* text, uint16_t fg, uint16_t bg) {
  Text_draw(x, y, text, 1, fg, bg, tft.width() - x);
}
