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

bool pageRedrawRequested = true;
bool pageRefreshRequested = false;
AppPage renderedPage = PAGE_CLOCK;
bool pageHasRendered = false;
uint32_t pageIntentUs = 0;
void Page_requestRedraw() { pageRedrawRequested = true; }
void Page_next() {
  currentPage = (AppPage)((currentPage + 1) % pageCount);
  pageIntentUs = micros();
}
void Page_confirm() { pageRefreshRequested = true; }
void Page_home() {
  currentPage = PAGE_CLOCK; pageRedrawRequested = true; pageIntentUs = micros();
}
void Page_render(bool force) {
  bool entered = !pageHasRendered || renderedPage != currentPage;
  if(entered) {
    Network_cancel();
    Stock_stop();
    renderedPage = currentPage; pageHasRendered = true;
    if(currentPage == PAGE_STOCK) Stock_prepareGroup();
    if(currentPage == PAGE_WEATHER) Weather_enter();
    if(currentPage == PAGE_CODEX) Codex_enter();
  }
  force = force || entered || pageRedrawRequested;
  pageRedrawRequested = false;
  if(pageRefreshRequested) {
    if(currentPage == PAGE_CLOCK) { Ntp_request(); force = true; }
    else if(currentPage == PAGE_STOCK) Stock_requestRefresh();
    else if(currentPage == PAGE_WEATHER) getCityWeater();
    else if(currentPage == PAGE_CODEX) Codex_requestRefresh();
    else force = true;
    pageRefreshRequested = false;
  }
  if(force) {
    tft.setTextSize(1); tft.setTextWrap(false, false); tft.setTextPadding(0);
    tft.setTextDatum(TL_DATUM); tft.setTextColor(TinyTV::Theme::text, bgColor);
  }
  uint32_t started = micros();
  if(entered && pageIntentUs && started - pageIntentUs > perf.firstMaxUs) perf.firstMaxUs = started - pageIntentUs;
  switch(currentPage) {
    case PAGE_CLOCK: Page_renderClock(force); break;
    case PAGE_STOCK: Page_renderStock(force); break;
    case PAGE_WEATHER: Page_renderWeather(force); break;
    case PAGE_SETTINGS: Page_renderSettings(force); break;
    case PAGE_CODEX: Codex_renderPage(force); break;
  }
  pageIntentUs = 0;
  if(force && micros() - started > perf.pageMaxUs) perf.pageMaxUs = micros() - started;
}
void Data_poll() {
  if(currentPage == PAGE_STOCK) Stock_poll();
  else if(currentPage == PAGE_WEATHER) Weather_poll();
  else if(currentPage == PAGE_CODEX) Codex_poll();
}

void Page_renderClock(bool force) {
  if(force) {
    tft.fillScreen(bgColor);
    Theme_drawRule(71);
  }
  digitalClockDisplay(force);
#if DHT_EN
  if(force && DHT_img_flag) IndoorTem();
#endif
}

void Page_renderStock(bool force)
{
  Stock_renderPage(force);
}

void Page_renderWeather(bool force) { Weather_drawCached(force); }

void Page_renderSettings(bool force) {
  using namespace TinyTV::Theme;
  if(!force && millis() - pageRenderTime < 5000) return;
  pageRenderTime = millis();
  bool connected = WiFi.status() == WL_CONNECTED;
  String ip = connected ? WiFi.localIP().toString() : "--";
  static bool previousConnected = false;
  static String previousIp;
  if(force) {
    tft.fillScreen(background);
    Theme_drawHeader("Settings", nullptr);
    const char* labels[] = {"Wi-Fi", "IP", "Firmware"};
    for(uint8_t i = 0; i < 3; ++i) {
      int y = 54 + i * 48;
      tft.fillRect(12, y, 216, 40, panel);
      tft.setTextDatum(ML_DATUM); tft.setTextColor(muted, panel);
      tft.drawString(labels[i], 22, y + 20, 2);
    }
    tft.setTextDatum(MR_DATUM); tft.setTextColor(text, panel);
    tft.drawString(Version, 218, 170, 2);
    tft.setTextDatum(CC_DATUM); tft.setTextColor(muted, background);
    tft.drawString("http://sd3.local", 120, 216, 2);
  }
  if(force || previousConnected != connected) {
    tft.fillRect(104, 62, 114, 24, panel);
    tft.setTextDatum(MR_DATUM); tft.setTextColor(text, panel);
    const char* status = connected ? "Connected" : "Offline";
    tft.drawString(status, 218, 74, 2);
    tft.fillRect(206 - tft.textWidth(status, 2), 71, 6, 6, connected ? green : red);
  }
  if(force || previousIp != ip) {
    tft.fillRect(65, 110, 153, 24, panel);
    tft.setTextDatum(MR_DATUM); tft.setTextColor(connected ? text : muted, panel);
    tft.drawString(ip, 218, 122, 2);
  }
  previousConnected = connected;
  previousIp = ip;
}

void Home_show()
{
  Page_home();
}
