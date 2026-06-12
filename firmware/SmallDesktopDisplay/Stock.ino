const uint16_t STOCK_HTTP_TIMEOUT_MS = 3500;
const unsigned long STOCK_REFRESH_MS = 3000;
const unsigned long STOCK_RENDER_CHECK_MS = 1000;
const char* STOCK_API_HOST = "http://stockdata.lan";
const int STOCK_VALUE_X = 100;
const int STOCK_VALUE_Y_OFFSET = 4;
const int STOCK_VALUE_WIDTH = 130;
const int STOCK_VALUE_HEIGHT = 48;
const int STOCK_VALUE_RIGHT = 122;
const uint8_t STOCK_VALUE_FONT = 4;
const uint8_t STOCK_PRICE_FONT = 2;
const int STOCK_PRICE_CENTER_Y = 10;
const int STOCK_PERCENT_CENTER_Y = 36;
const int STOCK_LABEL_X = 14;
const int STOCK_LABEL_WIDTH = 82;
const uint8_t STOCK_LABEL_CHINESE_SIZE = 18;
const uint8_t STOCK_LABEL_ASCII_SPACING = 8;

struct StockQuote
{
  char code[16];
  char symbol[16];
  char name[32];
  float price;
  float change;
  float changePercent;
  bool valid;
};

StockQuote stockQuotes[3];
char stockRenderedPrice[3][18];
char stockRenderedPercent[3][18];
bool stockRenderedValid[3];
unsigned long stockLastFetch = 0;
uint8_t stockLastGroup = 255;

void Stock_clearRenderedValues()
{
  for(uint8_t i = 0; i < 3; i++)
  {
    stockRenderedPrice[i][0] = '\0';
    stockRenderedPercent[i][0] = '\0';
    stockRenderedValid[i] = false;
  }
}

void Stock_init()
{
  for(uint8_t i = 0; i < 3; i++)
  {
    stockQuotes[i].code[0] = '\0';
    stockQuotes[i].symbol[0] = '\0';
    stockQuotes[i].name[0] = '\0';
    stockQuotes[i].price = 0;
    stockQuotes[i].change = 0;
    stockQuotes[i].changePercent = 0;
    stockQuotes[i].valid = false;
  }
  Stock_clearRenderedValues();
}

void Stock_invalidate()
{
  Stock_init();
  stockLastFetch = 0;
  stockLastGroup = 255;
}

String Stock_formatFloat(float value, uint8_t decimals)
{
  char buffer[18];
  dtostrf(value, 0, decimals, buffer);
  return String(buffer);
}

String Stock_formatPercent(float value)
{
  String text = Stock_formatFloat(value, 2);
  if(value > 0)
    text = "+" + text;
  text += "%";
  return text;
}

