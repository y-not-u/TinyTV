// Quota percentages, not request counts: the API does not expose call totals.
const char* CODEX_QUOTAS_URL = "http://aihub.lan/api/usage/quotas";
const unsigned long CODEX_REFRESH_MS = 60000;
const uint8_t CODEX_MAX_WINDOWS = 2;

struct CodexWindow {
  char name[20];
  float used;
  char resetsAt[32];
};

CodexWindow codexWindows[CODEX_MAX_WINDOWS];
uint8_t codexWindowCount = 0;
char codexPlan[16] = "--";
char codexReadAt[40] = "";
const char* codexStatus = "Not loaded";
bool codexHasData = false;
bool codexAttempted = false;
unsigned long codexLastAttempt = 0;

// API timestamps are UTC; use the same UTC+8 setting as the clock page.
String Codex_localTime(const char* timestamp) {
  int yearValue, monthValue, dayValue, hourValue, minuteValue, secondValue;
  if(sscanf(timestamp, "%d-%d-%dT%d:%d:%d", &yearValue, &monthValue,
      &dayValue, &hourValue, &minuteValue, &secondValue) != 6 ||
      yearValue < 1970 || yearValue > 2100 || monthValue < 1 || monthValue > 12 ||
      dayValue < 1 || dayValue > 31 || hourValue < 0 || hourValue > 23 ||
      minuteValue < 0 || minuteValue > 59 || secondValue < 0 || secondValue > 59)
    return "--";

  tmElements_t parts = {};
  parts.Year = CalendarYrToTm(yearValue);
  parts.Month = monthValue;
  parts.Day = dayValue;
  parts.Hour = hourValue;
  parts.Minute = minuteValue;
  parts.Second = secondValue;
  time_t local = makeTime(parts) + timeZone * SECS_PER_HOUR;
  char text[20];
  snprintf(text, sizeof(text), "%02d/%02d %02d:%02d", month(local), day(local), hour(local), minute(local));
  return String(text);
}

bool codexDirty = true, codexRequested = false;
bool Codex_parse(const char* payload) {
  // Ignore account details, balance histories and all unrelated fields.
  // Filter the bounded response into a small visible snapshot.
  StaticJsonDocument<512> filter;
  filter[0]["provider"] = true;
  filter[0]["plan"] = true;
  filter[0]["readAt"] = true;
  filter[0]["windows"][0]["name"] = true;
  filter[0]["windows"][0]["used"] = true;
  filter[0]["windows"][0]["resetsAt"] = true;
  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
  if(error || !doc.is<JsonArray>()) {
    Serial.printf("Codex JSON error: %s\n", error.c_str());
    codexStatus = "Invalid API data";
    return false;
  }

  for(JsonObject provider : doc.as<JsonArray>()) {
    if(strcmp(provider["provider"] | "", "codex") != 0)
      continue;

    JsonArray windows = provider["windows"].as<JsonArray>();
    if(windows.isNull()) {
      codexStatus = "Missing quota data";
      return false;
    }
    // Validate the entire visible snapshot before replacing cached data.
    CodexWindow nextWindows[CODEX_MAX_WINDOWS] = {};
    uint8_t count = 0;
    for(JsonObject window : windows) {
      if(count >= CODEX_MAX_WINDOWS)
        break;
      if(!window["used"].is<float>() || !window["name"].is<const char*>()) {
        codexStatus = "Invalid quota data";
        return false;
      }
      float used = window["used"].as<float>();
      if(!isfinite(used) || used < 0 || used > 100) {
        codexStatus = "Invalid quota data";
        return false;
      }
      TinyTV::copyUtf8(nextWindows[count].name, window["name"], sizeof(nextWindows[count].name));
      nextWindows[count].used = used;
      strlcpy(nextWindows[count].resetsAt, window["resetsAt"] | "", sizeof(nextWindows[count].resetsAt));
      count++;
    }
    memcpy(codexWindows, nextWindows, sizeof(codexWindows));
    codexWindowCount = count;
    strlcpy(codexPlan, provider["plan"] | "--", sizeof(codexPlan));
    strlcpy(codexReadAt, provider["readAt"] | "", sizeof(codexReadAt));
    codexHasData = true;
    codexStatus = count == 0 ? "No quota windows" : "";
    Serial.printf("Codex quotas updated: %u windows\n", count);
    return true;
  }
  codexStatus = "Codex not found";
  return false;
}

