#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#define IRAM_ATTR
#define PROGMEM
#define PSTR(x) x
#define F(x) x
#define pgm_read_byte(p) (*(const unsigned char*)(p))
#define pgm_read_ptr(p) (*(p))
#define LOW 0
#define HIGH 1
#define INPUT 0
#define OUTPUT 1
#define HEX 16
#define PI 3.14159265358979323846
#define TWO_PI 6.28318530717958647692
class String : public std::string {
public:
  String() = default;
  String(const char* text) : std::string(text) {}
  String(const std::string& text) : std::string(text) {}
  template<class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
  String(T value) : std::string(std::to_string(value)) {}
  String(double value, int digits = 2) {
    char text[64];
    std::snprintf(text, sizeof(text), "%.*f", digits, value);
    assign(text);
  }
};
template<class T, class L, class H> inline T constrain(T value, L low, H high) {
  return std::min<T>(std::max<T>(value, low), high);
}
inline uint64_t sitlTimeUs = 0;
inline unsigned long micros() { return static_cast<uint32_t>(sitlTimeUs); }
inline unsigned long millis() { return static_cast<uint32_t>(sitlTimeUs / 1000); }
inline void delay(unsigned long ms) { sitlTimeUs += ms * 1000; }
inline void delayMicroseconds(unsigned int us) { sitlTimeUs += us; }
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
class Print {
public:
  virtual ~Print() = default;
  virtual size_t write(uint8_t) { return 1; }
  virtual size_t write(const uint8_t*, size_t count) { return count; }
  template<class T> size_t print(const T&, int = 10) { return 0; }
  template<class T> size_t println(const T&, int = 10) { return 0; }
  size_t println() { return 0; }
};
class Stream : public Print {
public:
  virtual int available() { return 0; }
  virtual int read() { return -1; }
  virtual int peek() { return -1; }
  virtual void flush() {}
};
