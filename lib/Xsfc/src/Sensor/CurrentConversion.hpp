#pragma once

#include <cstdint>

namespace Xsfc::Sensor {

constexpr bool currentAdcEnabled(int source, int pin)
{
  return source == 1 && pin >= 0;
}

constexpr float currentFromVolts(float volts, int16_t scale, int16_t offset)
{
  return (volts + offset * 0.001f) * (scale * 0.1f);
}

} // namespace Xsfc::Sensor
