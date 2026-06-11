// Chinese 16x16 bitmap font renderer (GB2312, PROGMEM)
#include "ChineseFont.h"

static int chineseFont_findIndex(uint16_t unicode)
{
  int lo = 0, hi = CHINESE_FONT_CHAR_COUNT - 1;
  while(lo <= hi)
  {
    int mid = (lo + hi) >> 1;
    uint16_t cp = pgm_read_word(&chineseFontLookup[mid * 2]);
    if(cp == unicode) return pgm_read_word(&chineseFontLookup[mid * 2 + 1]);
    if(cp < unicode) lo = mid + 1;
    else hi = mid - 1;
  }
  return -1;
}

static uint16_t chineseFont_readRowBits(int offset, int row)
{
  int rowOffset = offset + row * 2;
  return ((uint16_t)pgm_read_byte(&chineseFontBitmaps[rowOffset]) << 8) |
    pgm_read_byte(&chineseFontBitmaps[rowOffset + 1]);
}

void ChineseFont_drawChar(int x, int y, uint16_t unicode, uint16_t fg, uint16_t bg)
{
  int idx = chineseFont_findIndex(unicode);
  if(idx < 0) return;

  int offset = idx * 32;
  tft.startWrite();
  tft.setAddrWindow(x, y, CHINESE_FONT_W, CHINESE_FONT_H);

  for(int row = 0; row < CHINESE_FONT_H; row++)
  {
    uint16_t bits = chineseFont_readRowBits(offset, row);
    for(int col = 0; col < CHINESE_FONT_W; col++)
      tft.pushColor(bits & (0x8000 >> col) ? fg : bg);
  }
  tft.endWrite();
}

void ChineseFont_drawCharSized(int x, int y, uint16_t unicode, uint8_t size, uint16_t fg, uint16_t bg)
{
  int idx = chineseFont_findIndex(unicode);
  if(idx < 0) return;

  int offset = idx * 32;
  tft.startWrite();
  tft.setAddrWindow(x, y, size, size);

  for(int row = 0; row < size; row++)
  {
    int srcRow = row * CHINESE_FONT_H / size;
    uint16_t bits = chineseFont_readRowBits(offset, srcRow);
    for(int col = 0; col < size; col++)
    {
      int srcCol = col * CHINESE_FONT_W / size;
      tft.pushColor(bits & (0x8000 >> srcCol) ? fg : bg);
    }
  }
  tft.endWrite();
}

// Draw mixed ASCII/Chinese string. ASCII uses font 2, CJK uses 16x16 bitmap.
// Position (x,y) is top-left, consistent with bitmap font.
void ChineseFont_drawString(int x, int y, const char* str, uint16_t fg, uint16_t bg)
{
  int sx = x;
  uint8_t b;
  while((b = (uint8_t)*str) != 0)
  {
    if(b < 0x80)
    {
      tft.drawChar(sx, y, b, fg, bg, 2);
      sx += tft.textWidth(String((char)b), 2);
      str++;
    }
    else if((b & 0xE0) == 0xC0 && str[1])
    {
      uint16_t cp = ((b & 0x1F) << 6) | ((uint8_t)str[1] & 0x3F);
      ChineseFont_drawChar(sx, y, cp, fg, bg);
      sx += CHINESE_FONT_W;
      str += 2;
    }
    else if((b & 0xF0) == 0xE0 && str[1] && str[2])
    {
      uint16_t cp = ((b & 0x0F) << 12) | (((uint8_t)str[1] & 0x3F) << 6) | ((uint8_t)str[2] & 0x3F);
      ChineseFont_drawChar(sx, y, cp, fg, bg);
      sx += CHINESE_FONT_W;
      str += 3;
    }
    else { str++; }
  }
}

// Smaller mixed string renderer for dense UI rows.
void ChineseFont_drawStringSmall(int x, int y, const char* str, uint16_t fg, uint16_t bg)
{
  const uint8_t chineseSize = 15;
  int sx = x;
  uint8_t b;
  while((b = (uint8_t)*str) != 0)
  {
    if(b < 0x80)
    {
      tft.drawChar(sx, y + 4, b, fg, bg, 1);
      sx += 6;
      str++;
    }
    else if((b & 0xE0) == 0xC0 && str[1])
    {
      uint16_t cp = ((b & 0x1F) << 6) | ((uint8_t)str[1] & 0x3F);
      ChineseFont_drawCharSized(sx, y, cp, chineseSize, fg, bg);
      sx += chineseSize;
      str += 2;
    }
    else if((b & 0xF0) == 0xE0 && str[1] && str[2])
    {
      uint16_t cp = ((b & 0x0F) << 12) | (((uint8_t)str[1] & 0x3F) << 6) | ((uint8_t)str[2] & 0x3F);
      ChineseFont_drawCharSized(sx, y, cp, chineseSize, fg, bg);
      sx += chineseSize;
      str += 3;
    }
    else { str++; }
  }
}
