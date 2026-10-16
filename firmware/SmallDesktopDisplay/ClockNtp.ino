static void Clock_drawDateLabel(int centerX, int y, const String &text) {
  int width = Text_width(text.c_str(), 2);
  Text_draw(centerX - width / 2, y, text.c_str(), 2, TFT_WHITE, bgColor, 110);
}

static uint8_t Clock_dateSign = 0;
static uint8_t Clock_weekdaySign = 0;

void digitalClockDisplay(int reflash_en)
{
  // JPEG decoding can cross a second/minute boundary. Keep this draw coherent.
  const time_t displayTime = now();
  int timey=82;
  bool forceRefresh = (reflash_en == 1);
  if(hour(displayTime)!=Hour_sign || reflash_en == 1)//时钟刷新
  {
    dig.printfW3660(20,timey,hour(displayTime)/10);
    dig.printfW3660(60,timey,hour(displayTime)%10);
    Hour_sign = hour(displayTime);
  }
  if(minute(displayTime)!=Minute_sign  || reflash_en == 1)//分钟刷新
  {
    dig.printfO3660(101,timey,minute(displayTime)/10);
    dig.printfO3660(141,timey,minute(displayTime)%10);
    Minute_sign = minute(displayTime);
  }
  if(second(displayTime)!=Second_sign  || reflash_en == 1)//分钟刷新
  {
    dig.printfW1830(182,timey+30,second(displayTime)/10);
    dig.printfW1830(202,timey+30,second(displayTime)%10);
    Second_sign = second(displayTime);
  }

  if(reflash_en == 1) reflash_en = 0;
  /***日期****/
  if(forceRefresh || day(displayTime) != Clock_dateSign || weekday(displayTime) != Clock_weekdaySign)
  {
    tft.fillRect(0, 50, 210, 28, bgColor);
    Clock_drawDateLabel(58, 54, monthDay(displayTime));
    Clock_drawDateLabel(158, 54, week(displayTime));
    Clock_dateSign = day(displayTime);
    Clock_weekdaySign = weekday(displayTime);
  }

  /***日期****/
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
