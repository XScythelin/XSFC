#pragma once
#include "Device/InputDevice.h"
#include <algorithm>

class InputSITL : public Xsfc::Device::InputDevice {
public:
  uint16_t channels[16]{};
  bool connected = true;
  bool fresh = false;
  Xsfc::InputStatus update() override {
    // No receiver packets: preserve frameTime so production timeout advances.
    if (!connected) return Xsfc::INPUT_IDLE;
    if (!fresh) return Xsfc::INPUT_IDLE;
    fresh = false;
    return Xsfc::INPUT_RECEIVED;
  }
  uint16_t get(uint8_t channel) const override { return channel < 16 ? channels[channel] : 1500; }
  void get(uint16_t* data, size_t len) const override {
    for (size_t i = 0; i < len; ++i) data[i] = i < 16 ? channels[i] : 1500;
  }
  size_t getChannelCount() const override { return 16; }
  bool needAverage() const override { return false; }
};
