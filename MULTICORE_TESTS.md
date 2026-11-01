# multicore_tests — cleanup and SPI preparation

## Current implementation

Latest user-requested SPI experiment: P4 SPI gyro cap8000Hz, I2C cap2000Hz.
User confirmed working SPI at4kHz with saved MOSI29/MISO30/SCK28/CS32.
P4 defaults are now pid_sync2/mixer_sync1: SPI gyro8kHz, PID/mixer4kHz.
Saved dividers are preserved. PID8kHz remains opt-in via pid_sync1, but user
reports near100% telemetry errors even with mixer_sync2; PID4kHz gave~1%.
No verified fix for PID8kHz bidirectional telemetry is implemented.
With pid_sync1/mixer_sync1, SPI gyro/PID/mixer target8kHz (125us period).
Validate actual rates with stats, plus ESC RPM/error behavior; CPU work time
alone does not establish bidirectional DShot wire timing. Keep propellers off.
To reduce PID/mixer only, use pid_sync2; restoring gyro4kHz requires reverting
the SPI cap and rebuilding or flashing the previous image.

P4 4kHz I2C experiment cancelled: gyro cap restored to2000Hz.
I2C ceiling800kHz is retained.
The existing default i2c_speed is800; saved values remain effective.
For the I2C test use `set i2c_speed 800`, `set pid_sync 1`,
`set mixer_sync 1`, then `save`. Verify actual rates with `stats` and
requested/actual bus speed in startup logs. This is not hardware validation.
With pid_sync1/mixer_sync1, gyro/PID/mixer now run at up to2kHz.
Reverting the remaining bus experiment requires restoring the ceiling400kHz
or setting i2c_speed400. Do not change filters or enable new diagnostics.

- ESP32-P4 stays at 360MHz. PID polling, queue, stats and mixer scheduling
  are restored to main. Experimental PID event notifications are removed.
- Analysis-only cpustats, FlightTiming, event timestamps, Wire profiling,
  section maxima and additional atomic accumulation are removed.
- Baseline stats/CPU status telemetry, ibatinfo and checked IMU reads remain.
- ICM42688 reads 12 bytes (accel+gyro) when accel is due, otherwise 6 gyro
  bytes. Accel cache is single-use; short reads cannot publish partial vectors.
- Other IMU drivers, filter coefficients, I2C speed and motor pins are unchanged.

## Hardware status

User reported intermittent startup/other ESC sounds. Actual power reset,
DShot dropout and beacon have not been distinguished. Restoring scheduling
and removing intrusive diagnostics is mitigation, not a proven repair.
The existing disarmed MODE_BUZZER beacon path was not changed by experiments
and is retained; do not silently disable it to conceal sounds.

Earlier combined-read measurements reduced CPU1 section time from ~78–79%
to ~72%, but increased average acquisition-to-output return from ~375us to
~412us. Combined reads are retained at user request. These are wall-time
measurements, not wire-completion or DRDY measurements. Full cpustats output
itself stalled serial servicing for ~3.36ms; that command is now removed.
Delayed I2C NACK remains unresolved.

## Bench check (no propellers)

1. Back up settings. Flash `.pio/build/esp32p4/firmware_0x00.bin` at `0x0`.
2. Keep disarmed; check `status`, `devinfo`, `escinfo`, then let it run without
   CLI traffic. Confirm BUZZER mode is not active for the comparison.
3. Record any recurring ESC sounds and whether all ESCs and/or the FC restart.
   Compare with a previously verified main image if the symptom remains.
4. Do not flight-test until the reported regression is resolved. Native tests
   and builds cannot validate ESC signals or electrical stability.

## SPI preparation — not switched on hardware

P4 defaults: SCK GPIO28, MOSI GPIO30, MISO GPIO29, gyro CS GPIO32.
ICM42688 explicit-CS detection already exists. BusSPI uses mode0, 1MHz normal
register access and 16MHz fast reads; SPI reports requested length without
physical transfer-error detection. Successful mock reads do not prove wiring.

Confirm the exact module/photo, SPI-capable pin labels, any mode jumpers and
actual wiring before changing saved settings. At power off, module SCL/SCLK
goes to SCK, SDA/SDI to MOSI, SDO to MISO and CS to CS; use the module's
documented supply/IO voltage and common ground. Do not apply 5V to P4 IO.
Do not guess module jumper changes or repurpose motor GPIO33/49/50/52.
Keep initial loop rates unchanged to isolate interface from scheduling changes.
