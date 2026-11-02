#include "Output/EscBeacon.hpp"
#include <cassert>

int main()
{
  Xsfc::Output::EscBeacon beacon;
  beacon.notifySaved();
  assert(beacon.updateTone(4000, false, true) == 1);
  assert(beacon.updateTone(4499, true, true) == 0);
  assert(beacon.updateTone(4500, true, true) == 2);
  assert(beacon.updateTone(5000, false, true) == 3);
  assert(beacon.updateTone(5500, false, true) == 0);
  assert(beacon.updateTone(5500, true, true) == 1);
  beacon.notifySaved();
  assert(beacon.updateTone(6000, false, false) == 0);
  assert(beacon.updateTone(6500, false, true) == 0);
  beacon.notifySaved();
  assert(beacon.updateTone(6500, false, true) == 1);
}