void Stock_drawStatus(const char* message)
{
  tft.fillScreen(TFT_BLACK);
  Page_drawHeader(pageNames[PAGE_STOCK]);
  tft.setTextDatum(CC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(message, 120, 118, 2);
}

void Stock_applyRequestHeaders(HTTPClient &httpClient)
{
  httpClient.setUserAgent("TinyTV/1.0 ESP8266");
  httpClient.addHeader("Accept", "application/json");
  httpClient.addHeader("Connection", "close");
}

String Stock_requestSymbol(const char* code)
{
  String symbol = String(code);
  symbol.trim();
  symbol.toUpperCase();

  // 000510 is both SZ stock Xinjinlu and SH index CSI A500.
  // The stock backend routes the bare code to the SZ stock, so force the index.
  if(symbol == "000510")
    symbol = "000510.SH";

  symbol.toLowerCase();
  return symbol;
}

bool Stock_fetchQuote(const char* code, uint8_t index)
{
  StockQuote *quote = &stockQuotes[index];
  quote->valid = false;
  strncpy(quote->code, code, sizeof(quote->code) - 1);
  quote->code[sizeof(quote->code) - 1] = '\0';

  if(code[0] == '\0')
    return false;

  String symbol = Stock_requestSymbol(code);
  String url = String(STOCK_API_HOST) + "/api/stocks/" + symbol + "/quote";
  HTTPClient httpClient;
  httpClient.begin(wificlient, url);
  httpClient.setTimeout(STOCK_HTTP_TIMEOUT_MS);
  Stock_applyRequestHeaders(httpClient);

  int httpCode = httpClient.GET();
  Serial.print("Stock quote: ");
  Serial.println(url);

  if(httpCode != HTTP_CODE_OK)
  {
    Serial.print("Stock HTTP error: ");
    Serial.println(httpCode);
    httpClient.end();
    return false;
  }

  String payload;
  payload.reserve(512);
  payload = httpClient.getString();
  httpClient.end();

  DynamicJsonDocument doc(768);
  DeserializationError error = deserializeJson(doc, payload);
  if(error)
  {
    Serial.print("Stock JSON error: ");
    Serial.println(error.c_str());
    return false;
  }

  JsonObject data = doc["data"];
  if(data.isNull())
    return false;

  String apiSymbol = data["symbol"] | code;
  apiSymbol.toCharArray(quote->symbol, sizeof(quote->symbol));
  String apiName = data["name"] | "";
  apiName.toCharArray(quote->name, sizeof(quote->name));
  quote->price = data["price"] | 0.0;
  quote->change = data["change"] | 0.0;
  quote->changePercent = data["change_percent"] | 0.0;
  quote->valid = true;
  return true;
}

void Stock_fetchQuotes(uint8_t stockGroup)
{
  for(uint8_t i = 0; i < 3; i++)
  {
    const char* code = getStockCode(stockGroup, i);
    Stock_fetchQuote(code, i);
    delay(10);
  }
  stockLastFetch = millis();
  stockLastGroup = stockGroup;
}

void Stock_drawHeaderRow()
{
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Code", 10, 14, 2);
  tft.setTextDatum(MR_DATUM);
  tft.drawString("Price", 158, 14, 2);
  tft.drawString("Chg%", 218, 14, 2);
  tft.drawFastHLine(8, 28, 224, TFT_DARKGREY);
}

uint16_t Stock_blockColor()
{
  return tft.color565(12, 18, 24);
}

bool Stock_isAsciiText(const char* text)
{
  while(*text)
  {
    if((uint8_t)*text >= 0x80)
      return false;
    text++;
  }
  return true;
}

int Stock_mixedLabelWidth(const char* text, bool largeAscii)
{
  int width = 0;
  while(*text)
  {
    uint8_t b = (uint8_t)*text;
    if(b < 0x80)
    {
      width += largeAscii ? STOCK_LABEL_ASCII_SPACING : 6;
      text++;
    }
    else if((b & 0xE0) == 0xC0 && text[1])
    {
      width += STOCK_LABEL_CHINESE_SIZE;
      text += 2;
    }
    else if((b & 0xF0) == 0xE0 && text[1] && text[2])
    {
      width += STOCK_LABEL_CHINESE_SIZE;
      text += 3;
    }
    else
    {
      text++;
    }
  }
  return width;
}

void Stock_drawMixedLabel(const char* text, int x, int y, bool largeAscii, uint16_t fg, uint16_t bg)
{
  int sx = x;
  while(*text)
  {
    uint8_t b = (uint8_t)*text;
    if(b < 0x80)
    {
      if(largeAscii)
      {
        tft.drawChar(sx, y + 3, b, fg, bg, 1);
        sx += STOCK_LABEL_ASCII_SPACING;
      }
      else
      {
        tft.drawChar(sx, y + 4, b, fg, bg, 1);
        sx += 6;
      }
      text++;
    }
    else if((b & 0xE0) == 0xC0 && text[1])
    {
      uint16_t cp = ((b & 0x1F) << 6) | ((uint8_t)text[1] & 0x3F);
      ChineseFont_drawCharSized(sx, y, cp, STOCK_LABEL_CHINESE_SIZE, fg, bg);
      sx += STOCK_LABEL_CHINESE_SIZE;
      text += 2;
    }
    else if((b & 0xF0) == 0xE0 && text[1] && text[2])
    {
      uint16_t cp = ((b & 0x0F) << 12) | (((uint8_t)text[1] & 0x3F) << 6) | ((uint8_t)text[2] & 0x3F);
      ChineseFont_drawCharSized(sx, y, cp, STOCK_LABEL_CHINESE_SIZE, fg, bg);
      sx += STOCK_LABEL_CHINESE_SIZE;
      text += 3;
    }
    else
    {
      text++;
    }
  }
}

void Stock_drawQuoteName(const char* text, int y, uint16_t fg, uint16_t bg)
{
  if(Stock_isAsciiText(text))
  {
    char label[24];
    strncpy(label, text, sizeof(label) - 1);
    label[sizeof(label) - 1] = '\0';
    while(label[0] != '\0' && tft.textWidth(label, 4) > STOCK_LABEL_WIDTH)
      label[strlen(label) - 1] = '\0';

    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(fg, bg);
    tft.drawString(label, STOCK_LABEL_X, y + 16, 4);
  }
  else
  {
    bool largeAscii = Stock_mixedLabelWidth(text, true) <= STOCK_VALUE_X - STOCK_LABEL_X;
    Stock_drawMixedLabel(text, STOCK_LABEL_X, y + 11, largeAscii, fg, bg);
  }
}

void Stock_drawQuoteValues(uint8_t index, bool force)
{
  StockQuote *quote = &stockQuotes[index];
  int y = 36 + index * 66;
  int valueY = y + STOCK_VALUE_Y_OFFSET;
  int valueRight = STOCK_VALUE_X + STOCK_VALUE_RIGHT;
  uint16_t blockColor = Stock_blockColor();
  uint16_t mutedColor = tft.color565(120, 128, 136);
  uint16_t priceColor = quote->changePercent < 0 ? tft.color565(102, 154, 118) : tft.color565(178, 112, 108);
  uint16_t percentColor = quote->changePercent < 0 ? TFT_GREEN : TFT_RED;
  String priceText = quote->valid ? Stock_formatFloat(quote->price, 2) : "";
  String percentText = quote->valid ? Stock_formatPercent(quote->changePercent) : "";

  if(!force &&
    stockRenderedValid[index] == quote->valid &&
    priceText == stockRenderedPrice[index] &&
    percentText == stockRenderedPercent[index])
    return;

  tft.fillRect(STOCK_VALUE_X, valueY, STOCK_VALUE_WIDTH, STOCK_VALUE_HEIGHT, blockColor);
  tft.setTextWrap(false);
  tft.setTextDatum(MR_DATUM);

  if(!quote->valid)
  {
    tft.setTextColor(mutedColor, blockColor);
    tft.drawString("No data", valueRight, valueY + STOCK_PERCENT_CENTER_Y, 2);
    stockRenderedPrice[index][0] = '\0';
    stockRenderedPercent[index][0] = '\0';
  }
  else
  {
    tft.setTextColor(priceColor, blockColor);
    tft.drawString(priceText, valueRight, valueY + STOCK_PRICE_CENTER_Y, STOCK_PRICE_FONT);
    tft.setTextColor(percentColor, blockColor);
    tft.drawString(percentText, valueRight, valueY + STOCK_PERCENT_CENTER_Y, STOCK_VALUE_FONT);
    priceText.toCharArray(stockRenderedPrice[index], sizeof(stockRenderedPrice[index]));
    percentText.toCharArray(stockRenderedPercent[index], sizeof(stockRenderedPercent[index]));
  }

  stockRenderedValid[index] = quote->valid;
}

void Stock_drawQuoteBlock(uint8_t index)
{
  StockQuote *quote = &stockQuotes[index];
  int y = 36 + index * 66;
  uint16_t blockColor = Stock_blockColor();
  uint16_t mutedColor = tft.color565(120, 128, 136);

  tft.fillRoundRect(6, y, 228, 56, 4, blockColor);
  tft.setTextWrap(false);

  tft.setTextDatum(ML_DATUM);
  Stock_drawQuoteName(quote->name[0] == '\0' ? (quote->symbol[0] == '\0' ? quote->code : quote->symbol) : quote->name, y, TFT_WHITE, blockColor);

  tft.setTextColor(mutedColor, blockColor);
  tft.drawString(quote->symbol[0] == '\0' ? "--" : quote->symbol, STOCK_LABEL_X, y + 39, 2);

  Stock_drawQuoteValues(index, true);
}

void Stock_renderPage(bool force)
{
  bool pageChanged = previousPage != currentPage;
  if(!force && !pageChanged && millis() - pageRenderTime < STOCK_RENDER_CHECK_MS)
    return;

  previousPage = currentPage;
  pageRenderTime = millis();

  uint8_t stockGroup = currentStockGroup();
  bool fullRedraw = force || pageChanged || stockLastGroup != stockGroup || stockLastFetch == 0;
  bool shouldFetch = fullRedraw || millis() - stockLastFetch > STOCK_REFRESH_MS;

  if(shouldFetch)
  {
    if(Wifi_en == 0)
    {
      WiFi.forceSleepWake();
      Wifi_en = 1;
    }

    if(WiFi.status() == WL_CONNECTED)
    {
      if(fullRedraw)
        Stock_drawStatus("Loading stocks...");
      Stock_fetchQuotes(stockGroup);
    }
    else
    {
      if(fullRedraw)
        Stock_drawStatus("Wi-Fi offline");
      return;
    }
  }

  if(fullRedraw)
  {
    tft.fillScreen(TFT_BLACK);
    Page_drawHeader(pageNames[PAGE_STOCK]);
    Stock_drawHeaderRow();
    for(uint8_t i = 0; i < 3; i++)
    {
      Stock_drawQuoteBlock(i);
    }
  }
  else if(shouldFetch)
  {
    for(uint8_t i = 0; i < 3; i++)
    {
      Stock_drawQuoteValues(i, false);
    }
  }
}