CodexWindow codexRenderedWindows[CODEX_MAX_WINDOWS] = {};
char codexRenderedPlan[16] = "";
String codexRenderedFooter;
uint8_t codexRenderedCount = 0;
void Codex_drawPage(bool force) {
  uint16_t muted = tft.color565(136, 148, 160);
  uint16_t panel = tft.color565(12, 18, 24);
  if(force) {
    tft.fillRect(0, 44, 240, 10, TFT_BLACK);
    tft.fillRect(0, 54, 8, 144, TFT_BLACK);
    tft.fillRect(232, 54, 8, 144, TFT_BLACK);
    tft.fillRect(8, 120, 224, 6, TFT_BLACK);
    tft.fillRect(8, 192, 224, 6, TFT_BLACK);
    tft.fillRect(0, 198, 240, 42, TFT_BLACK);
  }
  if(force || strcmp(codexRenderedPlan, codexPlan)) {
    tft.fillRect(0, 0, 240, 44, TFT_BLACK);
    tft.setTextWrap(false);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Codex", 12, 22, 4);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(muted, TFT_BLACK);
    tft.drawString(codexPlan, 228, 24, 2);
    tft.drawFastHLine(12, 44, 216, TFT_DARKGREY);

    strlcpy(codexRenderedPlan, codexPlan, sizeof(codexRenderedPlan));
  }
  if(!codexHasData || codexWindowCount == 0) {
    tft.fillRect(8, 54, 224, 144, TFT_BLACK);
    tft.setTextDatum(CC_DATUM);
    tft.drawString(codexStatus, 120, 112, 2);
    tft.drawString("Double tap to retry", 120, 142, 2);
  }
  for(uint8_t i = 0; i < codexWindowCount; i++) {
    int y = 54 + i * 72;
    CodexWindow &window = codexWindows[i];
    if(!force && i < codexRenderedCount && !memcmp(&window, &codexRenderedWindows[i], sizeof(window))) continue;
    uint16_t accent = window.used >= 90 ? TFT_RED :
      (window.used >= 70 ? TFT_ORANGE : tft.color565(93, 219, 166));
    Render_clearCorners(8, y, 224, 66, TFT_BLACK);
    tft.fillRoundRect(8, y, 224, 66, 4, panel);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(TFT_WHITE, panel);
    Text_draw(16, y + 8, window.name, 2, TFT_WHITE, panel, 100);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(accent, panel);
    tft.drawString(String(window.used, 1) + "% used", 224, y + 16, 2);
    tft.fillRoundRect(16, y + 31, 208, 6, 3, TFT_DARKGREY);
    int width = (int)(208 * window.used / 100.0f);
    if(width > 0)
      tft.fillRect(16, y + 31, width, 6, accent);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(muted, panel);
    tft.drawString("Reset " + Codex_localTime(window.resetsAt), 16, y + 52, 2);
  }
  for(uint8_t i = codexWindowCount; i < (force ? CODEX_MAX_WINDOWS : codexRenderedCount); ++i) {
    if(codexWindowCount) tft.fillRect(8, 54 + i * 72, 224, 66, TFT_BLACK);
  }
  memcpy(codexRenderedWindows, codexWindows, sizeof(codexWindows));
  codexRenderedCount = codexWindowCount;
  String footer = codexHasData && codexStatus[0] ? "Cached: " + String(codexStatus) : "Read " + Codex_localTime(codexReadAt);
  if(force || footer != codexRenderedFooter) {
    tft.fillRect(0, 198, 240, 42, TFT_BLACK);
    codexRenderedFooter = footer;
    tft.setTextDatum(ML_DATUM);
    if(codexHasData && codexStatus[0]) {
      tft.setTextColor(TFT_ORANGE, TFT_BLACK);
      tft.drawString("Cached: " + String(codexStatus), 12, 208, 1);
    }
    else {
      tft.setTextColor(muted, TFT_BLACK);
      tft.drawString("Read " + Codex_localTime(codexReadAt), 12, 208, 2);
    }
  }
}

void Codex_result(bool ok, const char* payload) {
  if(currentPage != PAGE_CODEX) return;
  if(!ok) codexStatus = "API unavailable";
  else Codex_parse(payload);
  codexDirty = true;
}
void Codex_enter() { if(!codexHasData) codexRequested = true; }
void Codex_requestRefresh() { codexRequested = true; }
void Codex_poll() {
  if(currentPage != PAGE_CODEX || Network_busy()) return;
  if(codexAttempted && !codexRequested && millis() - codexLastAttempt < CODEX_REFRESH_MS) return;
  if(codexAttempted && millis() - codexLastAttempt < 3000) return;
  codexLastAttempt = millis(); codexAttempted = true; codexRequested = false;
  if(!Network_start(3, 0, CODEX_QUOTAS_URL, "", "")) {
    codexStatus = WiFi.status() == WL_CONNECTED ? "Connection failed" : "Wi-Fi offline";
    codexDirty = true;
  }
}
void Codex_renderPage(bool force) {
  if(force || codexDirty) Codex_drawPage(force);
  codexDirty = false;
}

bool Codex_valid() { return codexHasData; }
