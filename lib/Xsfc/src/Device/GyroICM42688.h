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
    if(!configureAccelFilters()) return 0;
    if (!_bus->writeByte(_addr, ICM42688_REG_PWR_MGMT0, ICM42688_PWR_GYRO_ACCEL_LN)) return 0;
    delay(45);

    uint8_t actual[3]{};
    if(_bus->read(_addr, ICM42688_REG_PWR_MGMT0, sizeof(actual), actual) != sizeof(actual) ||
       actual[0] != ICM42688_PWR_GYRO_ACCEL_LN ||
       actual[1] != ICM42688_GYRO_2000DPS_8KHZ ||
       actual[2] != ICM42688_ACCEL_16G_8KHZ) return 0;
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
  bool configureAccelFilters()
  {
    // DS-000347 sections 5.3/5.5, 14.40/14.41: filter BEFORE the host
    // takes 500-Hz snapshots of the 8-kHz output. The software LPF is too
    // late to prevent aliasing. Leave gyro AAF/notch/UI settings unchanged.
    // Accel AAF 213 Hz: DELT=5, DELTSQR=25, BITSHIFT=10 (table 5.3).
    bool ok = _bus->writeByte(_addr, 0x76, 2);
    if(ok) ok = _bus->writeByte(_addr, 0x03, 5 << 1);
    if(ok) ok = _bus->writeByte(_addr, 0x04, 25);
    if(ok) ok = _bus->writeByte(_addr, 0x05, 10 << 4);
    uint8_t aaf[3]{};
    if(ok) ok = _bus->read(_addr, 0x03, sizeof(aaf), aaf) == sizeof(aaf) &&
      aaf[0] == (5 << 1) && aaf[1] == 25 && aaf[2] == (10 << 4);
    // Always attempt to restore Bank0, including failed writes/readback.
    const bool restored = _bus->writeByte(_addr, 0x76, 0);
    if(!ok || !restored) return false;

    uint8_t bw = 0, order = 0;
    if(_bus->readByte(_addr, 0x52, &bw) != 1 ||
       _bus->readByte(_addr, 0x53, &order) != 1) return false;
    bw = (bw & 0x0f) | 0x70; // accel UI BW=ODR/40=200 Hz; preserve gyro
    order = (order & ~0x18) | 0x10; // accel UI 3rd order; preserve DEC2/reserved
    if(!_bus->writeByte(_addr, 0x52, bw) || !_bus->writeByte(_addr, 0x53, order)) return false;
    uint8_t actual[2]{};
    return _bus->read(_addr, 0x52, sizeof(actual), actual) == sizeof(actual) &&
      actual[0] == bw && actual[1] == order;
  }

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
