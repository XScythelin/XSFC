#include "Output/EscBeacon.hpp"
#include "EscDriverBase.hpp"
#include <cassert>

int main()
{
  Xsfc::Output::EscBeacon beacon;
  assert(!beacon.update(2000, true, true));
  assert(!beacon.update(3000, false, true));
  assert(!beacon.update(3000, true, false));
  assert(beacon.update(3000, true, true));
  assert(!beacon.update(3499, true, true));
  assert(beacon.update(3500, true, true));
  assert(!beacon.update(4000, true, false));
  assert(!beacon.update(4000, false, true));
  assert(beacon.update(4000, true, true));
  // BEEP1=1 with telemetry-request bit: payload=3, checksum=3.
  assert(EscDriverBase::dshotEncode(1, false, true) == 0x0033);
  assert(EscDriverBase::dshotEncode(1, true, true) == 0x003c);
  assert(EscDriverBase::dshotEncode(0) == 0);
  assert(EscDriverBase::dshotConvert(1000) == 0);
  assert(EscDriverBase::dshotConvert(1001) == 49);
}
