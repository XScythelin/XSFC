#pragma once

#include <helper_3dmath.h>
#include "BusDevice.hpp"
#include "BusAwareDevice.hpp"

namespace Xsfc {

enum GyroDeviceType {
  GYRO_AUTO    = 0,
  GYRO_NONE    = 1,
  GYRO_MPU6000 = 2,
  GYRO_MPU6050 = 3,
  GYRO_MPU6500 = 4,
  GYRO_MPU9250 = 5,
  GYRO_LSM6DSO = 6,
  GYRO_ICM20602 = 7,
  GYRO_BMI160 = 8,
  GYRO_ICM42688 = 9,
  GYRO_ICM20948 = 10,
  GYRO_MAX
};

namespace Device {

class GyroDevice: public BusAwareDevice
{
  public:
    typedef GyroDeviceType DeviceType;

    virtual int begin(BusDevice * bus) = 0;
    virtual int begin(BusDevice * bus, uint8_t addr) = 0;

    virtual DeviceType getType() const = 0;
    
    virtual int readGyro(VectorInt16& v) = 0;
    virtual int readAccel(VectorInt16& v) = 0;

    virtual void setDLPFMode(uint8_t mode) = 0;
    // CLI AUTO keeps legacy behavior for other IMUs, enables DLPF on ICM20948.
    uint8_t resolveDLPFMode(uint8_t mode) const
    {
      return mode == 8 ? (getType() == GYRO_ICM20948 ? 1 : 0) : mode;
    }
    virtual int getRate() const = 0;
    virtual void setRate(int rate) = 0;

    virtual bool testConnection() = 0;

    // Optional diagnostics. Drivers without checked transfers return zero.
    virtual bool ready() const { return true; }
    virtual uint32_t readErrors() const { return 0; }
    virtual uint32_t consecutiveErrors() const { return 0; }
    virtual uint32_t lastGoodReadUs() const { return 0; }

    static const char ** getNames();
    static const char * getName(DeviceType type);
};

}

}
