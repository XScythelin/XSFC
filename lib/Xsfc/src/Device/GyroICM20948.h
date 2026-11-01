#pragma once

#include "GyroDevice.h"
#include "Debug_Xsfc.h"

namespace Xsfc::Device {

// ICM-20948 banked register map. SPI register setup stays at 1 MHz;
// sample reads use 7 MHz, the device's specified maximum SPI clock.
class GyroICM20948 : public GyroDevice
{
public:
  GyroDeviceType getType() const override { return GYRO_ICM20948; }
  int begin(BusDevice* bus) override
  {
    if(bus->isSPI()) return 0; // SPI requires an explicit CS GPIO.
    return begin(bus, 0x68) || begin(bus, 0x69);
  }

  int begin(BusDevice* bus, uint8_t address) override
  {
    setBus(bus, address);
    _ready = false;
    _rate = 1125;
    if(!testConnection()) return 0;
    for(unsigned attempt = 1; attempt <= 3; attempt++)
    {
      D("ICM20948 init", address, "attempt", attempt);
      if(initialize()) { _ready = true; return 1; }
      D("ICM20948 failed stage", _stage);
      bank(0); // Best effort only; next attempt explicitly selects bank 0.
      delay(25);
    }
    return 0;
  }

  bool testConnection() override
  {
    uint8_t id = 0;
    return bank(0) && _bus->readByte(_addr, 0x00, &id) == 1 && id == 0xEA;
  }

  void setDLPFMode(uint8_t mode) override
  {
    if(!_ready) return;
    // Preserve XSFC's MPU-style menu semantics: 256/EX are high-rate modes.
    // Other entries select the nearest ICM bandwidth, not the MPU register code.
    if(mode > 7) mode = 0;
    const bool bypass = mode == 0 || mode == 7;
    static constexpr uint8_t gyroLpf[] = {0, 0, 2, 3, 4, 5, 6, 0};
    static constexpr uint8_t accelLpf[] = {0, 1, 2, 3, 4, 5, 6, 0};
    const uint8_t gyroCfg = 0x06 | (bypass ? 0 : 1) | (gyroLpf[mode] << 3);
    // Accel is independent: retain its anti-alias filtering at 1125 Hz ODR.
    const uint8_t accelCfg = 0x07 | (accelLpf[mode] << 3);
    const bool ok = bank(2) && writeChecked(0x01, gyroCfg) && writeChecked(0x14, accelCfg) &&
      writeChecked(0x00, 0);
    const bool restored = bank(0);
    if(!ok || !restored) _ready = false;
    else { _bypass = bypass; _rate = bypass ? 9000 : 1125; }
  }

  // Divider applies only with DLPF enabled. Bypass ODR is fixed at 9 kHz.
  int getRate() const override { return _rate; }
  void setRate(int rate) override
  {
    if(!_ready || rate <= 0) return;
    if(_bypass) { _rate = 9000; return; }
    int divisor = (1125 + rate / 2) / rate;
    if(divisor < 1) divisor = 1;
    if(divisor > 256) divisor = 256;
    const bool ok = bank(2) && writeChecked(0x00, divisor - 1);
    const bool restored = bank(0);
    if(ok && restored) _rate = 1125 / divisor;
    else _ready = false;
  }

  int readGyro(VectorInt16& v) override { return _ready && readVector(0x33, v); }
  int readAccel(VectorInt16& v) override { return _ready && readVector(0x2D, v); }
  bool ready() const override { return _ready; }
  uint32_t readErrors() const override { return _readErrors; }
  uint32_t consecutiveErrors() const override { return _consecutiveErrors; }
  uint32_t lastGoodReadUs() const override { return _lastGoodReadUs; }
  unsigned failureStage() const { return _stage; }

private:
  bool _ready = false;
  bool _bypass = false;
  int _rate = 1125;
  unsigned _stage = 0;
  uint32_t _readErrors = 0, _consecutiveErrors = 0, _lastGoodReadUs = 0;

  bool bank(uint8_t value)
  {
    return _bus->writeByte(_addr, 0x7F, value << 4);
  }
  bool writeChecked(uint8_t reg, uint8_t value, uint8_t mask = 0xFF)
  {
    uint8_t actual = 0;
    return _bus->writeByte(_addr, reg, value) &&
      _bus->readByte(_addr, reg, &actual) == 1 && (actual & mask) == (value & mask);
  }
  bool readVector(uint8_t reg, VectorInt16& v)
  {
    uint8_t bytes[6];
    // Shared readFast is 16 MHz, beyond ICM20948's 7 MHz limit.
    if(_bus->readAtSpeed(_addr, reg, sizeof(bytes), bytes, 7000000) != sizeof(bytes))
    {
      ++_readErrors;
      ++_consecutiveErrors;
      return false;
    }
    v.x = static_cast<int16_t>((uint16_t(bytes[0]) << 8) | bytes[1]);
    v.y = static_cast<int16_t>((uint16_t(bytes[2]) << 8) | bytes[3]);
    v.z = static_cast<int16_t>((uint16_t(bytes[4]) << 8) | bytes[5]);
    _consecutiveErrors = 0;
    _lastGoodReadUs = micros();
    return true;
  }
  bool initialize()
  {
    _stage = 1;
    if(!bank(0) || !_bus->writeByte(_addr, 0x06, 0x80)) return false;
    delay(100);
    const uint32_t start = millis();
    bool resetDone = false;
    do
    {
      uint8_t power = 0;
      if(bank(0) && _bus->readByte(_addr, 0x06, &power) == 1 && !(power & 0x80))
      { resetDone = true; break; }
      delay(5);
    } while(uint32_t(millis() - start) < 200);
    _stage = 2;
    if(!resetDone || !testConnection()) return false;
    // Disable I2C interface only for SPI; FIFO/DMP/internal master remain off.
    if(!writeChecked(0x03, _bus->isSPI() ? 0x10 : 0x00, 0xF0) ||
       !writeChecked(0x05, 0x00, 0x70) || !writeChecked(0x06, 0x01, 0x7F) ||
       !writeChecked(0x07, 0x00, 0x3F)) return false;
    delay(35);
    _stage = 3;
    if(!bank(2) || !writeChecked(0x00, 0) || !writeChecked(0x01, 0x07) ||
       !writeChecked(0x10, 0, 0x0F) || !writeChecked(0x11, 0) ||
       !writeChecked(0x14, 0x07)) return false;
    _stage = 4;
    if(!bank(0)) return false;
    delay(20);
    VectorInt16 gyro, accel;
    return readVector(0x33, gyro) && readVector(0x2D, accel);
  }
};
}
