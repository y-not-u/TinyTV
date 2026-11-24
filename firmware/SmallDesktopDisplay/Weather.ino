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

static constexpr int Weather_missingTemp = 1000;

struct WeatherSnapshot {
  String city;
  String condition;
  int temperature = Weather_missingTemp;
  int low = Weather_missingTemp;
  int high = Weather_missingTemp;
  TinyTV::WeatherIcon icon = TinyTV::WeatherIcon::Unknown;
};

static WeatherSnapshot weatherCache;
static String weatherCacheCity;
static bool weatherHasData = false, weatherUnavailable = false, weatherDirty = true;
static bool weatherRequested = false, weatherAttempted = false, cityRequested = false;
static uint32_t weatherLastAttempt = 0;

static int Weather_parseTemperature(const String &value) {
  char* end;
  const char* start = value.c_str();
  float temp = strtof(start, &end);
  if(end == start) return Weather_missingTemp;
  while(isspace((unsigned char)*end)) ++end;
  if(*end || !isfinite(temp) || temp <= -999.5f || temp >= 999.5f) return Weather_missingTemp;
  return (int)(temp < 0 ? temp - 0.5f : temp + 0.5f);
}

static TinyTV::WeatherIcon Weather_parseIcon(const String &value) {
  const char* start = value.c_str();
  bool night = *start == 'n';
  if(night || *start == 'd') ++start;
  if(!isdigit((unsigned char)*start)) return TinyTV::WeatherIcon::Unknown;
  char* end;
  long code = strtol(start, &end, 10);
  return !*end && code <= 999 ? TinyTV::weatherIconFor((int)code, night) : TinyTV::WeatherIcon::Unknown;
}

static bool Weather_updateCache(const char* payload) {
  String body(payload), currentJson, forecastJson;
  // Extract the current conditions and daily range from the provider's JavaScript response.
  if(!Weather_extractJsonObject(body, "var dataSK", &currentJson)) return false;
  Weather_extractJsonObject(body, "\"f\":[", &forecastJson);

  DynamicJsonDocument doc(1024);
  if(deserializeJson(doc, currentJson)) return false;
  JsonObject current = doc.as<JsonObject>();
  if(!current["cityname"].is<const char*>() || !current["weather"].is<const char*>() || !current.containsKey("temp"))
    return false;

  WeatherSnapshot next;
  next.city = current["cityname"].as<String>();
  next.condition = current["weather"].as<String>();
  if(!next.city.length() || !next.condition.length()) return false;
  next.temperature = Weather_parseTemperature(current["temp"].as<String>());
  next.icon = Weather_parseIcon(current["weathercode"].as<String>());
  if(forecastJson.length() && !deserializeJson(doc, forecastJson)) {
    next.low = Weather_parseTemperature(doc["fd"].as<String>());
    next.high = Weather_parseTemperature(doc["fc"].as<String>());
  }
  weatherCache = next;
  return true;
}

void Weather_enter() {
  if(weatherCacheCity != cityCode) weatherRequested = true;
}
void Weather_configurationChanged() {
  Network_cancelOwner(2); Network_cancelOwner(4);
  weatherRequested = true;
}
void getCityCode() { cityRequested = true; }
void getCityWeater() { weatherRequested = true; }
void LCD_reflash(int en) { if(en) Page_requestRedraw(); }
void Weather_result(const String &key, bool ok, const char* payload) {
  if(currentPage != PAGE_WEATHER || cityCode != key) return;
  if(ok && Weather_updateCache(payload)) {
    weatherCacheCity = key;
    weatherHasData = true;
    weatherUnavailable = false;
    weatherDirty = true;
  }
  else {
    if(!weatherHasData && !weatherUnavailable) {
      weatherUnavailable = true;
      weatherDirty = true;
    }
    Serial.println(F("Weather update failed; retaining cache"));
  }
}
void Weather_cityResult(bool ok, const char* payload) {
  if(!ok) return;
  String text(payload);
  int start = text.indexOf("id=");
  if(start < 0) return;
  String code = text.substring(start + 4, start + 13);
  long value = code.toInt();
  if(value >= 101000000 && value <= 102000000) {
    cityCode = code; weatherRequested = true;
  }
}
void Weather_poll() {
  if(currentPage != PAGE_WEATHER || Network_busy()) return;
  if(cityRequested) {
    if(weatherAttempted && millis() - weatherLastAttempt < 3000) return;
    weatherAttempted = true; weatherLastAttempt = millis(); cityRequested = false;
    Network_start(4, 0, "http://wgeo.weather.com.cn/ip/?_=" + String(now()), "", "http://www.weather.com.cn/");
    return;
  }
  bool due = !weatherAttempted || millis() - weatherLastAttempt >= (uint32_t)updateweater_time * 60000UL;
  if(!due && !weatherRequested && !UpdateWeater_en) return;
  if(weatherAttempted && millis() - weatherLastAttempt < 3000) return;
  weatherAttempted = true; weatherLastAttempt = millis();
  weatherRequested = false; UpdateWeater_en = 0;
  Network_start(2, 0, "http://d1.weather.com.cn/weather_index/" + cityCode + ".html?_=" + String(now()),
    cityCode, "http://www.weather.com.cn/weather1d/" + cityCode + ".shtml");
}

bool Weather_valid() { return weatherHasData; }
