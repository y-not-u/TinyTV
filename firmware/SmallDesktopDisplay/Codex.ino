// Quota percentages, not request counts: the API does not expose call totals.
const char* CODEX_QUOTAS_URL = "http://aihub.lan/api/usage/quotas";
const unsigned long CODEX_REFRESH_MS = 60000;
const uint16_t CODEX_HTTP_TIMEOUT_MS = 3500;
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

bool Codex_fetch() {
  if(Wifi_en == 0) {
    WiFi.forceSleepWake();
    Wifi_en = 1;
  }
  if(WiFi.status() != WL_CONNECTED) {
    codexStatus = "Wi-Fi offline";
    return false;
  }

  WiFiClient client;
  HTTPClient http;
  // HTTP/1.0 avoids chunked encoding when parsing the response stream.
  http.useHTTP10(true);
  http.setTimeout(CODEX_HTTP_TIMEOUT_MS);
  if(!http.begin(client, CODEX_QUOTAS_URL)) {
    codexStatus = "Connection failed";
    return false;
  }
  http.addHeader("Accept", "application/json");
  int code = http.GET();
  if(code != HTTP_CODE_OK) {
    Serial.printf("Codex HTTP error: %d\n", code);
    codexStatus = "API unavailable";
    http.end();
    return false;
  }

  // Ignore account details, balance histories and all unrelated fields.
  // Stream directly instead of storing the whole multi-provider response.
  StaticJsonDocument<512> filter;
  filter[0]["provider"] = true;
  filter[0]["plan"] = true;
  filter[0]["readAt"] = true;
  filter[0]["windows"][0]["name"] = true;
  filter[0]["windows"][0]["used"] = true;
  filter[0]["windows"][0]["resetsAt"] = true;
  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
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
      strlcpy(nextWindows[count].name, window["name"], sizeof(nextWindows[count].name));
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

void Codex_drawPage() {
  uint16_t muted = tft.color565(136, 148, 160);
  uint16_t panel = tft.color565(12, 18, 24);
  tft.fillScreen(TFT_BLACK);
  tft.setTextWrap(false);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Codex", 12, 22, 4);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(muted, TFT_BLACK);
  tft.drawString(codexPlan, 228, 24, 2);
  tft.drawFastHLine(12, 44, 216, TFT_DARKGREY);

  if(!codexHasData || codexWindowCount == 0) {
    tft.setTextDatum(CC_DATUM);
    tft.drawString(codexStatus, 120, 112, 2);
    tft.drawString("Double tap to retry", 120, 142, 2);
  }
  for(uint8_t i = 0; i < codexWindowCount; i++) {
    int y = 54 + i * 72;
    CodexWindow &window = codexWindows[i];
    uint16_t accent = window.used >= 90 ? TFT_RED :
      (window.used >= 70 ? TFT_ORANGE : tft.color565(93, 219, 166));
    tft.fillRoundRect(8, y, 224, 66, 4, panel);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(TFT_WHITE, panel);
    tft.drawString(window.name, 16, y + 16, 2);
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

void Codex_renderPage(bool force) {
  if(!force && codexAttempted && millis() - codexLastAttempt < CODEX_REFRESH_MS)
    return;

  if(!codexHasData) {
    codexStatus = "Loading quotas...";
    Codex_drawPage();
  }
  Codex_fetch();
  // Record failures too, so an offline API cannot cause a tight retry loop.
  codexLastAttempt = millis();
  codexAttempted = true;
  Codex_drawPage();
}
