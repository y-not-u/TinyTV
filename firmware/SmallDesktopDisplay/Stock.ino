const uint16_t STOCK_HTTP_TIMEOUT_MS = 3500;
const unsigned long STOCK_REFRESH_MS = 3000;
const unsigned long STOCK_RENDER_CHECK_MS = 1000;
const char* STOCK_API_HOST = "http://stockdata.lan";

struct StockQuote
{
  char code[16];
  char symbol[16];
  float price;
  float change;
  float changePercent;
  bool valid;
};

StockQuote stockQuotes[3];
unsigned long stockLastFetch = 0;
uint8_t stockLastGroup = 255;

void Stock_init()
{
  for(uint8_t i = 0; i < 3; i++)
  {
    stockQuotes[i].code[0] = '\0';
    stockQuotes[i].symbol[0] = '\0';
    stockQuotes[i].price = 0;
    stockQuotes[i].change = 0;
    stockQuotes[i].changePercent = 0;
    stockQuotes[i].valid = false;
  }
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
  tft.drawString("名称代码", 10, 14, 2);
  tft.setTextDatum(MR_DATUM);
  tft.drawString("最新价", 156, 14, 2);
  tft.drawString("涨跌幅", 230, 14, 2);
  tft.drawFastHLine(8, 28, 224, TFT_DARKGREY);
}

uint16_t Stock_blockColor()
{
  return tft.color565(12, 18, 24);
}

void Stock_drawQuoteValues(uint8_t index)
{
  StockQuote *quote = &stockQuotes[index];
  int y = 36 + index * 66;
  uint16_t blockColor = Stock_blockColor();
  uint16_t mutedColor = tft.color565(120, 128, 136);
  uint16_t valueColor = quote->changePercent < 0 ? TFT_GREEN : TFT_RED;

  tft.fillRect(92, y + 8, 136, 40, blockColor);
  tft.setTextWrap(false);
  tft.setTextDatum(MR_DATUM);

  if(!quote->valid)
  {
    tft.setTextColor(mutedColor, blockColor);
    tft.drawString("No data", 224, y + 28, 2);
    return;
  }

  tft.setTextColor(valueColor, blockColor);
  tft.drawString(Stock_formatFloat(quote->price, 2), 158, y + 28, 2);
  tft.drawString(Stock_formatPercent(quote->changePercent), 226, y + 28, 2);
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
  tft.setTextColor(TFT_WHITE, blockColor);
  tft.drawString(quote->symbol[0] == '\0' ? quote->code : quote->symbol, 14, y + 17, 2);

  tft.setTextColor(mutedColor, blockColor);
  tft.drawString(quote->code[0] == '\0' ? "--" : quote->code, 14, y + 39, 2);

  Stock_drawQuoteValues(index);
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
      Stock_drawQuoteValues(i);
    }
  }
}
