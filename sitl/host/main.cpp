// Compile the production object graph, not a parallel controller implementation.
#include "Control/Controller.h"
#include "Output/Mixer.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include "Sensor/GyroSensor.h"
#include "Sensor/AccelSensor.h"
#include "Control/Fusion.h"
#include "ImuSITL.h"
#include "Control/Altitude.hpp"
#include "Sensor/BaroAltitude.hpp"
#include "Sensor/RangeMeasurement.hpp"
#include "SettingsSITL.h"
#include "Input.h"
#include "InputSITL.h"
#include "Control/Actuator.h"

int main(int argc, char** argv) {
  static Xsfc::Model model;
  if (!loadSettingsSITL(model, std::getenv("XSFC_HOST_SETTINGS"))) {
    std::fprintf(stderr, "Invalid SITL settings overlay; startup refused\n");
    return 6;
  }
  Xsfc::Control::Controller controller(model);
  Xsfc::Output::Mixer mixer(model);
  const bool rawImu = argc > 1 && std::strcmp(argv[1], "--raw-imu") == 0;
  const bool diagnostics = std::getenv("XSFC_HOST_DIAGNOSTICS") != nullptr;
  const bool altitudeEndpoint = std::getenv("XSFC_HOST_ALTITUDE") != nullptr;
  const bool receiverEndpoint = std::getenv("XSFC_HOST_RECEIVER") != nullptr;
  const bool rangeEndpoint = std::getenv("XSFC_HOST_RANGE") != nullptr;
  if (altitudeEndpoint && !rawImu) return 4;
  if (rangeEndpoint && !altitudeEndpoint) return 4;
  const int exchangeMs = argc > 2 ? std::atoi(argv[2]) : 1;
  if (exchangeMs < 1 || exchangeMs > 2) return 4;
  ImuSITL imu;
  Xsfc::Sensor::GyroSensor gyroSensor(model);
  Xsfc::Sensor::AccelSensor accelSensor(model);
  Xsfc::Control::Fusion fusion(model);
  Xsfc::Control::Altitude altitude(model);
  Xsfc::Sensor::BaroAltitude baro;
  Xsfc::Sensor::BaroFrame baroFrame;
  Xsfc::Sensor::RangeMeasurement range;
  Xsfc::Sensor::RangeFrame rangeFrame;
  InputSITL receiver;
  Xsfc::Input input(model, receiver);
  Xsfc::Control::Actuator actuator(model);
  model.state.loopTimer.setRate(4000);
  model.state.mixer.timer.setRate(4000);
  model.state.gyro.timer.setRate(4000);
  model.state.gyro.rate = 4000;
  model.config.output.dshotTelemetry = false;
  model.config.output.servoRate = 0;
  for (size_t axis = 0; axis < 4; ++axis) model.state.output.disarmed[axis] = model.config.output.minCommand;
  if (rawImu) {
    model.config.loopSync = 2;
    model.config.mixerSync = 1;
    model.config.gyroRate = 8000;
    // Hardware::begin supplies this before Model::begin on the real target.
    model.state.gyro.clock = imu.getRate();
    model.state.gyro.dev = &imu;
    model.state.gyro.present = model.state.accel.present = true;
    model.begin();
    if (model.state.loopTimer.rate != 4000 || model.state.gyro.timer.rate != 8000 || model.state.mixer.timer.rate != 4000) return 5;
    gyroSensor.begin();
    accelSensor.begin();
    fusion.begin();
    if (altitudeEndpoint) {
      model.config.baro.dev = Xsfc::BARO_BMP280;
      model.state.baro.present = true;
      altitude.begin();
      if (rangeEndpoint) {
        model.config.rangefinder.dev = Xsfc::RANGEFINDER_MSP;
        model.config.featureMask |= Xsfc::FEATURE_RANGEFINDER;
        range.begin(model.config.rangefinder.filter);
      }
    }
  }
  mixer.begin();
  controller.begin();
  if (receiverEndpoint) {
    // Real boards select their ESC backend in saved config. The host's memory
    // backend needs an enabled protocol for production arming checks as well.
    model.config.output.protocol = ESC_PROTOCOL_DSHOT300;
    for (int i = 0; i < 16; ++i) model.config.input.channel[i].map = i;
    for (auto& condition : model.config.conditions) condition.min = condition.max = 0;
    const Xsfc::FlightMode modes[] = {Xsfc::MODE_ARMED, Xsfc::MODE_ANGLE, rangeEndpoint ? Xsfc::MODE_RANGEHOLD : Xsfc::MODE_ALTHOLD};
    for (int i = 0; i < 3; ++i) {
      auto& condition = model.config.conditions[i];
      condition.id = modes[i]; condition.ch = Xsfc::AXIS_AUX_1 + i;
      condition.min = 1700; condition.max = 2100;
    }
    input.begin();
    actuator.begin();
  }
  if (argc > 1) {
    uint64_t exchanges = 0, gyroTicks = 0, pidTicks = 0, mixerTicks = 0;
    // Lockstep input: gyro xyz, attitude xyz (rad), RC r/p/y/throttle,
    // virtual arm, Angle enabled (0 = Acro). Body coordinates are the
    // production mixer convention. This endpoint still bypasses sensor fusion.
    float g[3], e[3], rc[4]; int armed, angle;
    while (std::scanf("%f %f %f %f %f %f %f %f %f %f %d %d",
        &g[0], &g[1], &g[2], &e[0], &e[1], &e[2],
         &rc[0], &rc[1], &rc[2], &rc[3], &armed, &angle) == 12) {
      float pressure = 101325.f;
      int hold = 0, baroHealthy = 1;
      if (altitudeEndpoint && (std::scanf("%f %d %d", &pressure, &hold, &baroHealthy) != 3 ||
          !std::isfinite(pressure) || pressure < 1000 || pressure > 120000 ||
          (hold != 0 && hold != 1) || (baroHealthy != 0 && baroHealthy != 1))) return 4;
      int rxHealthy = 1;
      int rangeMm = 0, rangeHealthy = 1;
      if (rangeEndpoint && (std::scanf("%d %d", &rangeMm, &rangeHealthy) != 2 ||
          (rangeHealthy != 0 && rangeHealthy != 1))) return 4;
      if (receiverEndpoint && (std::scanf("%d", &rxHealthy) != 1 || (rxHealthy != 0 && rxHealthy != 1))) return 4;
      if ((armed != 0 && armed != 1) || (angle != 0 && angle != 1)) return 4;
      for (int i = 0; i < 3; ++i) {
        if (!std::isfinite(g[i]) || !std::isfinite(e[i])) return 4;
        if (!rawImu) {
          model.state.gyro.adc.set(i, g[i]);
          model.state.attitude.euler.set(i, e[i]);
        }
      }
      if (rawImu) imu.sample(g, e); // fields 4..6 are specific force, NOT Euler angles.
      for (int i = 0; i < 4; ++i) {
        if (!std::isfinite(rc[i]) || rc[i] < -1 || rc[i] > 1) return 4;
        if (!receiverEndpoint) {
          model.state.input.ch[i] = rc[i];
          model.state.input.us[i] = 1500 + 500 * rc[i];
        }
        receiver.channels[i] = std::lround(1500 + 500 * rc[i]);
      }
      const uint32_t modes = (angle ? 1u << Xsfc::MODE_ANGLE : 0u) |
          (armed ? 1u << Xsfc::MODE_ARMED : 0u) | (hold ? 1u << (rangeEndpoint ? Xsfc::MODE_RANGEHOLD : Xsfc::MODE_ALTHOLD) : 0u);
      if (!receiverEndpoint) {
        model.updateSwitchActive(modes);
        model.updateModes(modes);
      }
      receiver.channels[4] = armed ? 2000 : 1000;
      receiver.channels[5] = angle ? 2000 : 1000;
      receiver.channels[6] = hold ? 2000 : 1000;
      for (int i = 7; i < 16; ++i) receiver.channels[i] = 1000;
      receiver.connected = rxHealthy;
      for (int substep = 0; substep < 8 * exchangeMs; ++substep) {
        sitlTimeUs += 125;
        const bool controlTick = sitlTimeUs % 250 == 0;
        if (controlTick) model.state.loopTimer.update(micros());
        if (receiverEndpoint && controlTick) {
          if (sitlTimeUs % 4000 == 0) receiver.fresh = true;
          input.update();
          if (sitlTimeUs % 1000 == 0) actuator.update();
        }
        if (rawImu) {
          gyroSensor.read();
          ++gyroTicks;
          if (controlTick) gyroSensor.filter();
          // Match SensorManager::preLoop: startup gain is only for calibration.
          if (model.state.gyro.biasSamples == 0) {
            model.state.gyro.biasSamples = -1;
            fusion.restoreGain();
          }
          if (sitlTimeUs % 2000 == 0) {
            accelSensor.update();
            fusion.update();
            if (altitudeEndpoint) {
              if (rangeEndpoint && sitlTimeUs % 20000 == 0 && rangeHealthy) {
                range.sample(rangeMm, model.state.attitude.cosTheta, micros(), 6000);
                rangeFrame.present = true;
                rangeFrame.valid = range.valid;
                rangeFrame.validSamples = range.streak;
                rangeFrame.height = range.height;
                rangeFrame.distance = range.distance;
                rangeFrame.vario = range.vario;
                rangeFrame.sampleIntervalf = range.sampleDt;
                rangeFrame.lastSampleUs = micros();
                rangeFrame.status = range.status;
                rangeFrame.raw = rangeMm;
                rangeFrame.maxRangeMm = 6000;
                ++rangeFrame.updateCount;
                model.state.rangefinder.sample.store(rangeFrame);
              }
              if (sitlTimeUs % 20000 == 0 && baroHealthy) {
                const auto status = baro.update(pressure, micros(), model.isModeActive(Xsfc::MODE_ARMED));
                baroFrame.healthy = true;
                baroFrame.altitudeBiasSamples = status == Xsfc::Sensor::BaroAltitude::Result::READY ? -1 : 1;
                baroFrame.pressure = pressure;
                baroFrame.altitudeGround = baro.height;
                baroFrame.vario = baro.vario;
                baroFrame.sampleIntervalf = baro.sampleDt;
                baroFrame.lastUpdateUs = micros();
                ++baroFrame.updateCount;
                model.state.baro.sample.store(baroFrame);
              }
              altitude.update();
            }
          }
          if (controlTick) gyroSensor.postLoop();
        }
        if (controlTick) {
          controller.update();
          ++pidTicks;
          mixer.update();
          ++mixerTicks;
        }
      }
      ++exchanges;
      for (int i = 0; i < 4; ++i) std::printf("%d ", model.state.mixer.escMotor->pulses[i]);
      if (diagnostics) {
        for (int i = 0; i < 3; ++i) std::printf("%.9g ", model.state.attitude.euler[i]);
        for (int i = 0; i < 3; ++i) std::printf("%.9g ", model.state.gyro.scaled[i]);
        for (int i = 0; i < 3; ++i) std::printf("%.9g ", model.state.gyro.adc[i]);
        std::printf("%d ", static_cast<int>(model.state.gyro.calibrationState));
        if (altitudeEndpoint) std::printf("%.9g %.9g %.9g %d %d ", model.state.altitude.height,
            model.state.altitude.vario, model.state.altitude.target,
            model.state.altitude.engaged ? 1 : 0, model.baroReadyForAltHold() ? 1 : 0);
        if (rangeEndpoint) std::printf("%.9g %.9g %d %d %d ", model.state.altitude.rangeHeight,
            model.state.altitude.rangeVario, model.rangefinderReadyForHold() ? 1 : 0,
            model.state.altitude.rangeSource ? 1 : 0, model.state.altitude.sensorLost ? 1 : 0);
        if (receiverEndpoint) std::printf("%u %u %d %d ", model.state.mode.mask,
            model.state.mode.armingDisabledFlags, static_cast<int>(model.state.failsafe.phase),
            model.state.input.channelsValid ? 1 : 0);
      }
      std::puts(""); std::fflush(stdout);
    }
    const uint64_t expectedPidTicks = exchanges * 4 * exchangeMs;
    if (pidTicks != expectedPidTicks || mixerTicks != expectedPidTicks ||
        (rawImu && gyroTicks != exchanges * 8 * exchangeMs)) return 7;
    if (rawImu) {
      std::fprintf(stderr, "IMU %.9g %.9g %.9g GYRO %.9g %.9g %.9g CAL %d\n",
                   model.state.attitude.euler.x, model.state.attitude.euler.y, model.state.attitude.euler.z,
                   model.state.gyro.adc.x, model.state.gyro.adc.y, model.state.gyro.adc.z,
                   static_cast<int>(model.state.gyro.calibrationState));
    }
    std::fprintf(stderr, "SCHEDULE exchange_ms=%d exchanges=%llu gyro=%llu pid=%llu mixer=%llu\n",
                 exchangeMs, static_cast<unsigned long long>(exchanges),
                 static_cast<unsigned long long>(gyroTicks), static_cast<unsigned long long>(pidTicks),
                 static_cast<unsigned long long>(mixerTicks));
    return 0;
  }
  model.state.input.ch[Xsfc::AXIS_THRUST] = -1.f;
  model.state.input.us[Xsfc::AXIS_THRUST] = 1000.f;
  for (int tick = 0; tick < 4000; ++tick) {
    sitlTimeUs += 250;
    model.state.loopTimer.update(micros());
    controller.update();
    mixer.update();
    for (size_t axis = 0; axis < 4; ++axis) {
      if (!std::isfinite(model.state.output.ch[axis])) return 2;
      if (model.state.mixer.escMotor->pulses[axis] != 1000) return 3;
    }
  }
  std::puts("XSFC host: production Controller + Mixer, 4000 disarmed ticks, four outputs at 1000; PASS");
  return 0;
}
