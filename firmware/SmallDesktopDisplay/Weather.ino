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
  
  //两秒钟更新一次
  if(second()%2 ==0&& prevTime == 0 || en == 1){
#if DHT_EN
    if(DHT_img_flag != 0)
    IndoorTem();
#endif
  }


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
    String str = httpClient.getString();
    
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

    String str = httpClient.getString();
    String jsonCityDZ;
    String jsonDataSK;
    String jsonFC;

    if(Weather_extractIndexData(str, &jsonCityDZ, &jsonDataSK, &jsonFC) && weaterData(&jsonCityDZ,&jsonDataSK,&jsonFC))
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

String Weather_withUnit(String value, const char* unit)
{
  value.trim();
  if(value.length() == 0 || value == "null")
    return "--";

  return value + unit;
}

void Weather_showStatus(const char* title, const char* message)
{
  clk.setColorDepth(8);
  clk.createSprite(220, 212);
  clk.fillSprite(TFT_BLACK);
  clk.setTextWrap(false);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_GREEN, TFT_BLACK);
  clk.drawString(title, 110, 86, 2);
  clk.setTextColor(TFT_WHITE, TFT_BLACK);
  clk.drawString(message, 110, 118, 2);
  clk.pushSprite(10, 28);
  clk.deleteSprite();
}

void Weather_drawDetails(const String details[], int detailCount)
{
  clk.setColorDepth(8);
  clk.loadFont(ZdyLwFont_20);

  clk.createSprite(150, 104);
  clk.fillSprite(bgColor);
  clk.setTextWrap(false);
  clk.setTextDatum(ML_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  for(int i=0; i<detailCount; i++)
  {
    clk.drawString(details[i], 0, 12 + i * 20);
  }
  clk.pushSprite(14, 64);
  clk.deleteSprite();
  clk.unloadFont();
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
  String currentTemp = Weather_withUnit(sk["temp"].as<String>(), "℃");
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
  String lowTemp = Weather_withUnit(fc["fd"].as<String>(), "℃");
  String highTemp = Weather_withUnit(fc["fc"].as<String>(), "℃");

  tft.fillScreen(bgColor);
  
  /***绘制相关文字***/
  clk.setColorDepth(8);
  clk.loadFont(ZdyLwFont_20);

  //城市名称
  clk.createSprite(86, 30);
  clk.fillSprite(bgColor);
  clk.setTextDatum(ML_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawString(cityName, 0, 16);
  clk.pushSprite(14, 16);
  clk.deleteSprite();

  //空气指数
  clk.createSprite(58, 26);
  clk.fillSprite(bgColor);
  clk.fillRoundRect(2,1,54,24,4,pm25BgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(0x0000);
  clk.drawString(aqiTxt,29,14);
  clk.pushSprite(102, 17);
  clk.deleteSprite();

  //天气图标
  wrat.printfweather(170,15,weatherCode);

  scrollText[0] = "实时天气 " + currentWeather;
  scrollText[1] = "空气质量 " + aqiTxt;
  scrollText[2] = "风向 " + windText;
  scrollText[3] = "今日" + todayWeather;
  scrollText[4] = "最低温度 " + lowTemp;
  scrollText[5] = "最高温度 " + highTemp;
  Weather_drawDetails(scrollText, 6);
  clk.loadFont(ZdyLwFont_20);

  //温度
  TJpgDec.drawJpg(15,183,temperature, sizeof(temperature));
  clk.createSprite(58, 24);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawString(currentTemp,28,13);
  clk.pushSprite(100,184);
  clk.deleteSprite();
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
  tempWin();
  
  //湿度
  TJpgDec.drawJpg(15,213,humidity, sizeof(humidity));
  clk.createSprite(58, 24);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawString(currentHumidity,28,13);
  clk.pushSprite(100,214);
  clk.deleteSprite();
  huminum = atoi(currentHumidity.substring(0,2).c_str());
  
  if(huminum>90)
    humicol=0x00FF;
  else if(huminum>70)
    humicol=0x0AFF;
  else if(huminum>40)
    humicol=0x0F0F;
  else if(huminum>20)
    humicol=0xFF0F;
  else
    humicol=0xF00F;
  humidityWin();

  clk.unloadFont();
  return true;
}

int currentIndex = 0;
TFT_eSprite clkb = TFT_eSprite(&tft);

void scrollBanner(){
  //if(millis() - prevTime > 2333) //3秒切换一次
//  if(second()%2 ==0&& prevTime == 0)
//  { 
    if(scrollText[currentIndex])
    {
      clkb.setColorDepth(8);
      clkb.loadFont(ZdyLwFont_20);
      clkb.createSprite(150, 30); 
      clkb.fillSprite(bgColor);
      clkb.setTextWrap(false);
      clkb.setTextDatum(CC_DATUM);
      clkb.setTextColor(TFT_WHITE, bgColor); 
      clkb.drawString(scrollText[currentIndex],74, 16);
      clkb.pushSprite(10,45);
       
      clkb.deleteSprite();
      clkb.unloadFont();
      
      if(currentIndex>=5)
        currentIndex = 0;  //回第一个
      else
        currentIndex += 1;  //准备切换到下一个        
    }
    prevTime = 1;
//  }
}
