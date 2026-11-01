#pragma once

#include "Esp.h"
#include "Debug_Xsfc.h"

// DFRobot FireBeetle 2 ESP32-P4 V1.0 (DFR1172), header GPIO numbering.
// SD, microphone, C6 radio, camera and display are not initialized.
#define XSFC_INPUT
#define XSFC_INPUT_PIN 36

#define XSFC_OUTPUT_COUNT 4
#define XSFC_OUTPUT_0 33
#define XSFC_OUTPUT_1 49
#define XSFC_OUTPUT_2 50
#define XSFC_OUTPUT_3 52

#define XSFC_SERIAL_0
#define XSFC_SERIAL_0_DEV Serial0
#define XSFC_SERIAL_0_DEV_T HardwareSerial
#define XSFC_SERIAL_0_TX 37
#define XSFC_SERIAL_0_RX 38
#define XSFC_SERIAL_0_FN (SERIAL_FUNCTION_MSP)
#define XSFC_SERIAL_0_BAUD (SERIAL_SPEED_115200)
#define XSFC_SERIAL_0_BBAUD (SERIAL_SPEED_NONE)

#define XSFC_SERIAL_1
#define XSFC_SERIAL_1_DEV Serial1
#define XSFC_SERIAL_1_DEV_T HardwareSerial
#define XSFC_SERIAL_1_TX 4
#define XSFC_SERIAL_1_RX 5
#define XSFC_SERIAL_1_FN (SERIAL_FUNCTION_MSP)
#define XSFC_SERIAL_1_BAUD (SERIAL_SPEED_115200)
#define XSFC_SERIAL_1_BBAUD (SERIAL_SPEED_NONE)

#define XSFC_SERIAL_2
#define XSFC_SERIAL_2_DEV Serial2
#define XSFC_SERIAL_2_DEV_T HardwareSerial
#define XSFC_SERIAL_2_TX 22
#define XSFC_SERIAL_2_RX 23
#define XSFC_SERIAL_2_FN (SERIAL_FUNCTION_RX_SERIAL)
#define XSFC_SERIAL_2_BAUD (SERIAL_SPEED_115200)
#define XSFC_SERIAL_2_BBAUD (SERIAL_SPEED_NONE)

#define XSFC_SERIAL_3
#define XSFC_SERIAL_3_DEV Serial3
#define XSFC_SERIAL_3_DEV_T HardwareSerial
#define XSFC_SERIAL_3_TX -1
#define XSFC_SERIAL_3_RX -1
#define XSFC_SERIAL_3_FN (SERIAL_FUNCTION_NONE)
#define XSFC_SERIAL_3_BAUD (SERIAL_SPEED_115200)
#define XSFC_SERIAL_3_BBAUD (SERIAL_SPEED_NONE)

#define XSFC_SERIAL_4
#define XSFC_SERIAL_4_DEV Serial4
#define XSFC_SERIAL_4_DEV_T HardwareSerial
#define XSFC_SERIAL_4_TX -1
#define XSFC_SERIAL_4_RX -1
#define XSFC_SERIAL_4_FN (SERIAL_FUNCTION_NONE)
#define XSFC_SERIAL_4_BAUD (SERIAL_SPEED_115200)
#define XSFC_SERIAL_4_BBAUD (SERIAL_SPEED_NONE)

#define XSFC_SERIAL_USB
#define XSFC_SERIAL_USB_DEV Serial
#define XSFC_SERIAL_USB_DEV_T HWCDC
#define XSFC_SERIAL_USB_FN (SERIAL_FUNCTION_MSP)
#define XSFC_SERIAL_REMAP_PINS
#define XSFC_SERIAL_DEBUG_PORT SERIAL_USB
#define SERIAL_TX_FIFO_SIZE 0xFF

#define XSFC_SPI_0
#define XSFC_SPI_0_DEV SPI1
#define XSFC_SPI_0_SCK 28
#define XSFC_SPI_0_MOSI 30
#define XSFC_SPI_0_MISO 29
#define XSFC_SPI_CS_GYRO 32
#define XSFC_SPI_CS_BARO -1 // GPIO33 is motor 1; assign another CS for an SPI barometer.

// Use hardware I2C: the bit-banged driver has Xtensa-specific timing.
#define XSFC_I2C_0
#define XSFC_I2C_0_SCL 8
#define XSFC_I2C_0_SDA 7

#define XSFC_BUZZER_PIN 34
#define XSFC_BUTTON_PIN 35
#define XSFC_LED_PIN 3
#define XSFC_ADC_0
#define XSFC_ADC_0_PIN 20
#define XSFC_ADC_1
#define XSFC_ADC_1_PIN 21
#define XSFC_ADC_SCALE (3.3f / 4096)
#define XSFC_FEATURE_MASK (FEATURE_RX_SERIAL | FEATURE_DYNAMIC_FILTER)
#define XSFC_GYRO_I2C_RATE_MAX 2000
#define XSFC_GYRO_SPI_RATE_MAX 4000
#define XSFC_DSHOT_TELEMETRY
#define XSFC_FREE_RTOS
#ifndef CONFIG_FREERTOS_UNICORE
  #define XSFC_MULTI_CORE
#endif
#define XSFC_ATOMIC_QUEUE

#include "Device/SerialDevice.h"
#include "Target/TargetEsp32Common.h"

namespace Xsfc {

template<>
inline int targetSerialInit(HWCDC& dev, const SerialDeviceConfig& conf)
{
  dev.begin(conf.baud);
  return 1;
}

}
