#pragma once
#include <stdint.h>
#define PROGMEM
inline uint16_t pgm_read_word(const uint16_t* p) { return *p; }
inline uint8_t pgm_read_byte(const uint8_t* p) { return *p; }
