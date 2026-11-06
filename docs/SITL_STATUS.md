# Host target and simulation development

The `XSFC_SITL` target compiles production Model, Controller, Rates, PID,
Mixer and filter sources with host compatibility headers in `sitl/host/include`.
The ESC endpoint stores four output pulses in memory; it performs no hardware I/O.
Normal ESP32 target selection is unchanged when `XSFC_SITL` is not defined.

Build: `python sitl/host/build.py` (Windows Python and WSL g++).
Check: `python sitl/host/test_bridge.py`.
`XSFC_HOST_BUILD_DIR` optionally sets the absolute output directory.

The visual environment and USB adapter are maintained separately:
https://github.com/XScythelin/XSFC-Simulation
That repository pins this firmware through a git submodule; do not duplicate
flight-control algorithms in the simulator.

Current host bridge accepts ideal gyro, ideal Euler attitude, normalized RC,
virtual arm and Angle/Acro selection. It does NOT yet execute the production
sensor/fusion/input/mode-manager pipeline, load board settings or emulate P4
instruction timing. Default settings and synthetic physics cannot validate
real aircraft PID tuning. Sensor pipeline integration is the next milestone.

## Raw IMU endpoint

`--raw-imu` uses the same 12-field protocol but replaces Euler fields with
body specific force XYZ in m/s² (gyro remains rad/s). The synthetic device
quantizes/clamps to ±2000deg/s / ±16g and invokes production GyroSensor.read,
filter/postLoop at 4kHz, AccelSensor.update and Fusion.update at 500Hz.
Measurements are currently held for four substeps per 1ms exchange; this is
not a 4kHz physical-sensor model. Device bus/register timing and internal
ICM42688 hardware filters are not emulated. Calibration storage is inert
and returns SAVE_ERROR, never accessing board EEPROM.

`python sitl/host/test_imu.py` verifies stationary level and gravity-vector
tilt convergence with disarmed outputs. Webots still uses the ideal bridge
until its frame, startup calibration and closed-loop tests are validated.
