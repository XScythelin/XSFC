#pragma once

// Host target: no GPIO, buses, serial devices or physical ESC output.
#include <Arduino.h>
#define XSFC_OUTPUT_COUNT 4
#define XSFC_OUTPUT_0 -1
#define XSFC_OUTPUT_1 -1
#define XSFC_OUTPUT_2 -1
#define XSFC_OUTPUT_3 -1
#define XSFC_BUTTON_PIN -1
#define XSFC_BUZZER_PIN -1
#define XSFC_LED_PIN -1
#define XSFC_FEATURE_MASK 0
#define XSFC_GYRO_SPI_RATE_MAX 8000
#define XSFC_GYRO_I2C_RATE_MAX 1000
#define XSFC_SERIAL_USB
#define XSFC_SERIAL_USB_FN SERIAL_FUNCTION_MSP
#define XSFC_ATOMIC_QUEUE
#define FAST_CODE_ATTR
#define F_CPU 360000000L
