const unsigned long STOCK_REFRESH_MS = 3000;
const char* STOCK_API_HOST = "http://stockdata.lan";
const int STOCK_VALUE_X = 100;
const int STOCK_VALUE_Y_OFFSET = 4;
const int STOCK_VALUE_WIDTH = 130;
const int STOCK_VALUE_HEIGHT = 48;
const uint8_t STOCK_VALUE_FONT = 4;
const uint8_t STOCK_PRICE_FONT = 2;
const int STOCK_LABEL_X = 14;
const int STOCK_LABEL_WIDTH = 82;

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
char stockRenderedPrice[3][48];
char stockRenderedPercent[3][52];
bool stockRenderedValid[3];
unsigned long stockLastFetch = 0;
uint8_t stockLastGroup = 255;

bool stockDirty = true;
bool stockRefreshRequested = false;
bool stockFetching = false;
uint8_t stockNextIndex = 0;
char stockRenderedName[3][32] = {};
char stockRenderedSymbol[3][16] = {};
uint16_t stockRenderedColor[3] = {};

bool stockForceBlocks = true;

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
  Network_cancelOwner(1);
  Stock_init();
  stockLastFetch = 0;
  stockLastGroup = 255;
}

String Stock_formatFloat(float value, uint8_t decimals)
{
  char buffer[48];
  snprintf(buffer, sizeof(buffer), "%.*f", decimals, (double)value);
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

bool Stock_parseQuote(const char* code, uint8_t index, const char* payload) {
  StaticJsonDocument<JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(5)> filter;
  filter["data"]["symbol"] = true;
  filter["data"]["name"] = true;
  filter["data"]["price"] = true;
  filter["data"]["change"] = true;
  filter["data"]["change_percent"] = true;
  DynamicJsonDocument doc(768);
  DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
  JsonObject data = doc["data"];
  if(error || data.isNull() || !data["price"].is<float>() ||
    !data["change_percent"].is<float>()) return false;
  StockQuote next = {};
  strlcpy(next.code, code, sizeof(next.code));
  strlcpy(next.symbol, data["symbol"] | code, sizeof(next.symbol));
  TinyTV::copyUtf8(next.name, data["name"] | "", sizeof(next.name));
  next.price = data["price"];
  next.change = data["change"] | 0.0f;
  next.changePercent = data["change_percent"];
  if(!isfinite(next.price) || !isfinite(next.changePercent) || !isfinite(next.change)) return false;
  next.valid = true;
  stockQuotes[index] = next;
  return true;
}
void Stock_result(uint8_t index, const String &key, bool ok, const char* payload) {
  if(currentPage != PAGE_STOCK || index >= 3 || currentStockGroup() != stockLastGroup ||
    key != getStockCode(stockLastGroup, index)) return;
  if(ok && Stock_parseQuote(key.c_str(), index, payload)) stockDirty = true;
  stockNextIndex = index + 1;
  if(stockNextIndex >= 3) { stockFetching = false; stockLastFetch = millis(); }
}
void Stock_requestRefresh() { stockRefreshRequested = true; }
void Stock_stop() { stockFetching = false; }
void Stock_prepareGroup() {
  uint8_t group = currentStockGroup();
  if(group != stockLastGroup) {
    Network_cancel(); Stock_init(); stockLastGroup = group;
    for(uint8_t i = 0; i < 3; ++i) strlcpy(stockQuotes[i].code, getStockCode(group, i), sizeof(stockQuotes[i].code));
    stockDirty = true; stockForceBlocks = true; stockRefreshRequested = true; stockFetching = false;
  }
}
void Stock_poll() {
  if(currentPage != PAGE_STOCK) { stockFetching = false; return; }
  uint8_t group = currentStockGroup();
  Stock_prepareGroup();
  if(!stockFetching && (stockRefreshRequested || millis() - stockLastFetch >= STOCK_REFRESH_MS)) {
    if(stockLastFetch && millis() - stockLastFetch < 1000) return;
    stockRefreshRequested = false; stockFetching = true; stockNextIndex = 0;
  }
  if(!stockFetching || Network_busy()) return;
  if(WiFi.status() != WL_CONNECTED) { stockLastFetch = millis(); stockFetching = false; return; }
  const char* code = getStockCode(group, stockNextIndex);
  if(!code[0] || !Network_start(1, stockNextIndex, String(STOCK_API_HOST) + "/api/stocks/" + Stock_requestSymbol(code) + "/quote", code, "")) {
    Stock_result(stockNextIndex, code, false, "");
  }
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

void Stock_drawQuoteName(const char* text, int y, uint16_t fg, uint16_t bg) {
  uint8_t font = Stock_isAsciiText(text) ? 4 : 1;
  Text_draw(STOCK_LABEL_X, y + (font == 4 ? 4 : 11), text, font, fg, bg, STOCK_LABEL_WIDTH);
}

void Stock_drawQuoteValues(uint8_t index, bool force)
{
  StockQuote *quote = &stockQuotes[index];
  int y = 36 + index * 66;
  int valueY = y + STOCK_VALUE_Y_OFFSET;
  uint16_t blockColor = Stock_blockColor();
  uint16_t mutedColor = tft.color565(120, 128, 136);
  uint16_t priceColor = quote->changePercent < 0 ? tft.color565(102, 154, 118) : tft.color565(178, 112, 108);
  uint16_t percentColor = quote->changePercent < 0 ? TFT_GREEN : TFT_RED;
  String priceText = quote->valid ? Stock_formatFloat(quote->price, 2) : "";
  String percentText = quote->valid ? Stock_formatPercent(quote->changePercent) : "";

  if(!force &&
    stockRenderedValid[index] == quote->valid &&
    priceText == stockRenderedPrice[index] &&
    percentText == stockRenderedPercent[index] &&
    stockRenderedColor[index] == percentColor)
    return;

  Render_valueLine(quote->valid ? priceText : String(""), STOCK_VALUE_X, valueY, 20,
    STOCK_PRICE_FONT, priceColor, blockColor);
  Render_valueLine(quote->valid ? percentText : String("No data"), STOCK_VALUE_X, valueY + 20, 28,
    quote->valid ? STOCK_VALUE_FONT : 2, quote->valid ? percentColor : mutedColor, blockColor);
  priceText.toCharArray(stockRenderedPrice[index], sizeof(stockRenderedPrice[index]));
  percentText.toCharArray(stockRenderedPercent[index], sizeof(stockRenderedPercent[index]));

  stockRenderedValid[index] = quote->valid;
  stockRenderedColor[index] = percentColor;
}

void Stock_drawQuoteBlock(uint8_t index)
{
  StockQuote *quote = &stockQuotes[index];
  int y = 36 + index * 66;
  uint16_t blockColor = Stock_blockColor();
  uint16_t mutedColor = tft.color565(120, 128, 136);

  Render_clearCorners(6, y, 228, 56, TFT_BLACK);
  tft.fillRoundRect(6, y, 228, 56, 4, blockColor);
  tft.setTextWrap(false);

  tft.setTextDatum(ML_DATUM);
  Stock_drawQuoteName(quote->name[0] == '\0' ? (quote->symbol[0] == '\0' ? quote->code : quote->symbol) : quote->name, y, TFT_WHITE, blockColor);

  tft.setTextColor(mutedColor, blockColor);
  Text_draw(STOCK_LABEL_X, y + 31, quote->symbol[0] == '\0' ? quote->code : quote->symbol,
    2, mutedColor, blockColor, STOCK_LABEL_WIDTH);
  strlcpy(stockRenderedName[index], quote->name, sizeof(stockRenderedName[index]));
  strlcpy(stockRenderedSymbol[index], quote->symbol, sizeof(stockRenderedSymbol[index]));

  Stock_drawQuoteValues(index, true);
}

void Stock_renderPage(bool force) {
  if(!force && !stockDirty) return;
  if(force) {
    // Paint backgrounds around the cards; cards overwrite their own regions.
    tft.fillRect(0, 0, 240, 36, TFT_BLACK);
    tft.fillRect(0, 224, 240, 16, TFT_BLACK);
    tft.fillRect(0, 36, 6, 188, TFT_BLACK);
    tft.fillRect(234, 36, 6, 188, TFT_BLACK);
    for(uint8_t i = 0; i < 3; ++i) tft.fillRect(6, 92 + i * 66, 228, 10, TFT_BLACK);
    Stock_drawHeaderRow();
  }
  for(uint8_t i = 0; i < 3; ++i) {
    if(force || stockForceBlocks || strcmp(stockRenderedName[i], stockQuotes[i].name) ||
      strcmp(stockRenderedSymbol[i], stockQuotes[i].symbol)) Stock_drawQuoteBlock(i);
    else Stock_drawQuoteValues(i, false);
  }
  stockDirty = false; stockForceBlocks = false;
}

uint8_t Stock_validCount() {
  uint8_t count = 0; for(uint8_t i = 0; i < 3; ++i) if(stockQuotes[i].valid) ++count;
  return count;
}
