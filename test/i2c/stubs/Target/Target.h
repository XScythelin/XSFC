#pragma once
#include <cstdint>
#define XSFC_I2C_0
#define FAST_CODE_ATTR
#define INPUT_PULLUP 1
#define OUTPUT 2
#define LOW 0
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int) { return 1; }
inline void delayMicroseconds(int) {}
inline uint32_t micros() { return 0; }
inline uint32_t millis() { return 0; }
struct WireClass
{
  unsigned probes = 0, transfers = 0;
  unsigned bytes = 0;
  uint8_t address = 0;
  bool present = false, fail = false;
  void beginTransmission(uint8_t a) { address = a; bytes = 0; }
  void write(uint8_t) { ++bytes; }
  uint8_t endTransmission(bool = true)
  {
    if(!bytes) { ++probes; return present ? 0 : 2; }
    ++transfers;
    return fail ? 2 : 0;
  }
  void requestFrom(uint8_t, uint8_t) {}
  int available() { return 1; }
  int read() { return 0x47; }
};
inline int targetI2CInit(WireClass&, int, int, uint32_t) { return 1; }
