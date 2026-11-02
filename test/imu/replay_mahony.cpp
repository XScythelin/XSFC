// Offline replay of production Mahony. Input: 500-Hz rows in SI units:
// gx gy gz ax ay az loggedRollDeg loggedPitchDeg. No board access.
#include "Mahony.h"
#include <cstdio>
#include <cmath>
#include <algorithm>

int main()
{
  Mahony filters[4];
  const float kp[] = {1.f, .5f, .2f, 0.f};
  const float ki[] = {.1f, .1f, .1f, 0.f};
  for(int i=0; i<4; ++i) {
    filters[i].begin(500); filters[i].setKp(kp[i]); filters[i].setKi(ki[i]);
  }
  float gx,gy,gz,ax,ay,az,roll,pitch;
  double squared[4][2]{};
  float peak[4][2]{};
  unsigned n=0;
  while(std::scanf("%f %f %f %f %f %f %f %f", &gx,&gy,&gz,&ax,&ay,&az,&roll,&pitch)==8)
  {
    if(n==0) {
      // Initialize tilt from the first available logged attitude, not from
      // possibly accelerated accel. Yaw is arbitrary without magnetometer.
      const float r=roll*.017453292519943295f, p=pitch*.017453292519943295f;
      for(auto& filter:filters) {
        filter.setKp(10); filter.setKi(0);
        for(int j=0;j<3000;++j)
          filter.update(0,0,0,-std::sin(p),std::sin(r)*std::cos(p),std::cos(r)*std::cos(p));
      }
      for(int i=0;i<4;++i) { filters[i].setKp(kp[i]); filters[i].setKi(ki[i]); }
    }
    for(int i=0;i<4;++i) {
      filters[i].update(gx,gy,gz,ax,ay,az);
      const auto e=filters[i].getEuler();
      const float error[]={e.x*57.29577951308232f-roll,e.y*57.29577951308232f-pitch};
      for(int k=0;k<2;++k) { squared[i][k]+=error[k]*error[k]; peak[i][k]=std::max(peak[i][k],std::fabs(error[k])); }
    }
    ++n;
  }
  if(!n) return 1;
  for(int i=0;i<4;++i)
    std::printf("twoKp=%.2f twoKi=%.2f rows=%u deviation_from_logged_RP_rms_deg=%.4f/%.4f peak_deg=%.4f/%.4f\n",
      kp[i],ki[i],n,std::sqrt(squared[i][0]/n),std::sqrt(squared[i][1]/n),peak[i][0],peak[i][1]);
}
