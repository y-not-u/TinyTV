const uint16_t WEATHER_HTTP_TIMEOUT_MS = 3500;

void Weather_applyRequestHeaders(HTTPClient &httpClient, const String &referer)
{
  httpClient.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/125.0.0.0 Safari/537.36");
  httpClient.addHeader("Referer", referer);
  httpClient.addHeader("Accept", "*/*");
  httpClient.addHeader("Accept-Language", "zh-CN,zh;q=0.9");
  httpClient.addHeader("Connection", "close");
}

int Weather_findObjectEnd(const String &payload, int objectStart)
{
  int depth = 0;
  bool inString = false;
  bool escaped = false;

  for(int i = objectStart; i < payload.length(); i++)
  {
    char c = payload.charAt(i);

    if(escaped)
    {
      escaped = false;
      continue;
    }

    if(c == '\\')
    {
      escaped = true;
      continue;
    }

    if(c == '"')
    {
      inString = !inString;
      continue;
    }

    if(inString)
      continue;

    if(c == '{')
      depth++;
    else if(c == '}')
    {
      depth--;
      if(depth == 0)
        return i;
    }
  }

  return -1;
}

bool Weather_extractJsonObject(const String &payload, const char* marker, String *json)
{
  int markerIndex = payload.indexOf(marker);
  if(markerIndex < 0)
    return false;

  int objectStart = payload.indexOf('{', markerIndex + strlen(marker));
  if(objectStart < 0)
    return false;

  int objectEnd = Weather_findObjectEnd(payload, objectStart);
  if(objectEnd <= objectStart)
    return false;

  *json = payload.substring(objectStart, objectEnd + 1);
  return true;
}

bool Weather_extractIndexData(const String &payload, String *cityDZ, String *dataSK, String *dataFC)
{
  if(!Weather_extractJsonObject(payload, "weatherinfo", cityDZ))
    return false;

  if(!Weather_extractJsonObject(payload, "var dataSK", dataSK))
    return false;

  if(!Weather_extractJsonObject(payload, "\"f\":[", dataFC))
    return false;

  return true;
}

void LCD_reflash(int en)
{
  if (now() != prevDisplay || en == 1) 
  {
    prevDisplay = now();
    if(currentPage == PAGE_CLOCK)
      digitalClockDisplay(en);
    prevTime=0;  
  }

  if(currentPage != PAGE_WEATHER)
    return;
  
  if(millis() - weaterTime > (60000*updateweater_time) || en == 1 || UpdateWeater_en != 0){ //10分钟更新一次天气
    if(Wifi_en == 0)
    {
      WiFi.forceSleepWake();//wifi on
      Serial.println("WIFI恢复......");
      Wifi_en = 1;
    }

    if(WiFi.status() == WL_CONNECTED)
    {
      // Serial.println("WIFI已连接");
      Weather_showStatus("Weather", "Loading weather...");
      getCityWeater();
      if(UpdateWeater_en != 0) UpdateWeater_en = 0;
      weaterTime = millis();
      #if !WebSever_EN
      WiFi.forceSleepBegin(); // Wifi Off
      Serial.println("WIFI休眠......");
      Wifi_en = 0;
      #endif
    }
  }
}

// 发送HTTP请求并且将服务器响应通过串口输出
void getCityCode(){
 String URL = "http://wgeo.weather.com.cn/ip/?_="+String(now());
  //创建 HTTPClient 对象
  HTTPClient httpClient;
 
  //配置请求地址。此处也可以不使用端口号和PATH而单纯的
  httpClient.begin(wificlient,URL); 
  httpClient.setTimeout(WEATHER_HTTP_TIMEOUT_MS);
  
  //设置请求头中的User-Agent
  Weather_applyRequestHeaders(httpClient, "http://www.weather.com.cn/");
 
  //启动连接并发送HTTP请求
  int httpCode = httpClient.GET();
  Serial.print("Send GET request to URL: ");
  Serial.println(URL);
  
  //如果服务器响应OK则从服务器获取响应体信息并通过串口输出
  if (httpCode == HTTP_CODE_OK) {
    String str;
    str.reserve(256);
    str = httpClient.getString();

    int aa = str.indexOf("id=");
    if(aa>-1)
    {
       //cityCode = str.substring(aa+4,aa+4+9).toInt();
       cityCode = str.substring(aa+4,aa+4+9);
       Serial.println(cityCode); 
       getCityWeater();
    }
    else
    {
      Serial.println("获取城市代码失败");  
    }
    
    
  } else {
    Serial.println("请求城市代码错误：");
    Serial.println(httpCode);
  }
 
  //关闭ESP8266与服务器连接
  httpClient.end();
}



