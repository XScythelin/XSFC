# Mahony acceleration rejection (development)

The accelerometer measures specific force, not gravity alone. Sustained
translation can keep specific force near body +Z while the craft is tilted.
The previous always-on gravity correction erased the gyro-integrated tilt and
integrated a false gyro bias. This was reproduced with known host trajectories
and Webots long roll/pitch holds.

The production Mahony path now rejects accelerometer correction **while armed**
when its magnitude is outside 0.9..1.1g, or its direction differs from predicted
gravity by more than 2 degrees. It clears/disables Mahony's integral feedback
while armed; the calibrated gyro bias remains in GyroSensor. On disarm the
configured integral gain is restored and normal gravity convergence resumes.
Startup calibration gains are preserved until normal restoreGain is called.

This is an explicit conservative heuristic, not a measurement of acceleration
and not a complete navigation estimator. No RC target, simulator truth, velocity
or motor model enters the correction. Small/slow acceleration can remain
indistinguishable from gravity; gyro bias can drift and exceed the innovation
gate, preventing in-flight convergence. Magnetometer-aided flight and noisy
hardware still require validation. Non-Mahony fusion modes are unchanged.
The current combined magnetometer update rejects the whole observation when
acceleration is rejected, so heading aiding is also temporarily unavailable.

`python sitl/host/test_fusion_acceleration.py` checks signed roll/pitch rotation,
six-second sustained thrust-axis specific force, gyro-driven recovery, gravity
reconvergence after disarm, and a slow-ramp case. Webots must also pass raw long
tilt both with and without drag, plus disturbed, receiver and AltHold regressions.
Passing these cases does not establish real-aircraft safety or bounded long-term
drift. Do not flash or fly based solely on these SITL checks.
