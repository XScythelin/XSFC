#include "Device/GyroICM42688.h"
#include <cassert>
#include <cstring>
#include <initializer_list>

using namespace Xsfc;
using namespace Xsfc::Device;

struct FakeBus : BusDevice
{
  BusType type = BUS_I2C;
  uint8_t address = 0x68;
  uint8_t regs[128] = {};
  int result = 6;
  bool failWrites = false;
  unsigned writes = 0, wrongAddressWrites = 0;
  FakeBus() { regs[ICM42688_REG_WHO_AM_I] = 0x47; }
  BusType getType() const override { return type; }
  int8_t read(uint8_t a, uint8_t r, uint8_t n, uint8_t* out) override
  {
    if(a != address) return -1;
    if(n == 6 && result != 6) return result;
    std::memcpy(out, regs + r, n);
    return n;
  }
  int8_t readFast(uint8_t a, uint8_t r, uint8_t n, uint8_t* out) override
  { return read(a, r, n, out); }
  bool write(uint8_t a, uint8_t r, uint8_t n, const uint8_t* in) override
  {
    if(a != address) { ++wrongAddressWrites; return false; }
    ++writes;
    if(failWrites) return false;
    std::memcpy(regs + r, in, n);
    return true;
  }
};

int main()
{
  for(uint8_t address : {0x68, 0x69})
  {
    FakeBus bus; bus.address = address;
    GyroICM42688 imu;
    assert(imu.begin(&bus) && imu.ready());
    assert(imu.getAddress() == address && bus.wrongAddressWrites == 0);
    assert(imu.getRate() == 8000);
    assert(bus.regs[ICM42688_REG_GYRO_CONFIG0] == 3);
    assert(bus.regs[ICM42688_REG_ACCEL_CONFIG0] == 3);
    assert(bus.regs[ICM42688_REG_PWR_MGMT0] == 15);
    VectorInt16 v(11, 22, 33);
    for(int count : {-1, 0, 1, 5})
    {
      bus.result = count;
      assert(!imu.readGyro(v) && !imu.readAccel(v));
      assert(v.x == 11 && v.y == 22 && v.z == 33);
    }
    bus.result = 6;
    bus.regs[ICM42688_REG_GYRO_DATA_X1] = 0x80;
    bus.regs[ICM42688_REG_GYRO_DATA_X1 + 2] = 0x7F;
    bus.regs[ICM42688_REG_GYRO_DATA_X1 + 3] = 0xFF;
    assert(imu.readGyro(v) && v.x == -32768 && v.y == 32767);
    bus.failWrites = true;
    assert(!imu.begin(&bus) && !imu.ready() && !imu.readGyro(v));
  }
  FakeBus spi; spi.type = BUS_SPI; spi.address = 32;
  GyroICM42688 imu;
  assert(!imu.begin(&spi) && spi.writes == 0);
  assert(imu.begin(&spi, 32));
  FakeBus absent; absent.regs[ICM42688_REG_WHO_AM_I] = 0;
  assert(!imu.begin(&absent) && absent.writes == 0);
}
