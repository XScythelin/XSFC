#pragma once

#include "Debug_Xsfc.h"
#include "GyroDevice.h"
#include "Utils/MemoryHelper.h"
#include "helper_3dmath.h"

#define ICM42688_REG_WHO_AM_I         0x75
#define ICM42688_REG_DEVICE_CONFIG    0x11
#define ICM42688_REG_PWR_MGMT0        0x4E
#define ICM42688_REG_GYRO_CONFIG0     0x4F
#define ICM42688_REG_ACCEL_CONFIG0    0x50
#define ICM42688_REG_ACCEL_DATA_X1    0x1F
#define ICM42688_REG_GYRO_DATA_X1     0x25

#define ICM42688_WHO_AM_I_VALUE       0x47
#define ICM42688_SOFT_RESET           0x01
#define ICM42688_PWR_GYRO_ACCEL_LN    0x0F
#define ICM42688_GYRO_2000DPS_8KHZ    0x03
#define ICM42688_ACCEL_16G_8KHZ       0x03

namespace Xsfc::Device {

class GyroICM42688 : public GyroDevice
{
public:
  int begin(BusDevice* bus) override
  {
    if(bus->isSPI()) return 0; // SPI requires an explicit CS GPIO.
    return begin(bus, 0x68) || begin(bus, 0x69);
  }

  int begin(BusDevice* bus, uint8_t addr) override
  {
    setBus(bus, addr);
    _ready = false;
    _readErrors = _consecutiveErrors = _lastGoodReadUs = 0;
    _combinedRequested = _accelCached = false;

    if (!testConnection()) return 0;

    if(!_bus->writeByte(_addr, ICM42688_REG_DEVICE_CONFIG, ICM42688_SOFT_RESET)) return 0;
    delay(2); // no check as min 1ms after soft reset is required before register access (see datasheet)

    // Configure ODR while sensors are off, then allow gyro startup to settle.
    if (!_bus->writeByte(_addr, ICM42688_REG_GYRO_CONFIG0, ICM42688_GYRO_2000DPS_8KHZ)) return 0;
    if (!_bus->writeByte(_addr, ICM42688_REG_ACCEL_CONFIG0, ICM42688_ACCEL_16G_8KHZ)) return 0;
    if (!_bus->writeByte(_addr, ICM42688_REG_PWR_MGMT0, ICM42688_PWR_GYRO_ACCEL_LN)) return 0;
    delay(45);

    _ready = true;
    return 1;
  }

  GyroDeviceType getType() const override
  {
    return GYRO_ICM42688;
  }

  int FAST_CODE_ATTR readGyro(VectorInt16& v) override
  {
    _accelCached = false;
    if(_combinedRequested) {
      _combinedRequested = false;
      uint8_t buffer[12];
      if(!readBytes(ICM42688_REG_ACCEL_DATA_X1, buffer, sizeof(buffer))) return 0;
      _cachedAccel = decode(buffer);
      v = decode(buffer + 6);
      _accelCached = true;
      return 1;
    }
    return readVector(ICM42688_REG_GYRO_DATA_X1, v);
  }

  int FAST_CODE_ATTR readAccel(VectorInt16& v) override
  {
    if(_accelCached) {
      v = _cachedAccel;
      _accelCached = false;
      return 1;
    }
    return readVector(ICM42688_REG_ACCEL_DATA_X1, v);
  }

  void prepareCombinedRead(bool accelDue) override {
    _combinedRequested = accelDue;
    _accelCached = false;
  }

  // ICM-42688-P AAF lives in Bank 1 registers; no-op matches GyroBMI160 pattern
  void setDLPFMode(uint8_t mode) override {}

  int getRate() const override
  {
    return 8000;
  }

  void setRate(int rate) override {}

  bool testConnection() override
  {
    uint8_t whoami = 0;
    // D("icm42688:whoami", _addr, whoami);
    return _bus->readByte(_addr, ICM42688_REG_WHO_AM_I, &whoami) == 1 &&
      whoami == ICM42688_WHO_AM_I_VALUE;
  }

  bool ready() const override { return _ready; }
  uint32_t readErrors() const override { return _readErrors; }
  uint32_t consecutiveErrors() const override { return _consecutiveErrors; }
  uint32_t lastGoodReadUs() const override { return _lastGoodReadUs; }

private:
  bool _ready = false;
  bool _combinedRequested = false, _accelCached = false;
  VectorInt16 _cachedAccel;
  uint32_t _readErrors = 0, _consecutiveErrors = 0, _lastGoodReadUs = 0;

  static VectorInt16 decode(const uint8_t* buffer)
  {
    return VectorInt16(static_cast<int16_t>((uint16_t(buffer[0]) << 8) | buffer[1]),
      static_cast<int16_t>((uint16_t(buffer[2]) << 8) | buffer[3]),
      static_cast<int16_t>((uint16_t(buffer[4]) << 8) | buffer[5]));
  }

  int readBytes(uint8_t reg, uint8_t* buffer, uint8_t length)
  {
    if(!_ready) return 0;
    if(_bus->readFast(_addr, reg, length, buffer) != length) {
      ++_readErrors;
      ++_consecutiveErrors;
      return 0;
    }
    _consecutiveErrors = 0;
    _lastGoodReadUs = micros();
    return 1;
  }

  int readVector(uint8_t reg, VectorInt16& v)
  {
    uint8_t buffer[6];
    if(!readBytes(reg, buffer, sizeof(buffer))) return 0;
    v = decode(buffer);
    return 1;
  }
};

}
