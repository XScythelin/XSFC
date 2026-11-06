#pragma once
#include "Device/GyroDevice.h"
#include <cmath>

// Synthetic raw-register boundary, not a second sensor/filter implementation.
// ±2000 deg/s gyro and ±16g accel, matching production scaling.
class BusSITL : public Xsfc::Device::BusDevice {
public:
  Xsfc::BusType getType() const override { return Xsfc::BUS_SPI; }
  int8_t read(uint8_t, uint8_t, uint8_t, uint8_t*) override { return 0; }
  int8_t readFast(uint8_t, uint8_t, uint8_t, uint8_t*) override { return 0; }
  bool write(uint8_t, uint8_t, uint8_t, const uint8_t*) override { return false; }
};

class ImuSITL : public Xsfc::Device::GyroDevice {
  BusSITL bus;
public:
  VectorInt16 gyro, accel;
  ImuSITL() { setBus(&bus, 0); }
  int begin(Xsfc::Device::BusDevice*) override { return 1; }
  int begin(Xsfc::Device::BusDevice*, uint8_t) override { return 1; }
  DeviceType getType() const override { return Xsfc::GYRO_ICM42688; }
  int readGyro(VectorInt16& value) override { value = gyro; return 1; }
  int readAccel(VectorInt16& value) override { value = accel; return 1; }
  void setDLPFMode(uint8_t) override {}
  int getRate() const override { return 4000; }
  void setRate(int) override {}
  bool testConnection() override { return true; }
  static int16_t quantize(float value, float scale) {
    return static_cast<int16_t>(std::lround(std::fmax(-32768.f, std::fmin(32767.f, value / scale))));
  }
  void sample(const float* g, const float* a) {
    gyro = VectorInt16(quantize(g[0], Xsfc::Utils::toRad(2000.f) / 32768.f),
                      quantize(g[1], Xsfc::Utils::toRad(2000.f) / 32768.f),
                      quantize(g[2], Xsfc::Utils::toRad(2000.f) / 32768.f));
    accel = VectorInt16(quantize(a[0], 16.f * Xsfc::ACCEL_G / 32768.f),
                       quantize(a[1], 16.f * Xsfc::ACCEL_G / 32768.f),
                       quantize(a[2], 16.f * Xsfc::ACCEL_G / 32768.f));
  }
};
