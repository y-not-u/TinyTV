#include "PixelDigits.h"

static void Weather_centerText(int x, int y, const String &text, int width, uint16_t color, uint16_t background) {
  String fitted = Text_fit(text.c_str(), width, 2);
  Text_draw(x - Text_width(fitted.c_str(), 2) / 2, y, fitted.c_str(), 2, color, background, width);
}

static void Weather_drawTemperature(int centerX, int centerY, int value, int pitch, int maxWidth,
                                    bool celsius, uint16_t color) {
  char text[6];
  if(value == Weather_missingTemp) snprintf(text, sizeof(text), "--");
  else snprintf(text, sizeof(text), "%d", value);
  int columns = 0;
  for(const char* c = text; *c; ++c) columns += *c == '-' ? 4 : 6;
  const int unitWidth = celsius ? 22 : 0;
  while(columns * pitch - 2 + unitWidth > maxWidth) --pitch;
  int x = centerX - (columns * pitch - 2 + unitWidth) / 2;
  int y = centerY - (7 * pitch - 1) / 2;

  tft.startWrite();
  for(const char* c = text; *c; ++c) {
    if(*c == '-') {
      tft.fillRect(x, y + 3 * pitch, 3 * pitch - 1, pitch - 1, color);
      x += 4 * pitch;
      continue;
    }
    for(int row = 0; row < 7; ++row) {
      uint8_t pixels = TinyTV::pixelDigits[*c - '0'][row];
      for(int column = 0; column < 5; ++column)
        if(pixels & (1 << (4 - column)))
          tft.fillRect(x + column * pitch, y + row * pitch, pitch - 1, pitch - 1, color);
    }
    x += 6 * pitch;
  }
  if(celsius) {
    tft.drawRect(x, y, 7, 7, color);
    tft.fillRect(x + 12, y + 3, 8, 2, color);
    tft.fillRect(x + 10, y + 5, 2, 8, color);
    tft.fillRect(x + 12, y + 13, 8, 2, color);
  }
  tft.endWrite();
}

static void Weather_drawPixels(int x, int y, int size, const char* const* pixels, uint16_t color) {
  for(int row = 0; pixels[row]; ++row) {
    const char* line = pixels[row];
    for(int column = 0; line[column];) {
      if(line[column] == '.') {
        ++column;
        continue;
      }
      int start = column++;
      while(line[column] == '+') ++column;
      tft.fillRect(x + start * size, y + row * size, (column - start) * size, size, color);
    }
  }
}

static void Weather_drawIcon(TinyTV::WeatherIcon icon) {
  using TinyTV::WeatherIcon;
  using namespace TinyTV::WeatherPixels;
  tft.startWrite();
  switch(icon) {
    case WeatherIcon::Sun:
      Weather_drawPixels(152, 78, 7, sun, TinyTV::Theme::amber);
      break;
    case WeatherIcon::Moon:
      Weather_drawPixels(152, 81, 7, moon, TinyTV::Theme::ice);
      break;
    case WeatherIcon::PartlyCloudy:
    case WeatherIcon::PartlyCloudyNight:
      if(icon == WeatherIcon::PartlyCloudy) Weather_drawPixels(176, 73, 5, sun, TinyTV::Theme::amber);
      else Weather_drawPixels(176, 73, 5, moon, TinyTV::Theme::ice);
      Weather_drawPixels(145, 100, 7, cloud, TinyTV::Theme::cloud);
      break;
    case WeatherIcon::Cloud:
      Weather_drawPixels(145, 89, 7, cloud, TinyTV::Theme::cloud);
      break;
    case WeatherIcon::Rain:
    case WeatherIcon::Storm:
    case WeatherIcon::Snow:
    case WeatherIcon::Fog:
      Weather_drawPixels(145, 75, 7, cloud, TinyTV::Theme::cloud);
      if(icon == WeatherIcon::Rain) Weather_drawPixels(145, 123, 7, rain, TinyTV::Theme::ice);
      else if(icon == WeatherIcon::Storm) Weather_drawPixels(154, 120, 5, lightning, TinyTV::Theme::amber);
      else if(icon == WeatherIcon::Snow) Weather_drawPixels(145, 123, 7, snow, TinyTV::Theme::text);
      else Weather_drawPixels(145, 123, 7, fog, TinyTV::Theme::muted);
      break;
    case WeatherIcon::Unknown:
      Weather_drawPixels(169, 89, 6, unknown, TinyTV::Theme::muted);
      break;
  }
  tft.endWrite();
}

static void Weather_drawRange(int x, const char* label, int value, uint16_t color) {
  tft.fillRect(x, 172, 98, 52, TinyTV::Theme::panel);
  Weather_centerText(x + 49, 178, label, 90, TinyTV::Theme::muted, TinyTV::Theme::panel);
  Weather_drawTemperature(x + 49, 209, value, 3, 90, false, color);
}

void Weather_drawCached(bool force) {
  static WeatherSnapshot rendered;
  static bool renderedUnavailable = false;
  if(!force && !weatherDirty) return;

  if(force) {
    tft.fillScreen(TinyTV::Theme::background);
    Theme_drawRule(51);
  }
  if(force || rendered.city != weatherCache.city) {
    tft.fillRect(18, 11, 204, 34, TinyTV::Theme::background);
    Text_drawCenteredDouble(120, 12, weatherCache.city.length() ? weatherCache.city.c_str() : "天气",
                            TinyTV::Theme::text, TinyTV::Theme::background, 204);
  }
  if(force || rendered.temperature != weatherCache.temperature) {
    tft.fillRect(18, 80, 112, 55, TinyTV::Theme::background);
    Weather_drawTemperature(74, 108, weatherCache.temperature, 7, 112, true, TinyTV::Theme::text);
  }
  if(force || rendered.condition != weatherCache.condition || renderedUnavailable != weatherUnavailable) {
    tft.fillRect(18, 140, 112, 19, TinyTV::Theme::background);
    const String &condition = weatherHasData ? weatherCache.condition : String(weatherUnavailable ? "暂无数据" : "更新中");
    Weather_centerText(74, 141, condition, 112, TinyTV::Theme::muted, TinyTV::Theme::background);
  }
  if(force || rendered.icon != weatherCache.icon) {
    tft.fillRect(140, 68, 84, 80, TinyTV::Theme::background);
    Weather_drawIcon(weatherCache.icon);
  }
  if(force || rendered.low != weatherCache.low) Weather_drawRange(18, "最低", weatherCache.low, TinyTV::Theme::ice);
  if(force || rendered.high != weatherCache.high) Weather_drawRange(124, "最高", weatherCache.high, TinyTV::Theme::amber);

  rendered = weatherCache;
  renderedUnavailable = weatherUnavailable;
  weatherDirty = false;
}
