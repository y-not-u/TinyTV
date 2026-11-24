// Quota percentages, not request counts: the API does not expose call totals.
const char* CODEX_QUOTAS_URL = "http://aihub.lan/v1/magpie/quotas";
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
  const char* p = payload;
  while(*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p;

  StaticJsonDocument<512> filter;
  if(*p == '{') {
    filter["data"][0]["provider"] = true;
    filter["data"][0]["plan"] = true;
    filter["data"][0]["readAt"] = true;
    filter["data"][0]["windows"][0]["name"] = true;
    filter["data"][0]["windows"][0]["used"] = true;
    filter["data"][0]["windows"][0]["resetsAt"] = true;
  } else {
    filter[0]["provider"] = true;
    filter[0]["plan"] = true;
    filter[0]["readAt"] = true;
    filter[0]["windows"][0]["name"] = true;
    filter[0]["windows"][0]["used"] = true;
    filter[0]["windows"][0]["resetsAt"] = true;
  }

  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
  if(error) {
    Serial.printf("Codex JSON error: %s\n", error.c_str());
    codexStatus = "Invalid API data";
    return false;
  }

  JsonArray providers = doc.is<JsonArray>() ? doc.as<JsonArray>() : doc["data"].as<JsonArray>();
  if(providers.isNull()) {
    codexStatus = "Invalid API data";
    return false;
  }

  for(JsonObject provider : providers) {
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
static void Codex_drawUsageBar(int y, float used, uint16_t color) {
  const int cells = 20, width = 7;
  int remaining = (int)(cells * width * used / 100.0f + 0.5f);
  tft.startWrite();
  for(int cell = 0; cell < cells; ++cell) {
    int x = 21 + cell * 10;
    tft.fillRect(x, y, width, 7, TinyTV::Theme::track);
    int active = min(width, remaining);
    if(active > 0) tft.fillRect(x, y, active, 7, color);
    remaining -= active;
  }
  tft.endWrite();
}

void Codex_drawPage(bool force) {
  using namespace TinyTV::Theme;
  if(force) tft.fillScreen(background);
  const char* plan = codexHasData ? codexPlan : "";
  if(force || strcmp(codexRenderedPlan, plan)) {
    Theme_drawHeader("Codex", plan);
    strlcpy(codexRenderedPlan, plan, sizeof(codexRenderedPlan));
  }
  if(!codexWindowCount || (!force && !codexRenderedCount))
    tft.fillRect(12, 54, 216, 138, background);
  if(!codexWindowCount) {
    tft.setTextDatum(CC_DATUM);
    tft.setTextColor(text, background);
    tft.drawString(codexStatus, 120, 112, 2);
    tft.setTextColor(muted, background);
    tft.drawString("Double tap to retry", 120, 142, 2);
  }
  for(uint8_t i = 0; i < codexWindowCount; i++) {
    int y = 54 + i * 72;
    CodexWindow &window = codexWindows[i];
    if(!force && i < codexRenderedCount && !memcmp(&window, &codexRenderedWindows[i], sizeof(window))) continue;
    uint16_t accent = window.used >= 90 ? red : window.used >= 70 ? amber : ice;
    tft.fillRect(12, y, 216, 66, panel);
    Text_draw(21, y + 8, window.name, 2, text, panel, 99);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(accent, panel);
    tft.drawString(String(window.used, 1) + "% used", 218, y + 16, 2);
    Codex_drawUsageBar(y + 31, window.used, accent);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(muted, panel);
    tft.drawString("Reset " + Codex_localTime(window.resetsAt), 21, y + 52, 2);
  }
  for(uint8_t i = codexWindowCount; i < (force ? CODEX_MAX_WINDOWS : codexRenderedCount); ++i) {
    if(codexWindowCount) tft.fillRect(12, 54 + i * 72, 216, 66, background);
  }
  memcpy(codexRenderedWindows, codexWindows, sizeof(codexWindows));
  codexRenderedCount = codexWindowCount;
  bool cached = codexWindowCount && codexStatus[0];
  String footer;
  if(cached) footer = "Cached: " + String(codexStatus);
  else if(codexWindowCount) footer = "Read " + Codex_localTime(codexReadAt);
  if(force || footer != codexRenderedFooter) {
    tft.fillRect(0, 198, 240, 42, background);
    codexRenderedFooter = footer;
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(cached ? amber : muted, background);
    tft.drawString(footer, 12, 209, 2);
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
