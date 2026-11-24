//TFT屏幕输出函数
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
  if ( y >= tft.height() ) return 0;
  tft.pushImage(x, y, w, h, bitmap);
  // Return 1 to decode next block
  return 1;
}

void Theme_drawRule(int y) {
  tft.drawFastHLine(12, y, 216, TinyTV::Theme::track);
  for(int x = 12; x <= 24; x += 6) tft.fillRect(x, y - 1, 4, 3, TinyTV::Theme::red);
}

void Theme_drawHeader(const char* title, const char* detail) {
  tft.fillRect(0, 0, 240, 43, TinyTV::Theme::background);
  tft.setTextSize(1);
  tft.setTextWrap(false, false);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TinyTV::Theme::text, TinyTV::Theme::background);
  tft.drawString(title, 12, 22, 4);
  if(detail && detail[0]) {
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(TinyTV::Theme::muted, TinyTV::Theme::background);
    tft.drawString(Text_fit(detail, 82, 2), 228, 24, 2);
  }
  Theme_drawRule(44);
}

static constexpr int Boot_segments = 14;

static void Boot_drawLabel(const char* label, int y, uint8_t font, uint16_t color) {
  tft.setTextColor(color, TinyTV::Theme::background);
  tft.drawString(label, 120, y, font);
}

static void Boot_drawHeader() {
  static const uint8_t wordmark[][7] = {
    {31, 4, 4, 4, 4, 4, 4},       // T
    {31, 4, 4, 4, 4, 4, 31},      // I
    {17, 25, 25, 21, 19, 19, 17}, // N
    {17, 17, 10, 4, 4, 4, 4},     // Y
    {31, 4, 4, 4, 4, 4, 4},       // T
    {17, 17, 17, 17, 10, 10, 4},  // V
  };
  tft.fillScreen(TinyTV::Theme::background);
  tft.startWrite();
  for(int letter = 0; letter < 6; ++letter) {
    for(int row = 0; row < 7; ++row) {
      for(int column = 0; column < 5; ++column) {
        if(wordmark[letter][row] & (16 >> column))
          tft.fillRect(50 + letter * 24 + column * 4, 67 + row * 4, 3, 3, TinyTV::Theme::text);
      }
    }
  }
  tft.endWrite();
  tft.setTextDatum(CC_DATUM);
  tft.setTextSize(1);
  tft.setTextWrap(false, false);
  Boot_drawLabel(Version, 202, 1, TinyTV::Theme::muted);
}

static void Boot_drawSegments(int first, int end, uint16_t color) {
  tft.startWrite();
  for(int segment = first; segment < end; ++segment)
    tft.fillRect(37 + segment * 12, 114, 9, 10, color);
  tft.endWrite();
}

void Boot_showConnecting(uint32_t elapsed) {
  static int filled = -1;
  if(filled < 0) {
    Boot_drawHeader();
    Boot_drawSegments(0, Boot_segments, TinyTV::Theme::track);
    Boot_drawLabel("Connecting to Wi-Fi", 151, 2, TinyTV::Theme::text);
    filled = 0;
  }
  // Reserve the final segment for an established connection.
  int target = min(Boot_segments - 1, (int)(elapsed * Boot_segments / Boot_wifiTimeoutMs));
  if(target > filled) {
    Boot_drawSegments(filled, target, TinyTV::Theme::red);
    filled = target;
  }
}

void Boot_showConnected(const char* address) {
  Boot_drawHeader();
  Boot_drawSegments(0, Boot_segments, TinyTV::Theme::red);
  Boot_drawLabel("Wi-Fi connected", 151, 2, TinyTV::Theme::text);
  if(address) Boot_drawLabel(address, 173, 2, TinyTV::Theme::muted);
}

void Boot_showSetup(const char* ssid) {
  Boot_drawHeader();
  Theme_drawRule(114);
  Boot_drawLabel("Wi-Fi setup", 141, 2, TinyTV::Theme::text);
  Boot_drawLabel(ssid, 164, 2, TinyTV::Theme::muted);
}

//湿度图标显示函数
void humidityWinAt(int x, int y) {
  tft.fillRect(x, y, 52, 6, bgColor);
  tft.drawRoundRect(x, y, 52, 6, 3, TFT_WHITE);
  tft.fillRoundRect(x + 1, y + 1, constrain(huminum / 2, 0, 50), 4, 2, humicol);
}

void humidityWin()
{
  humidityWinAt(45,222);
}

//温度图标显示函数
void tempWinAt(int x, int y) {
  tft.fillRect(x, y, 52, 6, bgColor);
  tft.drawRoundRect(x, y, 52, 6, 3, TFT_WHITE);
  tft.fillRoundRect(x + 1, y + 1, constrain(tempnum, 0, 50), 4, 2, tempcol);
}

void tempWin()
{
  tempWinAt(45,192);
}
