#pragma once
#include "EscDriverBase.hpp"
#include <array>

// Memory-only ESC endpoint. Never writes GPIO or opens a physical device.
class EscDriverSITL : public EscDriverBase {
public:
  std::array<int, 4> pulses{1000, 1000, 1000, 1000};
  int begin(const EscConfig&) { return 1; }
  void end() { pulses.fill(1000); }
  int attach(size_t channel, int, int pulse) { return write(channel, pulse); }
  int write(size_t channel, int pulse) {
    if (channel >= pulses.size()) return 0;
    pulses[channel] = pulse;
    return 1;
  }
  void apply() {}
  int pin(size_t) const { return -1; }
  uint32_t telemetry(size_t) const { return INVALID_TELEMETRY_VALUE; }
  bool telemetryEnabled() const { return false; }
  bool telemetryAttempted(size_t) const { return false; }
  void collectTelemetry() {}
  void beep(size_t, int) {}
};