// 获取城市天气
void getCityWeater(){
 //String URL = "http://d1.weather.com.cn/dingzhi/" + cityCode + ".html?_="+String(now());//新
 String URL = "http://d1.weather.com.cn/weather_index/" + cityCode + ".html?_="+String(now());//原来
  //创建 HTTPClient 对象
  HTTPClient httpClient;
  
  httpClient.begin(wificlient,URL); 
  httpClient.setTimeout(WEATHER_HTTP_TIMEOUT_MS);
  
  //设置请求头中的User-Agent
  Weather_applyRequestHeaders(httpClient, String("http://www.weather.com.cn/weather1d/") + cityCode + ".shtml");
 
  //启动连接并发送HTTP请求
  int httpCode = httpClient.GET();
  Serial.println("正在获取天气数据");
  Serial.println(URL);
  
  //如果服务器响应OK则从服务器获取响应体信息并通过串口输出
  if (httpCode == HTTP_CODE_OK) {

    String str;
    str.reserve(4096);
    str = httpClient.getString();
    String jsonCityDZ;
    String jsonDataSK;
    String jsonFC;

    bool parsed = Weather_extractIndexData(str, &jsonCityDZ, &jsonDataSK, &jsonFC);
    str = "";
    if(parsed && weaterData(&jsonCityDZ,&jsonDataSK,&jsonFC))
    {
      Serial.println("获取成功");
    }
    else
    {
      Serial.println("天气数据解析失败");
      if(currentPage == PAGE_WEATHER)
        Weather_showStatus("Update failed", "Parse failed");
    }
    
  } else {
    Serial.println("请求城市天气错误：");
    Serial.println(httpCode);
    if(currentPage == PAGE_WEATHER)
    {
      String message = "HTTP " + String(httpCode);
      Weather_showStatus("Update failed", message.c_str());
    }
  }
 
  //关闭ESP8266与服务器连接
  httpClient.end();
}


String scrollText[7];//天气信息存储

void Weather_reserveBuffers()
{
  for(uint8_t i = 0; i < 7; i++)
    scrollText[i].reserve(32);
}

String Weather_withUnit(String value, const char* unit)
{
  value.trim();
  if(value.length() == 0 || value == "null")
    return "--";

  return value + unit;
}

const uint8_t WEATHER_ICON_SCALE_NUM = 3;
const uint8_t WEATHER_ICON_SCALE_DEN = 2;
int16_t weatherIconX = 0;
int16_t weatherIconY = 0;

bool Weather_iconOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
  if(y >= tft.height())
    return 0;

  if(x >= tft.width() || y >= tft.height())
    return 1;

  int16_t sourceX = x - weatherIconX;
  int16_t sourceY = y - weatherIconY;
  uint16_t line[96];
  int16_t lineX = weatherIconX + (sourceX * WEATHER_ICON_SCALE_NUM) / WEATHER_ICON_SCALE_DEN;

  for(uint16_t row = 0; row < h; row++)
  {
    int16_t y0 = weatherIconY + ((sourceY + row) * WEATHER_ICON_SCALE_NUM) / WEATHER_ICON_SCALE_DEN;
    int16_t y1 = weatherIconY + ((sourceY + row + 1) * WEATHER_ICON_SCALE_NUM) / WEATHER_ICON_SCALE_DEN;
    uint16_t pixelH = (y1 > y0) ? (y1 - y0) : 1;
    uint16_t lineLen = 0;

    for(uint16_t col = 0; col < w; col++)
    {
      int16_t x0 = weatherIconX + ((sourceX + col) * WEATHER_ICON_SCALE_NUM) / WEATHER_ICON_SCALE_DEN;
      int16_t x1 = weatherIconX + ((sourceX + col + 1) * WEATHER_ICON_SCALE_NUM) / WEATHER_ICON_SCALE_DEN;
      uint16_t pixelW = (x1 > x0) ? (x1 - x0) : 1;
      uint16_t color = bitmap[row * w + col];
      while(pixelW-- && lineLen < (sizeof(line) / sizeof(line[0])))
        line[lineLen++] = color;
    }

    for(uint16_t repeat = 0; repeat < pixelH; repeat++)
      tft.pushImage(lineX, y0 + repeat, lineLen, 1, line);
  }

  return 1;
}

