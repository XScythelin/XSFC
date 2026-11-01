#include "Target/Target.h"
#include "Device/BusI2C.h"
#include <cassert>

int main()
{
  WireClass wire;
  Xsfc::Device::BusI2C bus(wire);
  assert(bus.begin(7, 8, 400000));
  uint8_t value = 0;
  assert(!bus.read(0x68, 0x75, 1, &value));
  assert(!bus.write(0x68, 0x7F, 1, &value));
  assert(wire.probes == 2 && wire.transfers == 0);
  wire.present = true;
  assert(bus.read(0x68, 0x75, 1, &value) == 1 && value == 0x47);
  const auto probes = wire.probes;
  assert(bus.read(0x68, 0x75, 1, &value) == 1);
  assert(wire.probes == probes);
  unsigned errors = 0;
  bus.onError = [&errors]() { ++errors; };
  wire.fail = true;
  assert(!bus.read(0x68, 0x75, 1, &value) && errors == 1);
  assert(!bus.write(0x68, 0x10, 1, &value) && errors == 2);
  assert(!bus.read(0, 0x75, 1, &value));
}
