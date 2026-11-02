#include <cstddef>
#include <cstdint>
#include <cmath>
#include "Target/Queue.h"
#include <cassert>

int main()
{
  VectorInt16 raw(123, -456, 2048);
  Xsfc::Event queued(Xsfc::EVENT_ACCEL_READ, raw);
  raw = VectorInt16(-32768, 32767, 0);
  const Xsfc::Event received = queued;
  assert(received.type == Xsfc::EVENT_ACCEL_READ);
  assert(received.accel.x == 123 && received.accel.y == -456 && received.accel.z == 2048);
  queued.accel = raw;
  assert(received.accel.z == 2048);
  VectorFloat gyro(1.f, 2.f, 3.f);
  VectorInt16 gyroRaw(10, 20, 30);
  const Xsfc::Event gyroEvent(Xsfc::EVENT_GYRO_READ, gyro, gyroRaw);
  gyro = VectorFloat(9.f, 9.f, 9.f);
  gyroRaw = VectorInt16(99, 99, 99);
  assert(gyroEvent.gyro.x == 1.f && gyroEvent.gyro.z == 3.f);
  assert(gyroEvent.accel.x == 10 && gyroEvent.accel.z == 30);
}
