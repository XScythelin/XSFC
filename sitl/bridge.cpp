// External physics bridge. stdin: gx gy gz ax ay az roll pitch yaw throttle arm.
// Body frame: x forward, y left, z up (FLU). Output normalized four motors.
#include "Mahony.h"
#include "Control/Pid.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>

int main(int argc,char** argv) {
  const int rate=argc>1?std::atoi(argv[1]):4000;
  if(rate!=2000 && rate!=4000 && rate!=8000) return 2;
  Mahony fusion; fusion.begin(500); fusion.setKp(1); fusion.setKi(.1f);
  Xsfc::Control::Pid pid[3];
  const float gains[3][3]={{45,53,24},{44,51,26},{45,69,0}};
  for(int k=0;k<3;++k) {
    pid[k].rate=rate; pid[k].Kp=gains[k][0]*Xsfc::PTERM_SCALE;
    pid[k].Ki=gains[k][1]*Xsfc::ITERM_SCALE;
    pid[k].Kd=gains[k][2]*Xsfc::DTERM_SCALE;
    pid[k].dtermFilter.begin({Xsfc::FILTER_PT1,128},rate);
    pid[k].dtermFilter2.begin({Xsfc::FILTER_PT1,128},rate); pid[k].begin();
  }
  float gx,gy,gz,ax,ay,az,r,p,y,thr; int armed;
  // Physics exchanges at500Hz, with controller substeps. Sensor zero-order
  // hold is an explicit approximation, NOT a4/8kHz IMU model.
  while(std::scanf("%f %f %f %f %f %f %f %f %f %f %d",&gx,&gy,&gz,&ax,&ay,&az,&r,&p,&y,&thr,&armed)==11) {
    fusion.update(gx,gy,gz,ax,ay,az);
    const auto e=fusion.getEuler(); float u[3]{};
    for(int step=0;step<rate/500;++step) {
      const float target[3]={std::clamp((r*.5f-e.x)*4.5f,-3.f,3.f),std::clamp((p*.5f-e.y)*4.5f,-3.f,3.f),y*2.f};
      const float measured[3]={gx,gy,gz};
      for(int k=0;k<3;++k) { if(!armed)pid[k].resetIterm(); u[k]=pid[k].update(target[k],measured[k]); }
    }
    // Motor positions FL,FR,RL,RR; thrust +Z; yaw spin +,-,-,+.
    float motors[4]={thr+u[0]-u[1]+u[2],thr-u[0]-u[1]-u[2],thr+u[0]+u[1]-u[2],thr-u[0]+u[1]+u[2]};
    for(float& m:motors)m=armed?std::clamp(m,0.f,1.f):0;
    std::printf("%.6f %.6f %.6f %.6f %.6f %.6f\n",motors[0],motors[1],motors[2],motors[3],e.x,e.y);std::fflush(stdout);
  }
}
