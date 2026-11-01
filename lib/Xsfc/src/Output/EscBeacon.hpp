#pragma once
#include <cstdint>

namespace Xsfc::Output {

// BEEP1 needs >=260 ms between commands; use 500 ms. No throttle values
// or persistent ESC settings are emitted by this scheduler.
class EscBeacon
{
public:
  bool update(uint32_t now, bool requested, bool safe)
  {
    if(!requested || !safe) return false;
    if(now < 3000) return false; // Give ESC startup/zero throttle time to settle.
    if(_sent && uint32_t(now - _last) < 500) return false;
    _last = now;
    _sent = true;
    return true;
  }
private:
  uint32_t _last = 0;
  bool _sent = false;
};
}
