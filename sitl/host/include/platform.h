#pragma once
#include <EscDriver.h>
// Firmware ESC registry hook; host outputs remain exclusively in memory.
inline void motorInitEscDevice(EscDriver*) {}
