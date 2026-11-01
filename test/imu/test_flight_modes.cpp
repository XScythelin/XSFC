#include "FlightModes.hpp"
#include <cassert>
#include <cstring>

int main()
{
  using namespace Xsfc;
  assert(MODE_GPIO_OUTPUT == 9);
  assert(MODE_MAG == 10);
  assert(MODE_POSHOLD == 14);
  bool gpioListed = false;
  for(size_t i = 0; i < sizeof(FLIGHT_MODES) / sizeof(FLIGHT_MODES[0]); ++i)
  {
    const auto& mode = FLIGHT_MODES[i];
    assert(isKnownFlightMode(mode.id));
    assert(mspModeMask(modeBit(mode.id)) == (1u << i));
    gpioListed |= mode.id == MODE_GPIO_OUTPUT;
  }
#if defined(ESP32P4)
  assert(!gpioListed);
  assert(!isKnownFlightMode(MODE_GPIO_OUTPUT));
#else
  assert(gpioListed);
  assert(isKnownFlightMode(MODE_GPIO_OUTPUT));
#endif
  assert(!isKnownFlightMode(8));
  assert(!isKnownFlightMode(11));
  assert(!isKnownFlightMode(12));
}
