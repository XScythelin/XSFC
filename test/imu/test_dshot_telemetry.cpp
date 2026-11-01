#include "EscDriverBase.hpp"
#include <cassert>
#include <cstdint>
#include <vector>

int main()
{
  // Independent forward encoder: telemetry payload + inverted XOR checksum,
  // 4b/5b GCR, then differential line coding (21 bits including start).
  const uint8_t encode[] = {25, 27, 18, 19, 29, 21, 22, 23, 26, 9, 10, 11, 30, 13, 14, 15};
  for(uint32_t payload = 0; payload < 4096; ++payload)
  {
    uint32_t checksum = (~(payload ^ (payload >> 4) ^ (payload >> 8))) & 15;
    uint32_t packet = (payload << 4) | checksum;
    uint32_t gcr = 0;
    for(int nibble = 3; nibble >= 0; --nibble)
      gcr = (gcr << 5) | encode[(packet >> (nibble * 4)) & 15];
    uint32_t line = 0;
    uint32_t level = 0;
    for(int bit = 20; bit >= 0; --bit)
    {
      level ^= (gcr >> bit) & 1;
      line = (line << 1) | level;
    }
    assert(EscDriverBase::gcrToRawValue(line) == payload);
    for(uint32_t ticks : {107u, 214u}) // DShot600/300 response bit periods
    {
      std::vector<uint16_t> runs;
      for(int bit = 20; bit >= 0;)
      {
        uint32_t runLevel = (line >> bit) & 1;
        uint32_t duration = 0;
        do { duration += ticks; --bit; }
        while(bit >= 0 && ((line >> bit) & 1) == runLevel);
        runs.push_back(static_cast<uint16_t>(duration | (runLevel << 15)));
      }
      std::vector<uint32_t> items;
      for(size_t n = 0; n < runs.size(); n += 2)
        items.push_back(runs[n] | (n + 1 < runs.size() ? uint32_t(runs[n + 1]) << 16 : 0));
      uint32_t captured = EscDriverBase::extractTelemetryGcr(items.data(), items.size(), ticks);
      assert(EscDriverBase::gcrToRawValue(captured) == payload);
      // Hardware capture can merge final HIGH bits with the idle threshold.
      if(line & 1)
      {
        runs.back() = static_cast<uint16_t>((runs.back() & 0x8000) | (runs.back() & 0x7fff) + 2400);
        items.clear();
        for(size_t n = 0; n < runs.size(); n += 2)
          items.push_back(runs[n] | (n + 1 < runs.size() ? uint32_t(runs[n + 1]) << 16 : 0));
        captured = EscDriverBase::extractTelemetryGcr(items.data(), items.size(), ticks);
        assert(EscDriverBase::gcrToRawValue(captured) == payload);
      }
    }
  }
  assert(EscDriverBase::gcrToRawValue(0) == EscDriverBase::INVALID_TELEMETRY_VALUE);
  assert(EscDriverBase::extractTelemetryGcr(nullptr, 0, 0) == 0);
  uint32_t bad[] = {30000}; // An overlong LOW is not a valid telemetry tail.
  assert(EscDriverBase::extractTelemetryGcr(bad, 1, 107) == 0);
}
