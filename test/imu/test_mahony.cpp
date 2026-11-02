// Native checks of the production AHRS math; no hardware access.
#include "Mahony.h"
#include <cassert>
#include <cmath>
#include <initializer_list>
#include <cstring>

static float legacy32(float x)
{
  // Defined bit-copy equivalent of the old algorithm on ESP32's 32-bit long.
  int32_t bits;
  std::memcpy(&bits, &x, sizeof(bits));
  bits = 0x5f3759df - (bits >> 1);
  float y;
  std::memcpy(&y, &bits, sizeof(y));
  y *= 1.5f - 0.5f * x * y * y;
  y *= 1.5f - 0.5f * x * y * y;
  return y;
}

int main()
{
  for(float x : {0.001f, 0.1f, 1.f, 9.81f * 9.81f, 256.f, 1000000.f})
  {
    assert(std::fabs(invSqrt(x) * std::sqrt(x) - 1.f) < 0.00001f);
    assert(invSqrt(x) == legacy32(x));
  }

  constexpr float rad = 0.017453292519943295f;
  for(int rate : {500, 1000, 2000})
  {
    Mahony gyro;
    gyro.begin(rate); gyro.setKp(0); gyro.setKi(0);
    for(int i = 0; i < rate; ++i) gyro.update(0, 30 * rad, 0, 0, 0, 0);
    assert(std::fabs(gyro.getEuler().y / rad - 30) < 0.01f);

    for(float angle : {-20.f, 20.f})
    {
      Mahony tilt;
      tilt.begin(rate); tilt.setKp(1.f); tilt.setKi(0.1f);
      const float pitch = angle * rad;
      for(int i = 0; i < rate * 30; ++i)
        tilt.update(0, 0, 0, -std::sin(pitch), 0, std::cos(pitch));
      assert(std::fabs(tilt.getEuler().y / rad - angle) < 0.2f);
      assert(std::fabs(tilt.getEuler().x / rad) < 0.01f);
    }
  }

  // Known true attitude remains level, but accelerometer sees sustained
  // forward acceleration. A gravity-only AHRS cannot distinguish this from
  // tilt; verify expected false tilt rather than label it an implementation bug.
  Mahony accelerated;
  accelerated.begin(500); accelerated.setKp(1); accelerated.setKi(.1f);
  for(int i=0;i<500*10;++i) accelerated.update(0,0,0,.1f,0,1);
  assert(accelerated.getEuler().y / rad < -5.f);
  assert(accelerated.getEuler().y / rad > -8.f);
  for(int i=0;i<500*60;++i) accelerated.update(0,0,0,0,0,1);
  assert(std::fabs(accelerated.getEuler().y / rad) < .1f);
}
