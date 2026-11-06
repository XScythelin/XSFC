#include "Hal/Gpio.h"
namespace Xsfc::Hal {
void Gpio::digitalWrite(uint8_t, pin_status_t) {}
pin_status_t Gpio::digitalRead(uint8_t) { return 0; }
void Gpio::pinMode(uint8_t, pin_mode_t) {}
}
