void setBacklight(int value, bool saveToEeprom)
{
  LCD_BL_PWM = constrain(value, LCD_BL_MIN, LCD_BL_MAX);
  analogWrite(LCD_BL_PIN, 1023 - (LCD_BL_PWM*10));
  if(saveToEeprom)
  {
    EEPROM.write(BL_addr, LCD_BL_PWM);
    EEPROM.commit();
  }
}

#if TOUCH_EN
void Touch_init()
{
  #if TOUCH_AUTO_PIN
  static const uint8_t touchPins[] = {4, 12, 16}; // D2, D6, D0 from original firmware options.
  for(uint8_t i = 0; i < sizeof(touchPins); i++)
  {
    pinMode(touchPins[i], INPUT_PULLUP);
  }
  #else
  pinMode(TOUCH_PIN, INPUT_PULLUP);
  #endif
}

void Touch_poll()
{
  static bool lastRawPressed = false;
  static bool stablePressed = false;
  static uint8_t clickCount = 0;
  static bool initialized = false;
  static unsigned long lastChange = 0;
  static unsigned long pressStart = 0;
  static unsigned long lastRelease = 0;
  static int activePin = -1;
  #if TOUCH_AUTO_PIN
  static const uint8_t touchPins[] = {4, 12, 16};
  static uint8_t idleLevels[sizeof(touchPins)];
  #endif

  unsigned long nowMs = millis();
  bool rawPressed = false;

  #if TOUCH_AUTO_PIN
  if(!initialized)
  {
    for(uint8_t i = 0; i < sizeof(touchPins); i++)
      idleLevels[i] = digitalRead(touchPins[i]);
    initialized = true;
    return;
  }

  if(activePin >= 0)
  {
    uint8_t idx = 0;
    for(uint8_t i = 0; i < sizeof(touchPins); i++)
    {
      if(touchPins[i] == activePin)
      {
        idx = i;
        break;
      }
    }
    rawPressed = (digitalRead(activePin) != idleLevels[idx]);
    if(!rawPressed)
      activePin = -1;
  }
  else
  {
    for(uint8_t i = 0; i < sizeof(touchPins); i++)
    {
      if(digitalRead(touchPins[i]) != idleLevels[i])
      {
        activePin = touchPins[i];
        rawPressed = true;
        Serial.print("Touch pin GPIO");
        Serial.println(activePin);
        break;
      }
    }
  }
  #else
  rawPressed = (digitalRead(TOUCH_PIN) == LOW);
  #endif

  if(rawPressed != lastRawPressed)
  {
    lastRawPressed = rawPressed;
    lastChange = nowMs;
  }

  if((nowMs - lastChange) < TOUCH_DEBOUNCE_MS)
    return;

  if(rawPressed != stablePressed)
  {
    stablePressed = rawPressed;
    if(stablePressed)
    {
      pressStart = nowMs;
    }
    else
    {
      if((nowMs - pressStart) >= TOUCH_LONG_PRESS_MS)
      {
        clickCount = 0;
        Touch_onLongPress();
      }
      else
      {
        clickCount++;
        lastRelease = nowMs;
      }
    }
  }

  if(!stablePressed && clickCount > 0 && (nowMs - lastRelease > TOUCH_DOUBLE_CLICK_MS))
  {
    if(clickCount >= 2)
      Touch_onDoubleClick();
    else
      Touch_onSingleClick();
    clickCount = 0;
  }
}

void Touch_onSingleClick()
{
  Serial.println("Single click.");
  Page_next();
}

void Touch_onDoubleClick()
{
  Serial.println("Double click.");
  Page_confirm();
}

void Touch_onLongPress()
{
  Serial.println("LongClick");
  Page_home();
}
#endif

void Page_next()
{
  currentPage = (AppPage)((currentPage + 1) % pageCount);
  Page_render(true);
}

void Page_confirm()
{
  switch(currentPage)
  {
    case PAGE_CLOCK:
      Page_renderClock(true);
      break;
    case PAGE_STOCK:
      Page_renderStock(true);
      break;
    case PAGE_WEATHER:
      UpdateWeater_en = 1;
      Page_renderWeather(true);
      break;
    case PAGE_SETTINGS:
      Page_renderSettings(true);
      break;
  }
}

