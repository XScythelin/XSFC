#include "SensorManager.h"

namespace Xsfc {

SensorManager::SensorManager(Model& model):
  _model(model), _gyro(model), _accel(model), _mag(model), _baro(model),
  _rangefinder(model), _flow(model), _voltage(model), _fusion(model), _altitude(model), _fusionUpdate(false)
{
}

int SensorManager::begin()
{
  _gyro.begin();
  _accel.begin();
  _mag.begin();
  _baro.begin();
  _rangefinder.begin();
  _flow.begin();
  _voltage.begin();
  _fusion.begin();
  _altitude.begin();
  _button.begin(_model.config.pin[PIN_BUTTON]);

  return 1;
}

int FAST_CODE_ATTR SensorManager::read()
{
  const auto& accelTimer = _model.state.accel.timer;
  const bool accelDue = accelTimer.denom > 0
    ? (_model.state.gyro.timer.iteration % accelTimer.denom == 0)
    : (accelTimer.interval != 0 && micros() >= accelTimer.next);
  if(_model.state.gyro.dev) _model.state.gyro.dev->prepareCombinedRead(accelDue && _model.accelActive());
  const bool gyroRead = _gyro.read() != 0;

  if(_model.state.loopTimer.syncTo(_model.state.gyro.timer) && gyroRead)
  {
    _model.state.appQueue.send(Event(EVENT_GYRO_READ));
  }

  if(_model.state.accel.timer.syncTo(_model.state.gyro.timer))
  {
    if(_accel.update()) _model.state.appQueue.send(Event(EVENT_ACCEL_READ));
    _model.state.mode.button = _button.update();
    return 1;
  }

  if(_mag.update()) return 1;

  if(_baro.update()) return 1;
  if(_rangefinder.update()) return 1;
  if(_voltage.update()) return 1;

  return 0;
}

int FAST_CODE_ATTR SensorManager::preLoop()
{
  _gyro.filter();
  _flow.accumulateGyro();
  if(_model.state.gyro.biasSamples == 0)
  {
    _model.state.gyro.biasSamples = -1;
    _fusion.restoreGain();
  }
  return 1;
}

int SensorManager::postLoop()
{
  _gyro.postLoop();
  _flow.update();
  return 1;
}

int FAST_CODE_ATTR SensorManager::fusion()
{
  _fusion.update();
  _altitude.update();
  return 1;
}

// main task
int FAST_CODE_ATTR SensorManager::update()
{
  if(!_gyro.read()) return 0;
  return preLoop();
}

// sub task
int SensorManager::updateDelayed()
{
  _gyro.postLoop();
  _flow.update();

  // update at most one sensor besides gyro
  int status = 0;
  if(_model.state.accel.timer.syncTo(_model.state.gyro.timer))
  {
    const int accelRead = _accel.update();
    _model.state.mode.button = _button.update();
    status = accelRead;
  }

  // delay imu update to next cycle
  if(_fusionUpdate)
  {
    _fusionUpdate = false;
    fusion();
  }
  _fusionUpdate = status;

  if(status) return 1;

  if(_mag.update()) return 1;

  if(_baro.update()) return 1;

  if(_rangefinder.update()) return 1;

  if(_voltage.update()) return 0;

  return 0;
}

}
