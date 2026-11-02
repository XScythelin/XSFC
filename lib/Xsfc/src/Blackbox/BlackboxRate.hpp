#pragma once
#include <cstdint>

namespace Xsfc::Blackbox {
// Stored values 0..4 are rate indices; larger values are legacy I/P ratios.
inline uint8_t sampleRateIndex(uint16_t stored, uint32_t pidIntervalUs)
{
  if(stored <= 4) return static_cast<uint8_t>(stored);
  if(!pidIntervalUs) return 0;
  uint32_t interval = 32000u / pidIntervalUs / stored;
  uint8_t index = 0;
  while(interval > 1 && index < 4) { interval >>= 1; ++index; }
  return index;
}
inline bool loggingEnabled(uint8_t device)
{
  return device == 1 || device == 3; // FLASH or SERIAL; rate index 0 is valid.
}
}
