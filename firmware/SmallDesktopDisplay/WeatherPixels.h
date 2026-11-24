#pragma once

namespace TinyTV {

enum class WeatherIcon { Unknown, Sun, Moon, PartlyCloudy, PartlyCloudyNight, Cloud, Rain, Storm, Snow, Fog };

inline WeatherIcon weatherIconFor(int code, bool night) {
  if(code == 0) return night ? WeatherIcon::Moon : WeatherIcon::Sun;
  if(code == 1) return night ? WeatherIcon::PartlyCloudyNight : WeatherIcon::PartlyCloudy;
  if(code == 2) return WeatherIcon::Cloud;
  if(code == 4 || code == 5) return WeatherIcon::Storm;
  if((code >= 3 && code <= 12) || (code >= 21 && code <= 25) || code == 301) return WeatherIcon::Rain;
  if((code >= 13 && code <= 17) || (code >= 26 && code <= 28) || code == 302) return WeatherIcon::Snow;
  if(code == 18 || code == 19 || code == 20 || (code >= 29 && code <= 32) ||
     code == 49 || (code >= 53 && code <= 58)) return WeatherIcon::Fog;
  return WeatherIcon::Unknown;
}

namespace WeatherPixels {

constexpr const char* sun[] = {
  "....+....",
  ".+..+..+.",
  "..+++++..",
  "..+++++..",
  "+++++++++",
  "..+++++..",
  "..+++++..",
  ".+..+..+.",
  "....+....",
  nullptr,
};

constexpr const char* moon[] = {
  ".....++..",
  "...+++...",
  "..+++....",
  ".+++.....",
  ".+++.....",
  ".++++...+",
  "..++++++.",
  "...++++..",
  nullptr,
};

constexpr const char* cloud[] = {
  "...+++.....",
  "..+++++....",
  "..++++++...",
  ".+++++++++.",
  "+++++++++++",
  ".+++++++++.",
  nullptr,
};

constexpr const char* rain[] = {
  "..+..+..+..",
  ".+..+..+...",
  ".+..+..+...",
  nullptr,
};

constexpr const char* lightning[] = {
  ".....++....",
  "....++.....",
  "...+++++...",
  ".....++....",
  "....++.....",
  nullptr,
};

constexpr const char* snow[] = {
  "..+.....+..",
  ".+++...+++.",
  "..+.....+..",
  nullptr,
};

constexpr const char* fog[] = {
  ".+++++++...",
  "...........",
  "...+++++++.",
  nullptr,
};

constexpr const char* unknown[] = {
  ".+++.",
  "+...+",
  "....+",
  "..++.",
  "..+..",
  ".....",
  "..+..",
  nullptr,
};

}
}