void Weather_drawIconScaled(int16_t x, int16_t y, int weatherCode)
{
  weatherIconX = x;
  weatherIconY = y;
  TJpgDec.setCallback(Weather_iconOutput);
  wrat.printfweather(x, y, weatherCode);
  TJpgDec.setCallback(tft_output);
}

void Weather_showStatus(const char* title, const char* message)
{
  tft.fillRect(10, 28, 220, 212, TFT_BLACK);
  tft.setTextWrap(false);
  tft.setTextDatum(CC_DATUM);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString(title, 120, 114, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(message, 120, 146, 2);
}

void Weather_drawDetails(const String details[], int detailCount)
{
  tft.fillRect(17, 104, 206, 82, bgColor);
  ChineseFont_drawString(20, 109, details[0].c_str(), TFT_WHITE, bgColor);
  for(int i=1; i<detailCount; i++)
    ChineseFont_drawString(20, 109 + i * 18, details[i].c_str(), TFT_WHITE, bgColor);
}

int16_t Weather_mixedTextWidth(const char* str, bool small)
{
  int16_t width = 0;
  uint8_t b;
  while((b = (uint8_t)*str) != 0)
  {
    if(b < 0x80)
    {
      width += small ? 6 : tft.textWidth(String((char)b), 2);
      str++;
    }
    else if((b & 0xE0) == 0xC0 && str[1])
    {
      width += small ? 15 : CHINESE_FONT_W;
      str += 2;
    }
    else if((b & 0xF0) == 0xE0 && str[1] && str[2])
    {
      width += small ? 15 : CHINESE_FONT_W;
      str += 3;
    }
    else
    {
      str++;
    }
  }
  return width;
}

void Weather_drawStringCentered(int16_t centerX, int16_t y, const String &text, uint16_t fg, uint16_t bg, bool small)
{
  int16_t x = centerX - Weather_mixedTextWidth(text.c_str(), small) / 2;
  if(x < 0)
    x = 0;

  if(small)
    ChineseFont_drawStringSmall(x, y, text.c_str(), fg, bg);
  else
    ChineseFont_drawString(x, y, text.c_str(), fg, bg);
}

int Weather_roundTemperature(const String &value)
{
  float temp = value.toFloat();
  if(temp >= 0)
    return (int)(temp + 0.5f);

  return (int)(temp - 0.5f);
}

String Weather_unitC(String value)
{
  value.trim();
  if(value.length() == 0 || value == "null")
    return "--C";

  return String(Weather_roundTemperature(value)) + "C";
}

String Weather_rangeC(String low, String high)
{
  return Weather_unitC(low) + " / " + Weather_unitC(high);
}

void Weather_parseSunTimes(String combined, String fallbackSunset, String *sunriseText, String *sunsetText)
{
  combined.trim();
  fallbackSunset.trim();

  int sep = combined.indexOf('|');
  if(sep < 0)
    sep = combined.indexOf('/');

  if(sep > 0)
  {
    *sunriseText = combined.substring(0, sep);
    *sunsetText = combined.substring(sep + 1);
  }
  else
  {
    *sunriseText = combined;
    *sunsetText = fallbackSunset;
  }

  sunriseText->trim();
  sunsetText->trim();
  if(sunriseText->length() == 0 || *sunriseText == "null")
    *sunriseText = "--:--";
  if(sunsetText->length() == 0 || *sunsetText == "null")
    *sunsetText = "--:--";
}

void Weather_drawDegreeC(int16_t x, int16_t y, uint16_t color)
{
  tft.drawCircle(x + 3, y + 4, 3, color);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(color, bgColor);
  tft.drawString("C", x + 10, y + 2, 2);
}

void Weather_drawLocationPin(int16_t x, int16_t y, uint16_t color)
{
  tft.fillCircle(x, y + 6, 6, color);
  tft.fillTriangle(x - 5, y + 10, x + 5, y + 10, x, y + 20, color);
  tft.fillCircle(x, y + 6, 2, bgColor);
}

void Weather_drawTopBar(const String &cityName)
{
  Weather_drawLocationPin(17, 12, TFT_WHITE);
  ChineseFont_drawString(32, 12, cityName.c_str(), TFT_WHITE, bgColor);

  char timeText[6];
  snprintf(timeText, sizeof(timeText), "%02d:%02d", hour(), minute());
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(TFT_WHITE, bgColor);
  tft.drawString(timeText, 224, 13, 2);
}

void Weather_drawTemperatureDigits(int16_t baseX, int16_t baseY, const String &value)
{
  String text = value;
  text.trim();
  if(text.length() == 0 || text == "null")
    text = "--";

  uint8_t digitCount = 0;
  bool negative = text.charAt(0) == '-';
  for(uint8_t i = negative ? 1 : 0; i < text.length(); i++)
  {
    if(isDigit(text.charAt(i)))
      digitCount++;
  }

  if(digitCount == 0)
  {
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, bgColor);
    tft.drawString("--", baseX + 22, baseY + 22, 2);
    Weather_drawDegreeC(baseX + 54, baseY + 8, TFT_WHITE);
    return;
  }

  int16_t x = baseX;

  if(negative)
  {
    tft.fillRoundRect(x, baseY + 28, 14, 4, 2, TFT_WHITE);
    x += 18;
  }

  for(uint8_t i = negative ? 1 : 0; i < text.length(); i++)
  {
    char c = text.charAt(i);
    if(!isDigit(c))
      continue;

    dig.printfW3660(x, baseY, c - '0');
    x += 36;
  }

  Weather_drawDegreeC(x + 1, baseY + 6, TFT_WHITE);
}

