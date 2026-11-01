#pragma once
#include <cstdint>
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif
#ifndef PSTR
#define PSTR(x) x
#endif
class Stream;
inline uint32_t testTimeUs = 0;
inline unsigned long micros() { return testTimeUs; }
inline unsigned long millis() { return testTimeUs / 1000; }
inline void delay(unsigned long ms) { testTimeUs += ms * 1000; }
