// Compile the production object graph, not a parallel controller implementation.
#include "Control/Controller.h"
#include "Output/Mixer.h"
#include <cstdio>
#include <cmath>

int main(int argc, char**) {
  static Xsfc::Model model;
  Xsfc::Control::Controller controller(model);
  Xsfc::Output::Mixer mixer(model);
  model.state.loopTimer.setRate(4000);
  model.state.mixer.timer.setRate(4000);
  model.state.gyro.timer.setRate(4000);
  model.state.gyro.rate = 4000;
  model.config.output.dshotTelemetry = false;
  model.config.output.servoRate = 0;
  for (size_t axis = 0; axis < 4; ++axis) model.state.output.disarmed[axis] = model.config.output.minCommand;
  mixer.begin();
  controller.begin();
  if (argc > 1) {
    // Lockstep input: gyro xyz, attitude xyz (rad), RC r/p/y/throttle,
    // virtual arm, Angle enabled (0 = Acro). Body coordinates are the
    // production mixer convention. This endpoint still bypasses sensor fusion.
    float g[3], e[3], rc[4]; int armed, angle;
    while (std::scanf("%f %f %f %f %f %f %f %f %f %f %d %d",
        &g[0], &g[1], &g[2], &e[0], &e[1], &e[2],
        &rc[0], &rc[1], &rc[2], &rc[3], &armed, &angle) == 12) {
      if ((armed != 0 && armed != 1) || (angle != 0 && angle != 1)) return 4;
      for (int i = 0; i < 3; ++i) {
        if (!std::isfinite(g[i]) || !std::isfinite(e[i])) return 4;
        model.state.gyro.adc.set(i, g[i]);
        model.state.attitude.euler.set(i, e[i]);
      }
      for (int i = 0; i < 4; ++i) {
        if (!std::isfinite(rc[i]) || rc[i] < -1 || rc[i] > 1) return 4;
        model.state.input.ch[i] = rc[i];
        model.state.input.us[i] = 1500 + 500 * rc[i];
      }
      model.updateModes((angle ? 1u << Xsfc::MODE_ANGLE : 0u) | (armed ? 1u << Xsfc::MODE_ARMED : 0u));
      for (int substep = 0; substep < 4; ++substep) {
        sitlTimeUs += 250;
        model.state.loopTimer.update(micros());
        controller.update();
        mixer.update();
      }
      for (int i = 0; i < 4; ++i) std::printf("%d ", model.state.mixer.escMotor->pulses[i]);
      std::puts(""); std::fflush(stdout);
    }
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
