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
virtual arm and Angle/Acro selection. The default ideal endpoint bypasses
sensor/fusion; neither endpoint executes the full input/mode-manager pipeline,
loads board settings or emulates P4
instruction timing. Default settings and synthetic physics cannot validate
real aircraft PID tuning.

## Raw IMU endpoint

`--raw-imu` uses the same 12-field protocol but replaces Euler fields with
body specific force XYZ in m/s² (gyro remains rad/s). The synthetic device
quantizes/clamps to ±2000deg/s / ±16g and invokes production GyroSensor.read,
filter/postLoop at 4kHz, AccelSensor.update and Fusion.update at 500Hz.
Measurements are held for four/eight substeps per 1/2ms exchange; this is
not a 4kHz physical-sensor model. Device bus/register timing and internal
ICM42688 hardware filters are not emulated. Calibration storage is inert
and returns SAVE_ERROR, never accessing board EEPROM.

The host supplies the synthetic device clock before Model::begin, verifies
4kHz timer initialization, and restores normal fusion gain at gyro calibration
completion, matching SensorManager::preLoop. These are host integration fixes,
not changes to production PID or fusion algorithms.

`python sitl/host/test_imu.py` verifies stationary level, gravity-vector tilt,
dynamic roll, freefall and 180Hz attenuation through production filters with
disarmed outputs. `XSFC_HOST_DIAGNOSTICS=1` appends Euler XYZ, scaled gyro XYZ,
filtered gyro XYZ and calibration state after the four motor pulses.
Scaled gyro is already downstream of the read-stage prefilter, not raw registers.

Webots 2ms raw closed-loop flight, raw seeded-noise + wrench-pulse flight,
ideal flight and raw throttle-cut checks pass on the provisional plant.
Interactive realtime performance and real-airframe fidelity remain unvalidated.
