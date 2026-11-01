#include "Sensor/CurrentConversion.hpp"
#include <cassert>
#include <cmath>

int main()
{
  using namespace Xsfc::Sensor;
  assert(!currentAdcEnabled(0, 21));
  assert(!currentAdcEnabled(1, -1));
  assert(!currentAdcEnabled(2, 21));
  assert(currentAdcEnabled(1, 21));
  assert(std::fabs(currentFromVolts(0.08f, 100, 0) - 0.8f) < 0.00001f);
  assert(std::fabs(currentFromVolts(0.08f, 200, 0) - 1.6f) < 0.00001f);
  assert(std::fabs(currentFromVolts(0.08f, 100, -80)) < 0.00001f);
}
