#pragma once

#include "EscDriverBase.hpp"

#if defined(XSFC_SITL)
  #define ESC_CHANNEL_COUNT 4
  #include "EscDriverSITL.h"
  #define EscDriver EscDriverSITL
  #define ESC_DRIVER_MOTOR_TIMER 0
  #define ESC_DRIVER_SERVO_TIMER 1
#elif defined(ESP32S3) || defined(ESP32P4)

  #define ESC_CHANNEL_COUNT 4
  #include "EscDriverEsp32.h"
  #define EscDriver EscDriverEsp32

  #define ESC_DRIVER_MOTOR_TIMER 0
  #define ESC_DRIVER_SERVO_TIMER 1

#elif defined(ESP32)

  #define ESC_CHANNEL_COUNT RMT_CHANNEL_MAX
  #include "EscDriverEsp32.h"
  #define EscDriver EscDriverEsp32

  #define ESC_DRIVER_MOTOR_TIMER 0
  #define ESC_DRIVER_SERVO_TIMER 1

#else

  #error "Unsupported platform"

#endif