void Weather_drawSun(int16_t cx, int16_t cy, int16_t r, uint16_t color)
{
  for(uint8_t i = 0; i < 8; i++)
  {
    float angle = i * 0.785398f;
    int16_t x0 = cx + cos(angle) * (r + 4);
    int16_t y0 = cy + sin(angle) * (r + 4);
    int16_t x1 = cx + cos(angle) * (r + 12);
    int16_t y1 = cy + sin(angle) * (r + 12);
    tft.drawLine(x0, y0, x1, y1, color);
    tft.drawLine(x0 + 1, y0, x1 + 1, y1, color);
  }
  tft.fillCircle(cx, cy, r, color);
}

void Weather_drawCloud(int16_t x, int16_t y, uint16_t color)
{
  tft.fillCircle(x + 20, y + 27, 17, color);
  tft.fillCircle(x + 42, y + 19, 24, color);
  tft.fillCircle(x + 69, y + 28, 18, color);
  tft.fillRoundRect(x + 12, y + 27, 76, 24, 8, color);
}

void Weather_drawRainDrops(int16_t x, int16_t y, uint16_t color)
{
  for(uint8_t i = 0; i < 3; i++)
  {
    int16_t dx = x + i * 20;
    tft.drawLine(dx, y, dx - 5, y + 13, color);
    tft.drawLine(dx + 1, y, dx - 4, y + 13, color);
  }
}

void Weather_drawSnow(int16_t x, int16_t y, uint16_t color)
{
  for(uint8_t i = 0; i < 3; i++)
  {
    int16_t cx = x + i * 20;
    tft.drawFastHLine(cx - 5, y, 10, color);
    tft.drawFastVLine(cx, y - 5, 10, color);
    tft.drawLine(cx - 4, y - 4, cx + 4, y + 4, color);
    tft.drawLine(cx + 4, y - 4, cx - 4, y + 4, color);
  }
}

