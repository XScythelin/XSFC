// Host regression tests: compile with C++17, stubs first on include path.
#include "Device/GyroICM20948.h"
#include <cassert>
#include <cstring>
#include <initializer_list>

using namespace Xsfc;
using namespace Xsfc::Device;

struct FakeBus : BusDevice
{
  uint8_t regs[4][128] = {};
  unsigned bank = 0;
  BusType type = BUS_I2C;
  int shortRead = 6;
  bool failWrites = false, stuckReset = false;
  unsigned fastReads = 0, writes = 0;
  uint32_t sampleClock = 0;
  FakeBus() { regs[0][0] = 0xEA; }
  BusType getType() const override { return type; }
  int8_t read(uint8_t, uint8_t reg, uint8_t n, uint8_t* out) override
  {
    if(n == 6 && shortRead != 6) return shortRead;
    std::memcpy(out, &regs[bank][reg], n);
    return n;
  }
  int8_t readFast(uint8_t a, uint8_t r, uint8_t n, uint8_t* out) override
  { ++fastReads; return read(a, r, n, out); }
  int8_t readAtSpeed(uint8_t a, uint8_t r, uint8_t n, uint8_t* out, uint32_t speed) override
  { sampleClock = speed; return read(a, r, n, out); }
  bool write(uint8_t, uint8_t r, uint8_t n, const uint8_t* in) override
  {
    ++writes;
    if(failWrites) return false;
    if(r == 0x7F) { bank = in[0] >> 4; return true; }
    if(bank == 0 && r == 6 && in[0] == 0x80)
    { regs[0][6] = stuckReset ? 0x80 : 0x41; bank = 0; return true; }
    std::memcpy(&regs[bank][r], in, n);
    return true;
  }
};

int main()
{
  FakeBus bus;
  GyroICM20948 imu;
  assert(imu.resolveDLPFMode(8) == 1);
  assert(imu.resolveDLPFMode(0) == 0);
  bus.bank = 2; // Warm reset: sensor did not lose power.
  assert(imu.begin(&bus));
  assert(bus.bank == 0 && imu.ready() && imu.getRate() == 1125);
  imu.setDLPFMode(3);
  assert(bus.bank == 0 && bus.regs[2][1] == 0x1F);
  imu.setRate(375);
  assert(imu.getRate() == 375 && bus.regs[2][0] == 2 && bus.bank == 0);
  imu.setDLPFMode(0);
  assert(imu.getRate() == 9000 && !(bus.regs[2][1] & 1));
  imu.setRate(4000);
  assert(imu.getRate() == 9000); // Bypass cannot be divided via SMPLRT_DIV.
  imu.setDLPFMode(7);
  assert(imu.getRate() == 9000);
  imu.setDLPFMode(2);
  assert(imu.getRate() == 1125 && bus.regs[2][1] == 0x17);
  VectorInt16 value;
  value.x = 11; value.y = 22; value.z = 33;
  for(int count : {-1, 0, 1, 5})
  {
    bus.shortRead = count;
    assert(!imu.readGyro(value) && !imu.readAccel(value));
    assert(value.x == 11 && value.y == 22 && value.z == 33);
  }
  assert(imu.readErrors() == 8);
  bus.shortRead = 6;
  bus.regs[0][0x33] = 0x80;
  bus.regs[0][0x35] = 0x7F; bus.regs[0][0x36] = 0xFF;
  assert(imu.readGyro(value) && value.x == -32768 && value.y == 32767);
  assert(imu.consecutiveErrors() == 0 && bus.fastReads == 0);

  FakeBus spi; spi.type = BUS_SPI;
  GyroICM20948 spiImu;
  assert(!spiImu.begin(&spi)); // Explicit CS required.
  assert(spiImu.begin(&spi, 32));
  assert(spi.regs[0][3] == 0x10 && spiImu.readGyro(value));
  assert(spi.fastReads == 0); // Never use shared 16 MHz reads.
  assert(spi.sampleClock == 7000000);

  FakeBus wrong; wrong.regs[0][0] = 0x47;
  GyroICM20948 absent;
  assert(!absent.begin(&wrong));
  FakeBus failed; failed.failWrites = true;
  assert(!absent.begin(&failed));
  FakeBus stuck; stuck.stuckReset = true;
  const auto start = millis();
  assert(!absent.begin(&stuck) && millis() - start < 2500);
}