void Page_home()
{
  currentPage = PAGE_CLOCK;
  Page_render(true);
}

void Page_render(bool force)
{
  switch(currentPage)
  {
    case PAGE_CLOCK:
      Page_renderClock(force);
      break;
    case PAGE_STOCK:
      Page_renderStock(force);
      break;
    case PAGE_WEATHER:
      Page_renderWeather(force);
      break;
    case PAGE_SETTINGS:
      Page_renderSettings(force);
      break;
  }
}

void Page_drawHeader(const char* title)
{
  (void)title;
}

void Page_renderClock(bool force)
{
  if(force)
  {
    tft.fillScreen(TFT_BLACK);
    Page_drawHeader(pageNames[PAGE_CLOCK]);
    Hour_sign = 60;
    Minute_sign = 60;
    Second_sign = 60;
    prevDisplay = 0;
  }
  digitalClockDisplay(force ? 1 : 0);
#if imgAst_EN
  if(DHT_img_flag == 0)
    imgAnim();
#endif
}

void Page_renderStock(bool force)
{
  if(!force && millis() - pageRenderTime < 5000)
    return;
  pageRenderTime = millis();
  tft.fillScreen(TFT_BLACK);
  Page_drawHeader(pageNames[PAGE_STOCK]);

  clk.setColorDepth(8);
  clk.loadFont(ZdyLwFont_20);
  clk.createSprite(220, 150);
  clk.fillSprite(TFT_BLACK);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, TFT_BLACK);
  uint8_t stockGroup = currentStockGroup();
  clk.drawString(stockGroup == 0 ? "白天股票" : "晚上股票", 110, 24);
  clk.setTextColor(TFT_GREEN, TFT_BLACK);
  for(uint8_t i = 0; i < 3; i++)
  {
    String code = getStockCode(stockGroup, i);
    if(code.length() == 0)
      code = "--";
    clk.drawString(code, 110, 58 + (i * 28));
  }
  clk.setTextColor(TFT_DARKGREY, TFT_BLACK);
  clk.drawString(stockGroup == 0 ? "06:00-17:59" : "18:00-05:59", 110, 140);
  clk.pushSprite(10, 54);
  clk.deleteSprite();
  clk.unloadFont();
}

void Page_renderWeather(bool force)
{
  if(force)
  {
    tft.fillScreen(TFT_BLACK);
    Page_drawHeader(pageNames[PAGE_WEATHER]);
    UpdateWeater_en = 1;
    weaterTime = 0;
  }
  LCD_reflash(force ? 1 : 0);
}

void Page_renderSettings(bool force)
{
  if(!force && millis() - pageRenderTime < 5000)
    return;
  pageRenderTime = millis();
  tft.fillScreen(TFT_BLACK);
  Page_drawHeader(pageNames[PAGE_SETTINGS]);

  bool wifiConnected = WiFi.status() == WL_CONNECTED;
  String ipText = wifiConnected ? WiFi.localIP().toString() : "--";

  tft.setTextDatum(CC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Settings", 120, 28, 2);
  tft.drawFastHLine(22, 46, 196, TFT_DARKGREY);

  tft.drawRoundRect(10, 62, 220, 40, 6, TFT_DARKGREY);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Wi-Fi", 22, 82, 2);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(wifiConnected ? TFT_GREEN : TFT_RED, TFT_BLACK);
  tft.drawString(wifiConnected ? "Connected" : "Offline", 218, 82, 2);

  tft.drawRoundRect(10, 112, 220, 40, 6, TFT_DARKGREY);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("IP", 22, 132, 2);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(wifiConnected ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
  tft.drawString(ipText, 218, 132, 2);

  tft.drawRoundRect(10, 162, 220, 40, 6, TFT_DARKGREY);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Firmware", 22, 182, 2);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(Version, 218, 182, 2);

  tft.setTextDatum(CC_DATUM);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("http://sd3.local", 120, 222, 2);
}

void Home_show()
{
  Page_home();
}
