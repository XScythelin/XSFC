// Algorithm-level P4 profile, NOT an ESP32/FreeRTOS emulator.
#include "Mahony.h"
#include "Control/Pid.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char** argv)
{
  int rate = 4000;
  const char* csv = nullptr;
  for(int i=1; i<argc; ++i) {
    if(!std::strcmp(argv[i], "--rate") && i+1<argc) rate=std::atoi(argv[++i]);
    else if(!std::strcmp(argv[i], "--csv") && i+1<argc) csv=argv[++i];
    else { std::fprintf(stderr,"Usage: xsfc-p4-sitl [--rate 2000|4000|8000] [--csv file]\n"); return 2; }
  }
  if(rate!=2000 && rate!=4000 && rate!=8000) return 2;
  FILE* out=csv ? std::fopen(csv,"w") : nullptr;
  if(csv && !out) return 2;
  if(out) std::fprintf(out,"time_s,target_deg,true_pitch_deg,estimated_pitch_deg,gyro_dps,control,linear_accel_g\n");

  constexpr double rad=0.017453292519943295, dt=1.0/8000;
  Mahony ahrs;
  ahrs.begin(500); ahrs.setKp(1); ahrs.setKi(.1f);
  Xsfc::Control::Pid inner, outer;
  inner.rate=rate;
  // Production pitch gain scaling. Not a complete Controller::begin setup.
  inner.Kp=44*Xsfc::PTERM_SCALE; inner.Ki=51*Xsfc::ITERM_SCALE;
  inner.Kd=26*Xsfc::DTERM_SCALE; inner.Kf=0;
  inner.dtermFilter.begin({Xsfc::FILTER_PT1,128},rate);
  inner.dtermFilter2.begin({Xsfc::FILTER_PT1,128},rate);
  inner.begin();
  outer.rate=rate; outer.Kp=45*Xsfc::LEVEL_PTERM_SCALE;
  outer.oLimitLow=-300*rad; outer.oLimitHigh=300*rad;
  outer.ptermFilter.begin({Xsfc::FILTER_PT1,90},rate); outer.begin();
  Xsfc::Utils::Filter gyroFilter, accelFilter[3];
  gyroFilter.begin({Xsfc::FILTER_PT1,100},rate);
  for(auto& f:accelFilter) f.begin({Xsfc::FILTER_BIQUAD,15},500);

  double angle=0, velocity=0, motor=0, control=0, estimated=0;
  double returnError=0, falseTilt=0, recovered=0;
  bool finite=true;
  // Base clock8kHz; gyro/PID/mixer at requested rate, accel/fusion500Hz.
  // Placeholder plant: pitch inertia0.003kg*m2, differential torque0.12Nm,
  // motor time constant25ms and rotational damping. Not identified hardware.
  for(int tick=0; tick<8000*18; ++tick) {
    const double t=tick*dt;
    const double target=(t>=1 && t<3) ? 15*rad : 0;
    const double acceleration=(t>=7 && t<10) ? .1 : 0;
    if(tick%(8000/rate)==0) {
      const float measured=gyroFilter.update(velocity);
      const float demand=outer.update(target,estimated);
      control=inner.update(demand,measured);
    }
    motor+=(control-motor)*(1-std::exp(-dt/.025));
    velocity+=(.12*motor-.012*velocity)/.003*dt;
    angle+=velocity*dt;
    if(tick%16==0) {
      // Specific force in body frame: gravity plus world horizontal accel.
      // No register quantization, hardware AAF/UI model or aliasing model yet.
      const float ax=accelFilter[0].update(-std::sin(angle)+acceleration*std::cos(angle));
      const float ay=accelFilter[1].update(0);
      const float az=accelFilter[2].update(std::cos(angle)+acceleration*std::sin(angle));
      ahrs.update(0,velocity,0,ax,ay,az);
      estimated=ahrs.getEuler().y;
      if(t>=6 && t<7) returnError=std::max(returnError,std::fabs(angle/rad));
      if(t>=9 && t<10) falseTilt=std::max(falseTilt,std::fabs((estimated-angle)/rad));
      if(t>=17) recovered=std::max(recovered,std::fabs((estimated-angle)/rad));
      finite=finite && std::isfinite(angle) && std::isfinite(estimated) && std::fabs(angle)<1.5;
      if(out) std::fprintf(out,"%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.3f\n",t,target/rad,angle/rad,estimated/rad,velocity/rad,control,acceleration);
    }
  }
  if(out) std::fclose(out);
  std::printf("P4 algorithm profile: gyro/PID/mixer=%dHz accel/fusion=500Hz plant=8000Hz\n",rate);
  std::printf("return_true_error=%.3fdeg accel_induced_estimate_error=%.3fdeg final_estimate_error=%.3fdeg\n",returnError,falseTilt,recovered);
  // Loose smoke checks, NOT acceptance criteria for real flight tuning.
  const bool ok=finite && returnError<2 && falseTilt>1 && recovered<2;
  std::printf("Smoke test: %s (synthetic plant, no hardware timing validation)\n",ok?"PASS":"FAIL");
  return ok?0:1;
}
