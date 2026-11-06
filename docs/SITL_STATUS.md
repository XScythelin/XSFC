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
sensor/fusion; the optional receiver endpoint below executes Input/Actuator.
Neither endpoint loads complete board settings or emulates P4
instruction timing. Default settings and synthetic physics cannot validate
real aircraft PID tuning.

## Raw IMU endpoint

`--raw-imu` uses the same 12-field protocol but replaces Euler fields with
body specific force XYZ in m/s² (gyro remains rad/s). The synthetic device
quantizes/clamps to ±2000deg/s / ±16g and invokes production GyroSensor.read
and prefilter at 8kHz, filter/postLoop and PID/mixer at 4kHz,
AccelSensor.update and Fusion.update at 500Hz.
Measurements are held for eight/sixteen substeps per 1/2ms exchange; this is
not an independent 8kHz physical-sensor model. Device bus/register timing and internal
ICM42688 hardware filters are not emulated. Calibration storage is inert
and returns SAVE_ERROR, never accessing board EEPROM.

The host supplies the synthetic device clock before Model::begin, verifies
8/4/4kHz gyro/PID/mixer timer initialization, and restores normal fusion gain at gyro calibration
completion, matching SensorManager::preLoop. These are host integration fixes,
not changes to production PID or fusion algorithms.

`python sitl/host/test_schedule.py` checks actual gyro/PID/mixer call counts
per simulated second for both exchange sizes; the endpoint validates cumulative
counts at clean exit and reports them on stderr. This does not measure hardware
execution timing or validate 8kHz physical IMU bandwidth.

`python sitl/host/test_imu.py` verifies stationary level, gravity-vector tilt,
dynamic roll, freefall and 180Hz attenuation through production filters with
disarmed outputs. `XSFC_HOST_DIAGNOSTICS=1` appends Euler XYZ, scaled gyro XYZ,
filtered gyro XYZ and calibration state after the four motor pulses.
Scaled gyro is already downstream of the read-stage prefilter, not raw registers.

Webots 2ms raw closed-loop flight, raw seeded-noise + wrench-pulse flight,
ideal flight and raw throttle-cut checks pass on the provisional plant.
Interactive realtime performance and real-airframe fidelity remain unvalidated.

## Opt-in altitude endpoint and memory-only settings

`XSFC_HOST_ALTITUDE=1` requires raw IMU and appends pressure Pa, hold request
(0/1), barometer delivery healthy (0/1) to each input row. It runs production
BaroAltitude calibration/filtering at 50Hz and Control::Altitude at 500Hz;
production Controller runs the actual altitude hold logic. Missing samples age
out through production BaroFrame timeout, not an ideal-height fallback.
Diagnostics append estimated height, vario, target, engaged and baro-ready.
`test_altitude.py` checks calibration, convergence, engagement, stale exit and
disarm. This bypasses device register timing and BaroSensor's hardware driver.

`XSFC_HOST_SETTINGS` selects a text overlay (`key integer` per line) before
initialization. Supported fields are input RPY rate/srate/expo/limit, rate type,
RPY PID P/I/D/F and gyro/D-term LPF frequencies. Unknown/out-of-range fields
refuse startup; pins, calibration, storage commands and binary dumps are not
accepted. This is partial settings import, not the full production CLI/input
or actuator mode pipeline. `test_settings.py` checks effect and fail-closed behavior.

## Production receiver endpoint

`XSFC_HOST_RECEIVER=1` appends receiver delivery healthy (0/1), after optional
barometer fields. Synthetic 250Hz frames feed production Input and Actuator,
including RC smoothing, AUX ARM/Angle/AltHold and RX timeout/failsafe. Diagnostics
append mode mask, arming-disabled flags, failsafe phase and channels-valid.
The memory ESC protocol is enabled solely for host arming checks. No physical
receiver, GPIO or ESC is accessed. `test_receiver.py` checks startup gating,
arming, filtered throttle, loss detection and disarm. Webots receiver-flight
and receiver-loss pass at 2ms on the current development binary.

## Sustained acceleration blocker

Long roll/pitch holds expose substantial production Mahony estimate divergence:
Webots truth roll ~1.122rad vs estimate ~0.294rad at 7s. The ideal comparison
recovers. A host-only known-motion probe also converges with rotated gravity but
loses tilt with thrust-axis specific force. This is not a camera-only fault.
Accelerometer gravity correction under sustained translation is implicated;
real-board reproduction remains unverified. Production Mahony now rejects large
gravity innovations while armed and disables false integral bias learning.
Signed host trajectories, slow ramp, disarmed reconvergence and initial Webots
long-tilt comparisons pass. See `docs/FUSION_ACCELERATION.md` for remaining drift
and observability limitations; this is not hardware flight validation.
