static int Clock_textWidthSized(const char* str, uint8_t chineseSize)
{
  int width = 0;
  uint8_t b;
  while((b = (uint8_t)*str) != 0)
  {
    if(b < 0x80)
    {
      width += (b == ' ') ? 6 : 12;
      str++;
    }
    else if((b & 0xE0) == 0xC0 && str[1])
    {
      width += chineseSize;
      str += 2;
    }
    else if((b & 0xF0) == 0xE0 && str[1] && str[2])
    {
      width += chineseSize;
      str += 3;
    }
    else { str++; }
  }
  return width;
}

static void Clock_drawStringSized(int x, int y, const char* str, uint8_t chineseSize, uint16_t fg, uint16_t bg)
{
  int sx = x;
  uint8_t b;
  while((b = (uint8_t)*str) != 0)
  {
    if(b < 0x80)
    {
      if(b == ' ') sx += 6;
      else
      {
        tft.drawChar(sx, y + 2, b, fg, bg, 2);
        sx += 12;
      }
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

static void Clock_drawDateLabel(int centerX, int y, const String &text)
{
  const uint8_t chineseSize = 20;
  int textWidth = Clock_textWidthSized(text.c_str(), chineseSize);
  Clock_drawStringSized(centerX - textWidth / 2, y, text.c_str(), chineseSize, TFT_WHITE, bgColor);
}

static uint8_t Clock_dateSign = 0;
static uint8_t Clock_weekdaySign = 0;

void digitalClockDisplay(int reflash_en)
{
  int timey=82;
  bool forceRefresh = (reflash_en == 1);
  if(hour()!=Hour_sign || reflash_en == 1)//时钟刷新
  {
    dig.printfW3660(20,timey,hour()/10);
    dig.printfW3660(60,timey,hour()%10);
    Hour_sign = hour();
  }
  if(minute()!=Minute_sign  || reflash_en == 1)//分钟刷新
  {
    dig.printfO3660(101,timey,minute()/10);
    dig.printfO3660(141,timey,minute()%10);
    Minute_sign = minute();
  }
  if(second()!=Second_sign  || reflash_en == 1)//分钟刷新
  {
    dig.printfW1830(182,timey+30,second()/10);
    dig.printfW1830(202,timey+30,second()%10);
    Second_sign = second();
  }

  if(reflash_en == 1) reflash_en = 0;
  /***日期****/
  if(forceRefresh || day() != Clock_dateSign || weekday() != Clock_weekdaySign)
  {
    tft.fillRect(0, 50, 210, 28, bgColor);
    if(forceRefresh) tft.fillRect(0, 148, 180, 26, bgColor);
    Clock_drawDateLabel(58, 54, monthDay());
    Clock_drawDateLabel(158, 54, week());
    Clock_dateSign = day();
    Clock_weekdaySign = weekday();
  }

  /***日期****/
}

//星期
String week()
{
  String wk[7] = {"星期日","星期一","星期二","星期三","星期四","星期五","星期六"};
  return wk[weekday()-1];
}

//月日
String monthDay()
{
  return String(month()) + "月 " + String(day()) + "日";
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP时间在消息的前48字节中
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  //Serial.println("Transmit NTP Request");
  for(uint8_t serverIndex=0; serverIndex < (sizeof(ntpServerNames) / sizeof(ntpServerNames[0])); serverIndex++)
  {
    WiFi.hostByName(ntpServerNames[serverIndex], ntpServerIP);
    if(ntpServerIP == INADDR_NONE)
      continue;
    sendNTPpacket(ntpServerIP);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500) {
      int size = Udp.parsePacket();
      if (size >= NTP_PACKET_SIZE) {
        Serial.print("Receive NTP Response: ");
        Serial.println(ntpServerNames[serverIndex]);
        Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
        unsigned long secsSince1900;
        // convert four bytes starting at location 40 to a long integer
        secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
        secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
        secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
        secsSince1900 |= (unsigned long)packetBuffer[43];
        //Serial.println(secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR);
        return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
      }
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // 无法获取时间时返回0
}

bool syncClock(uint8_t retryCount)
{
  for(uint8_t i=0; i<retryCount; i++)
  {
    time_t ntpTime = getNtpTime();
    if(ntpTime != 0)
    {
      setTime(ntpTime);
      Serial.print("Time synced: ");
      Serial.println(ntpTime);
      return true;
    }
    delay(300);
  }
  return false;
}

// 向NTP服务器发送请求
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
