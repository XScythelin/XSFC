#pragma once

// Stop the active gyro timer before entering blocking ESC passthrough.
#if defined(XSFC_MULTI_CORE) && defined(XSFC_FREE_RTOS)
void gyroTimerStop();
#endif
