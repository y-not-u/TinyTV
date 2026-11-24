#include "DisplayTheme.h"
#include "PixelDigits.h"

static void Clock_drawDateLabel(int centerX, int y, const String &text) {
  int width = Text_width(text.c_str(), 2);
  Text_draw(centerX - width / 2, y, text.c_str(), 2, TinyTV::Theme::muted, bgColor, 110);
}

static void Clock_drawDigit(int x, int y, int pitch, int radius, int digit, int previous, uint16_t color) {
  for(int row = 0; row < 7; ++row) {
    uint8_t dots = TinyTV::pixelDigits[digit][row];
    uint8_t changed = dots ^ (previous < 0 ? 0 : TinyTV::pixelDigits[previous][row]);
    for(int column = 0; column < 5; ++column) {
      uint8_t bit = 1 << (4 - column);
      if(changed & bit)
        tft.fillCircle(x + column * pitch, y + row * pitch, radius, dots & bit ? color : bgColor);
    }
  }
}

void digitalClockDisplay(bool force) {
  static int previousDigits[6] = {-1, -1, -1, -1, -1, -1};
  static int previousMonth = 0, previousDay = 0, previousWeekday = 0;
  // Use the same timestamp for every digit and both date fields.
  const time_t displayTime = now();
  const int hours = hour(displayTime), minutes = minute(displayTime), seconds = second(displayTime);
  const int digits[] = {hours / 10, hours % 10, minutes / 10, minutes % 10, seconds / 10, seconds % 10};
  const int digitX[] = {19, 69, 135, 185, 100, 124};

  for(int i = 0; i < 6; ++i) {
    if(!force && digits[i] == previousDigits[i]) continue;
    bool isSeconds = i >= 4;
    tft.startWrite();
    Clock_drawDigit(digitX[i], isSeconds ? 175 : 92, isSeconds ? 4 : 9, isSeconds ? 1 : 3,
                    digits[i], force ? -1 : previousDigits[i], isSeconds ? TinyTV::Theme::red : TinyTV::Theme::text);
    tft.endWrite();
    previousDigits[i] = digits[i];
  }

  if(force) {
    tft.fillCircle(120, 110, 3, TinyTV::Theme::red);
    tft.fillCircle(120, 128, 3, TinyTV::Theme::red);
  }

  const int dateMonth = month(displayTime), dateDay = day(displayTime), dateWeekday = weekday(displayTime);
  if(force || dateMonth != previousMonth || dateDay != previousDay || dateWeekday != previousWeekday) {
    tft.fillRect(0, 36, 240, 24, bgColor);
    Clock_drawDateLabel(69, 40, monthDay(displayTime));
    Clock_drawDateLabel(171, 40, week(displayTime));
    previousMonth = dateMonth;
    previousDay = dateDay;
    previousWeekday = dateWeekday;
  }
}

//星期
String week(time_t value)
{
  String wk[7] = {"星期日","星期一","星期二","星期三","星期四","星期五","星期六"};
  return wk[weekday(value)-1];
}

//月日
String monthDay(time_t value)
{
  return String(month(value)) + "月 " + String(day(value)) + "日";
}