void Weather_drawMainIcon(int16_t x, int16_t y, int weatherCode)
{
  uint16_t sunColor = tft.color565(255, 196, 35);
  uint16_t cloudColor = tft.color565(238, 243, 248);
  uint16_t rainColor = tft.color565(70, 168, 255);
  uint16_t hazeColor = tft.color565(150, 162, 176);

  bool isSunny = weatherCode == 0;
  bool isRain = weatherCode == 3 || weatherCode == 4 || weatherCode == 5 ||
    weatherCode == 6 || weatherCode == 7 || weatherCode == 8 ||
    weatherCode == 9 || weatherCode == 10 || weatherCode == 11 ||
    weatherCode == 12 || weatherCode == 21 || weatherCode == 22 ||
    weatherCode == 23 || weatherCode == 24 || weatherCode == 25 ||
    weatherCode == 301 || weatherCode == 302;
  bool isSnow = weatherCode == 13 || weatherCode == 14 || weatherCode == 15 ||
    weatherCode == 16 || weatherCode == 17 || weatherCode == 26 ||
    weatherCode == 27 || weatherCode == 28;
  bool isHaze = weatherCode == 18 || weatherCode == 20 || weatherCode == 29 ||
    weatherCode == 30 || weatherCode == 31 || weatherCode == 53 ||
    weatherCode == 32 || weatherCode == 49 || weatherCode == 54 ||
    weatherCode == 55 || weatherCode == 56 || weatherCode == 57 ||
    weatherCode == 58;

  if(isSunny)
  {
    Weather_drawSun(x + 48, y + 43, 28, sunColor);
    return;
  }

  Weather_drawSun(x + 63, y + 31, 25, sunColor);
  Weather_drawCloud(x, y + 22, isHaze ? hazeColor : cloudColor);

  if(isRain)
    Weather_drawRainDrops(x + 32, y + 78, rainColor);
  else if(isSnow)
    Weather_drawSnow(x + 32, y + 82, TFT_WHITE);
  else if(isHaze)
  {
    tft.drawFastHLine(x + 16, y + 81, 70, hazeColor);
    tft.drawFastHLine(x + 26, y + 91, 55, hazeColor);
  }
}

void Weather_drawBottomIcon(int16_t centerX, int16_t y, uint8_t iconType)
{
  uint16_t muted = tft.color565(174, 190, 210);
  uint16_t blue = tft.color565(67, 169, 255);
  uint16_t yellow = tft.color565(255, 190, 42);

  if(iconType == 0)
  {
    tft.drawFastHLine(centerX - 14, y + 12, 27, muted);
    tft.drawFastHLine(centerX - 10, y + 19, 20, muted);
    tft.drawCircle(centerX + 14, y + 10, 5, muted);
    tft.drawCircle(centerX + 8, y + 19, 4, muted);
  }
  else if(iconType == 1)
  {
    tft.fillCircle(centerX, y + 14, 8, blue);
    tft.fillTriangle(centerX - 8, y + 13, centerX + 8, y + 13, centerX, y, blue);
  }
  else
  {
    tft.drawFastHLine(centerX - 15, y + 20, 30, yellow);
    tft.fillCircle(centerX, y + 20, 9, yellow);
    tft.fillRect(centerX - 11, y + 20, 22, 10, bgColor);
    for(uint8_t i = 0; i < 5; i++)
    {
      int16_t dx = (int16_t)i * 8 - 16;
      tft.drawLine(centerX + dx, y + 11, centerX + dx / 2, y + 16, yellow);
    }
  }
}

void Weather_drawMetricColumn(int16_t centerX, const String &value, const String &label, uint8_t iconType)
{
  Weather_drawBottomIcon(centerX, 173, iconType);
  Weather_drawStringCentered(centerX, 201, value, TFT_WHITE, bgColor, true);
  Weather_drawStringCentered(centerX, 222, label, tft.color565(150, 164, 184), bgColor, true);
}

