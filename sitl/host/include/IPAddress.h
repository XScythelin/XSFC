#pragma once
#include <cstdint>
class IPAddress {
public:
  IPAddress(uint32_t = 0) {}
  IPAddress(uint8_t, uint8_t, uint8_t, uint8_t) {}
};
