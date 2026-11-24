#pragma once

#include <stdint.h>

namespace TinyTV {
namespace Theme {

constexpr uint16_t rgb(uint8_t red, uint8_t green, uint8_t blue) {
  return ((red & 0xf8) << 8) | ((green & 0xfc) << 3) | (blue >> 3);
}

constexpr uint16_t background = rgb(0, 0, 0);
constexpr uint16_t panel = rgb(22, 24, 27);
constexpr uint16_t track = rgb(53, 57, 64);
constexpr uint16_t text = rgb(244, 246, 248);
constexpr uint16_t muted = rgb(168, 176, 185);
constexpr uint16_t red = rgb(255, 65, 65);
constexpr uint16_t green = rgb(109, 210, 158);
constexpr uint16_t ice = rgb(125, 212, 253);
constexpr uint16_t amber = rgb(255, 199, 104);
constexpr uint16_t cloud = rgb(174, 199, 217);

}
}
