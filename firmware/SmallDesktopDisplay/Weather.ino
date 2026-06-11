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

void Weather_drawPill(int16_t x, int16_t y, int16_t w, const String &text, uint16_t fillColor, uint16_t textColor)
{
  tft.fillRoundRect(x, y, w, 22, 5, fillColor);
  Weather_drawStringCentered(x + w / 2, y + 4, text, textColor, fillColor, true);
}

String Weather_unitC(String value)
{
  value.trim();
  if(value.length() == 0 || value == "null")
    return "--C";

  return value + "C";
}

void Weather_drawDegreeC(int16_t x, int16_t y, uint16_t color)
{
  tft.drawCircle(x + 3, y + 4, 3, color);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(color, bgColor);
  tft.drawString("C", x + 10, y + 2, 2);
}

void Weather_drawTemperatureDigits(const String &value)
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
    tft.drawString("--", 146, 91, 2);
    Weather_drawDegreeC(170, 86, TFT_WHITE);
    return;
  }

  int16_t width = digitCount * 36 + (negative ? 18 : 0) + 28;
  int16_t x = 118 + (112 - width) / 2;
  if(x < 100)
    x = 100;

  if(negative)
  {
    tft.fillRoundRect(x, 104, 14, 4, 2, TFT_WHITE);
    x += 18;
  }

  for(uint8_t i = negative ? 1 : 0; i < text.length(); i++)
  {
    char c = text.charAt(i);
    if(!isDigit(c))
      continue;

    dig.printfW3660(x, 76, c - '0');
    x += 36;
  }

  Weather_drawDegreeC(x + 1, 82, TFT_WHITE);
}

void Weather_drawInfoLine(int16_t y, const String &label, const String &value)
{
  tft.drawFastHLine(18, y - 6, 204, TFT_DARKGREY);
  ChineseFont_drawStringSmall(20, y, label.c_str(), TFT_DARKGREY, bgColor);
  ChineseFont_drawStringSmall(64, y, value.c_str(), TFT_WHITE, bgColor);
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
  if(currentTemp.length() == 0 || currentTemp == "null")
    currentTemp = "--";
  String currentHumidity = sk["SD"].as<String>();
  currentHumidity.trim();
  if(currentHumidity.length() == 0 || currentHumidity == "null")
    currentHumidity = "--";
  String currentWeather = sk["weather"].as<String>();
  String windText = sk["WD"].as<String>() + sk["WS"].as<String>();
  int weatherCode = atoi((sk["weathercode"].as<String>()).substring(1,3).c_str());
  int pm25V = sk["aqi"];
  int currentTempValue = sk["temp"].as<int>();

  String aqiTxt = "优";
  uint16_t pm25BgColor = tft.color565(156,202,127);//优
  if(pm25V>200){
    pm25BgColor = tft.color565(136,11,32);//重度
    aqiTxt = "重度";
  }else if(pm25V>150){
    pm25BgColor = tft.color565(186,55,121);//中度
    aqiTxt = "中度";
  }else if(pm25V>100){
    pm25BgColor = tft.color565(242,159,57);//轻
    aqiTxt = "轻度";
  }else if(pm25V>50){
    pm25BgColor = tft.color565(247,219,100);//良
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

  tft.fillScreen(bgColor);
  
  Weather_drawStringCentered(120, 10, cityName, TFT_WHITE, bgColor, false);
  Weather_drawStringCentered(120, 35, currentWeather, TFT_WHITE, bgColor, true);
  Weather_drawPill(84, 56, 72, aqiTxt, pm25BgColor, TFT_BLACK);

  // Draw the weather icon at its native 60x60 size. The old 1.5x software
  // scaler made the JPEG icon look blocky on the physical screen.
  TJpgDec.setCallback(tft_output);
  wrat.printfweather(28, 76, weatherCode);
  Weather_drawTemperatureDigits(currentTemp);

  scrollText[0] = "空气质量 " + aqiTxt;
  scrollText[1] = "风向 " + windText;
  scrollText[2] = "今日 " + todayWeather;
  scrollText[3] = "温度 " + Weather_unitC(lowTemp) + "-" + Weather_unitC(highTemp);
  scrollText[4] = "最高温度 " + Weather_unitC(highTemp);
  scrollText[5] = "湿度 " + currentHumidity;

  Weather_drawInfoLine(152, "今日", todayWeather);
  Weather_drawInfoLine(176, "风力", windText);
  Weather_drawInfoLine(200, "温度", Weather_unitC(lowTemp) + "-" + Weather_unitC(highTemp));
  Weather_drawInfoLine(224, "湿度", currentHumidity);

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
