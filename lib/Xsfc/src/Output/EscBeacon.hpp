#pragma once
#include <cstdint>

namespace Xsfc::Output {

// BEEP1 needs >=260 ms between commands; use 500 ms. No throttle values
// or persistent ESC settings are emitted by this scheduler.
class EscBeacon
{
public:
  void notifySaved() { _saveStep = 1; }

  uint8_t updateTone(uint32_t now, bool requested, bool safe)
  {
    if(!safe)
    {
      _saveStep = 0; // Never replay a stale completion after the next flight.
      return 0;
    }
    if(!requested && !_saveStep) return 0;
    if(now < 3000) return 0;
    if(_sent && uint32_t(now - _last) < 500) return 0;
    _last = now;
    _sent = true;
    if(_saveStep)
    {
      const uint8_t tone = _saveStep; // DShot BEEP1, BEEP2, BEEP3.
      _saveStep = _saveStep == 3 ? 0 : _saveStep + 1;
      return tone;
    }
    return 1;
  }

  bool update(uint32_t now, bool requested, bool safe)
  {
    return updateTone(now, requested, safe) != 0;
  }
private:
  uint32_t _last = 0;
  bool _sent = false;
  uint8_t _saveStep = 0;
};
}
