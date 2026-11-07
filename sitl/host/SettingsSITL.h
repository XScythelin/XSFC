#pragma once
#include "Model.h"
#include <fstream>
#include <sstream>
#include <string>

// Deliberately narrow flight-dynamics overlay. Never load a binary board dump,
// pin assignments or calibration, and never persist anything to Storage.
inline bool loadSettingsSITL(Xsfc::Model& model, const char* path) {
  if (!path) return true;
  std::ifstream stream(path);
  if (!stream) return false;
  std::string line;
  while (std::getline(stream, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream row(line);
    std::string key, extra;
    int value;
    if (!(row >> key >> value) || (row >> extra)) return false;
    bool found = false;
    const char* axes[] = {"roll", "pitch", "yaw"};
    for (int i = 0; i < 3; ++i) {
      const std::string prefix = std::string("input_") + axes[i];
      if (key == prefix + "_rate" && value >= 1 && value <= 255) { model.config.input.rate[i] = value; found = true; }
      if (key == prefix + "_srate" && value >= 0 && value <= 255) { model.config.input.superRate[i] = value; found = true; }
      if (key == prefix + "_expo" && value >= 0 && value <= 100) { model.config.input.expo[i] = value; found = true; }
      if (key == prefix + "_limit" && value >= 1 && value <= 1998) { model.config.input.rateLimit[i] = value; found = true; }
      const std::string pid = std::string("pid_") + axes[i];
      if (key == pid + "_p" && value >= 0 && value <= 255) { model.config.pid[i].P = value; found = true; }
      if (key == pid + "_i" && value >= 0 && value <= 255) { model.config.pid[i].I = value; found = true; }
      if (key == pid + "_d" && value >= 0 && value <= 255) { model.config.pid[i].D = value; found = true; }
      if (key == pid + "_f" && value >= 0 && value <= 255) { model.config.pid[i].F = value; found = true; }
    }
    if (key == "input_rate_type" && value >= 0 && value <= 4) { model.config.input.rateType = value; found = true; }
    const char* holdNames[] = {"althold_pos", "althold_vel", "poshold_pos", "poshold_vel"};
    const int holdAxes[] = {Xsfc::FC_PID_ALT, Xsfc::FC_PID_VEL, Xsfc::FC_PID_POS, Xsfc::FC_PID_POSR};
    for (int i = 0; i < 4; ++i) {
      const std::string prefix = std::string("pid_") + holdNames[i];
      auto& pid = model.config.pid[holdAxes[i]];
      if (value >= 0 && value <= 255) {
        if (key == prefix + "_p") { pid.P = value; found = true; }
        if (key == prefix + "_i") { pid.I = value; found = true; }
        if (key == prefix + "_d") { pid.D = value; found = true; }
        if (key == prefix + "_f") { pid.F = value; found = true; }
      }
    }
    auto frequency = [&](const char* name, auto& field) {
      if (key == name && value >= 0 && value <= 1900) { field = value; found = true; }
    };
    frequency("gyro_lpf_freq", model.config.gyro.filter.freq);
    frequency("gyro_lpf2_freq", model.config.gyro.filter2.freq);
    frequency("gyro_lpf3_freq", model.config.gyro.filter3.freq);
    frequency("pid_dterm_lpf_freq", model.config.dterm.filter.freq);
    frequency("pid_dterm_lpf2_freq", model.config.dterm.filter2.freq);
    if (!found) return false;
  }
  return true;
}
