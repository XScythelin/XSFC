#include "Target/Target.h"

#if defined(XSFC_I2C_0)

#include "BusI2C.h"

namespace Xsfc {

namespace Device {

namespace {
// Startup only: release a slave left mid-byte by a warm MCU reset.
// Never drive either line high; external pull-ups provide the high level.
bool releaseBus(int sda, int scl)
{
  pinMode(sda, INPUT_PULLUP);
  pinMode(scl, INPUT_PULLUP);
  auto waitClockHigh = [scl]() {
    const uint32_t start = micros();
    while(!digitalRead(scl))
    {
      if(static_cast<uint32_t>(micros() - start) >= 2000) return false;
      delayMicroseconds(5);
    }
    return true;
  };
  if(!waitClockHigh()) return false;
  if(digitalRead(sda)) return true;

  for(int pulse = 0; pulse < 9 && !digitalRead(sda); pulse++)
  {
    digitalWrite(scl, LOW);
    pinMode(scl, OUTPUT);
    delayMicroseconds(5);
    pinMode(scl, INPUT_PULLUP);
    if(!waitClockHigh()) return false;
    delayMicroseconds(5);
  }

  // STOP: SDA rises while SCL is high. Prepare SDA while SCL is low.
  digitalWrite(scl, LOW);
  pinMode(scl, OUTPUT);
  digitalWrite(sda, LOW);
  pinMode(sda, OUTPUT);
  delayMicroseconds(5);
  pinMode(scl, INPUT_PULLUP);
  if(!waitClockHigh())
  {
    pinMode(sda, INPUT_PULLUP);
    return false;
  }
  delayMicroseconds(5);
  pinMode(sda, INPUT_PULLUP);
  delayMicroseconds(5);
  return digitalRead(sda) && digitalRead(scl);
}
}

BusI2C::BusI2C(WireClass& i2c): _dev(i2c) {}

BusType BusI2C::getType() const { return BUS_I2C; }

int BusI2C::begin(int sda, int scl, uint32_t speed)
{
  if(_ready) return 0; // Do not bit-bang a bus already serving sensors.
  if(sda < 0 || scl < 0 || sda == scl) return 0;
  if(!releaseBus(sda, scl)) return 0;
  _ready = targetI2CInit(_dev, sda, scl, speed) != 0;
  return _ready;
}

int8_t FAST_CODE_ATTR BusI2C::readFast(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint8_t *data)
{
  return read(devAddr, regAddr, length, data);
}

bool BusI2C::ensureAddress(uint8_t address)
{
  if(!_ready || address < 0x08 || address > 0x77) return false;
  if(_addressSeen[address]) return true;
  // An empty transmission uses i2c_master_probe on Arduino/IDF5. Missing
  // devices are expected during discovery; do not issue register traffic.
  _dev.beginTransmission(address);
  if(_dev.endTransmission(true) != 0) return false;
  _addressSeen[address] = true;
  // Cache successes only. Real transfer failures after detection still pass
  // through the normal driver diagnostics and onError path.
  return true;
}

int8_t FAST_CODE_ATTR BusI2C::read(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint8_t *data)
{
  int8_t count = 0;
  if(!_ready || !data || length == 0 || length > 127) return 0;
  if(!ensureAddress(devAddr)) return 0;
  uint32_t t1 = millis();

  //D("i2c:r0", devAddr, regAddr, length);

  _dev.beginTransmission(devAddr);
  _dev.write(regAddr);
  uint8_t status = _dev.endTransmission(false);
  if(status != 0)
  {
    if(onError) onError();
    return 0;
  }
  _dev.requestFrom(devAddr, length);

  for (; count < length && _dev.available() && (_timeout == 0 || millis() - t1 < _timeout); count++)
  {
    data[count] = _dev.read();
    //D("i2c:r1", count, data[count]);
  }

  //D("i2c:r3", length, count);
  if (_timeout > 0 && millis() - t1 >= _timeout && count < length) count = -1; // timeout

  if(onError && count != length) onError();

  return count;
}

bool BusI2C::write(uint8_t devAddr, uint8_t regAddr, uint8_t length, const uint8_t* data)
{
  if(!_ready || (length && !data)) return false;
  if(!ensureAddress(devAddr)) return false;
  //Serial.print("I2C W "); Serial.print(devAddr, HEX); Serial.print(' '); Serial.print(regAddr, HEX); Serial.print(' '); Serial.println(length);

  _dev.beginTransmission(devAddr);
  _dev.write((uint8_t) regAddr); // send address
  for (uint8_t i = 0; i < length; i++)
  {
    _dev.write(data[i]);
    //Serial.print("I2C W "); Serial.print(i); Serial.print(' '); Serial.println(data[i], HEX);
  }
  uint8_t status = _dev.endTransmission();

  if(onError && status != 0) onError();

  return status == 0;
}

}

}

#endif