//天气信息写到屏幕上
bool weaterData(String *cityDZ,String *dataSK,String *dataFC)
{
  //解析第一段JSON
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, *dataSK);
  if(error)
  {
    Serial.print("dataSK JSON错误: ");
    Serial.println(error.c_str());
    return false;
  }
  JsonObject sk = doc.as<JsonObject>();

  String cityName = sk["cityname"].as<String>();
  String currentTemp = sk["temp"].as<String>();
  currentTemp.trim();
  int currentTempValue = 0;
  if(currentTemp.length() == 0 || currentTemp == "null")
  {
    currentTemp = "--";
  }
  else
  {
    currentTempValue = Weather_roundTemperature(currentTemp);
    currentTemp = String(currentTempValue);
  }
  String currentHumidity = sk["SD"].as<String>();
  currentHumidity.trim();
  if(currentHumidity.length() == 0 || currentHumidity == "null")
    currentHumidity = "--";
  String currentWeather = sk["weather"].as<String>();
  String windText = sk["WD"].as<String>() + sk["WS"].as<String>();
  int weatherCode = atoi((sk["weathercode"].as<String>()).substring(1,3).c_str());
  int pm25V = sk["aqi"];

  String aqiTxt = "优";
  if(pm25V>200){
    aqiTxt = "重度";
  }else if(pm25V>150){
    aqiTxt = "中度";
  }else if(pm25V>100){
    aqiTxt = "轻度";
  }else if(pm25V>50){
    aqiTxt = "良";
  }

  error = deserializeJson(doc, *cityDZ);
  if(error)
  {
    Serial.print("cityDZ JSON错误: ");
    Serial.println(error.c_str());
    return false;
  }
  JsonObject dz = doc.as<JsonObject>();
  String todayWeather = dz["weather"].as<String>();

  error = deserializeJson(doc, *dataFC);
  if(error)
  {
    Serial.print("dataFC JSON错误: ");
    Serial.println(error.c_str());
    return false;
  }
  JsonObject fc = doc.as<JsonObject>();
  String lowTemp = fc["fd"].as<String>();
  String highTemp = fc["fc"].as<String>();
  lowTemp.trim();
  highTemp.trim();
  String sunriseText;
  String sunsetText;
  Weather_parseSunTimes(fc["fi"].as<String>(), fc["fj"].as<String>(), &sunriseText, &sunsetText);

  tft.fillScreen(bgColor);
  
  Weather_drawTopBar(cityName);
  Weather_drawTemperatureDigits(18, 58, currentTemp);
  Weather_drawStringCentered(50, 129, currentWeather, TFT_WHITE, bgColor, false);
  Weather_drawStringCentered(50, 153, Weather_rangeC(lowTemp, highTemp), tft.color565(160, 174, 196), bgColor, true);
  Weather_drawMainIcon(132, 61, weatherCode);

  scrollText[0] = "空气质量 " + aqiTxt;
  scrollText[1] = "风向 " + windText;
  scrollText[2] = "今日 " + todayWeather;
  scrollText[3] = "温度 " + Weather_unitC(lowTemp) + "-" + Weather_unitC(highTemp);
  scrollText[4] = "最高温度 " + Weather_unitC(highTemp);
  scrollText[5] = "湿度 " + currentHumidity;

  tft.drawFastHLine(18, 169, 204, tft.color565(72, 82, 96));
  tft.drawFastVLine(82, 184, 42, tft.color565(72, 82, 96));
  tft.drawFastVLine(158, 184, 42, tft.color565(72, 82, 96));

  Weather_drawMetricColumn(44, windText, "风力", 0);
  Weather_drawMetricColumn(120, currentHumidity, "湿度", 1);
  Weather_drawMetricColumn(196, sunriseText + "/" + sunsetText, "日出日落", 2);

  tempnum = currentTempValue;
  tempnum = tempnum+10;
  if(tempnum<10)
    tempcol=0x00FF;
  else if(tempnum<28)
    tempcol=0x0AFF;
  else if(tempnum<34)
    tempcol=0x0F0F;
  else if(tempnum<41)
    tempcol=0xFF0F;
  else if(tempnum<49)
    tempcol=0xF00F;
  else
  {
    tempcol=0xF00F;
    tempnum=50;
  }
  return true;
}

int currentIndex = 0;

void scrollBanner(){
    if(scrollText[currentIndex])
    {
      tft.fillRect(10, 45, 220, 18, bgColor);
      ChineseFont_drawString(14, 45, scrollText[currentIndex].c_str(), TFT_WHITE, bgColor);

      if(currentIndex>=5)
        currentIndex = 0;
      else
        currentIndex += 1;
    }
    prevTime = 1;
}
