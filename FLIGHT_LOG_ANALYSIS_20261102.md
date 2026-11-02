# Flight logs, 2026-11-02

Inputs: BTFL_BLACKBOX_LOG_20261102_143418.BBL,
BTFL_BLACKBOX_LOG_20261102_143951.BBL and BTFL_cli_20261102_144159.txt.
Parsed with configurator FlightLogParser starting at the end of the header;
parsing from byte zero produces bogus frames and must not be used.

| Measurement | First flight | Second flight |
|---|---:|---:|
| Valid main frames / invalid | 14094 / 0 | 5775 / 0 |
| Recorded span | 18.427257 s | 6.916792 s |
| Nominal PID interval | 250 us | 250 us |
| Recorded iteration step | 4 | 4 |
| Median frame time delta | 1250 us | 1000 us |
| p99 / maximum frame delta | 2508 / 2753 us | 2751 / 3000 us |
| Iterations / elapsed time | 3059 Hz | 3339 Hz |
| Bidirectional DShot header | enabled | disabled |
| RPM filter harmonics | 3 | 3 |
| Debug mode | NONE | NONE |
| Absolute roll D p95 / p99 | 157 / 247 | 326 / 461 |
| Absolute pitch D p95 / p99 | 124 / 219 | 425 / 545 |

The iteration counter is consecutive at the configured logging stride: elapsed
time is excessive without missing main frames. This establishes a timing
shortfall, not which task caused it. Header loop time is nominal, not measured.
PID/filter math uses configured rates, so this shortfall is relevant to control.

Both logs have invalid D-term frequencies. First header: static LPFs 59395,
dynamic 56323..56325. Second: all four 56325; current CLI dump prints -9211
(same uint16 bits). FilterConfig::sanitize clamps negative Hz to zero and
disables the filter. Controller::beginInnerLoop uses those configurations.
Gyro LPF1/2 and dynamic LPF are also zero; FO150 and notches are separate.
Large D contributions and motor-command changes are present, but cannot alone
identify mechanical resonance, ESC desync or actual winding temperatures.

Second-flight symptoms with bidirectional mode disabled exclude RX telemetry
as the sole explanation; they do not exonerate the RMT TX path. The logs contain
commanded motor values and last available eRPM, not ESC waveform captures or
telemetry-validity counters. Radio commands are processed, not raw CRSF frames.

BlackboxFlashfs.cpp calls esp_partition_write_raw synchronously from its
misleadingly named flashfsFlushAsync. This is a concrete blocking path and a
timing suspect; causal contribution needs prop-free comparison with logging
disabled and enabled. A driver replacement is not established as the remedy.

Local fixes: CLI now dumps serialrx_provider (previously absent, so defaults
restored SBUS); XSFC download filenames use board identity, retaining BTFL wire
variant; filter MSP writes reject signed-overflow frequency values before
mutation, and configurator refuses invalid Hz. Origin of existing bad values
is not yet reproduced. Existing stored bad configuration is NOT auto-repaired.

ARM: both supplied dumps contain exactly one active ARM and one ANGLE condition;
remaining ARM slots are empty 900..900. Added decoder regression reproduces
this without an extra ARM entry. The reported OR duplication is not reproduced;
do not claim a fix or change persistent mode IDs on this evidence.

No further flight until invalid filter configuration is repaired and read back,
motors inspected, and timing/DShot checked without props. No flash reset or
automatic tuning performed. Hardware save/restore tests remain outstanding.

## Third flight: XSFC_BLACKBOX_LOG_20261102_150136.BBL

Current dump XSFC_cli_20261102_150323.txt confirms gyro/PID/mixer2k,
D-term128/128 with dynamic60..145, RPM0, bidirectional DShot0, logging index1.
22575 valid frames, zero invalid, 27.090 s, iteration stride2 throughout.
20316 frame intervals about1ms and2258 about3ms; every long interval is
exactly10 frames after the previous long interval. Effective iteration rate
45148/27.09 = 1666.6 Hz, not nominal2000. This very regular pattern is not
random parser loss or proof of flash causation; scheduling/flash require isolation.

Absolute D p95 roll/pitch19/15, p99 31/26, maxima217/469. Strong reduction
versus previous log p95 326/425, but multiple things changed concurrently:
restored filters, lower rate, RPM disabled. Cannot attribute improvement to one.

New concern: accSmooth norm (header1g=2048), nominally processed accel,
has median about2.94..3.59g over airborne-looking windows2..24s and p95
5.81..6.48g. Start/end are near1g. These values cannot be accepted as a clean
gravity reference; vibration/aliasing, sensor read correctness, logging coherence
and mechanical mounting need checking. Do not diagnose calibration or SPI faults
from this alone. Mahony consumes state.accel.adc, also the source of accSmooth.
Attitude Euler is not logged with debug NONE; cannot demonstrate false tilt
or a particular jerk's cause. At13.098s from first frame, RCroll=-23,
RCpitch=0 but rate setpoint roll=-61,pitch=-15, with very large accel values.
Angle rate setpoints can be nonzero at centered sticks by design.

Processed RC increments at most20/9/6 units per recorded sample (R/P/Y),
not evidence of a full-scale single-frame stick glitch; raw radio loss is not
observable. Sustained roll input/correction is present; its cause is unknown.
Largest pitch gyro excursion at26.173s may be landing/contact and must be
matched to pilot timing before treating it as an airborne fault.

No PID/fusion tuning or new runtime instrumentation added for this flight.
Ask pilot jerk timing/direction, whether roll was held to oppose drift, and
whether26.17s was landing. First verify stationary level/motor-off attitude,
receiver centers and prop-free sensor behavior. Do not infer position hold:
dump enables ANGLE, not GPS/optical-flow position hold.
# Follow-up implementation: accel ownership and logging

P4 storage mode is now selectable and included in `dump all`:
`set blackbox_flash_mode PSRAM` (default) or `set blackbox_flash_mode DIRECT`,
then `save` and reboot. The mode is latched at boot, not switched mid-log.
EEPROM v9 migrates v8 without losing gyroRate or the earlier configuration.
DIRECT restores synchronous flash writes during recording and does not reserve
the partition-sized PSRAM buffer or use the deferred-save arming interlock.
It can retain the written part of a recording after a power cut, but can stall
flight control. Neither mode has been hardware-validated in this image.

After a successful PSRAM commit (data and journal), ESCs receive DShot BEEP1,
BEEP2, BEEP3 commands at 500ms intervals, disarmed with stopped motors only.
Failure, startup repair and DIRECT do not emit this completion sequence.
Actual sound/pitch depends on ESC firmware; verify audibility with props removed.
The melody is automatic and introduces no additional configurable parameter.

The latest two supplied logs (15:20:58 and 15:24:58) also decode without invalid
main frames. Effective PID rates are approximately 1782 and 1667 Hz, versus
nominal 2000 Hz. In the latter log the accel norm median is 3.09g and p95 7.0g.
I frames and P frames both show this, so average-predictor drift alone does not
explain it (I median 3.076g, P median 3.087g).

A concrete cross-core race was found: CPU1 exposed raw/aligned/partially filtered
`accel.adc` while CPU0 used it for attitude fusion and Blackbox. The new path
copies raw XYZ into the event queue and performs accel filtering on CPU0 before
fusion. The magnitude-based Mahony guard has been removed, as requested.
This fixes the identified ownership bug, but does not establish that it caused
all the observed high acceleration; vibration, aliasing and hardware integrity
still need measurement on the board.

P4 Blackbox now stages writes in PSRAM and commits data, then journal, only
after log close while disarmed and with motors stopped. EOF close defers commit
until disarm. Arming is blocked with LOAD while save is pending. Allocation
failure disables flash logging rather than falling back to in-flight writes.
Reading returns the coherent PSRAM image, including existing saved logs.
**Power loss before commit loses the staged recording.** Saving may take seconds;
do not unplug before it completes. Failed saves keep the arming interlock set.
Removing synchronous flash programming from the active logging path addresses
a known source of stalls; it is not proof that all periodic 3ms intervals are
gone. This requires a prop-free active-log timing test and downloaded-log review.

P4 defaults now match the latest working 2kHz baseline, with the subsequently
reported bidirectional DShot600 setting. Calibration offsets remain board-specific.
CLI registry coverage was expanded to masks, receiver checks/provider, all PID
slots and filter components. Source coverage checks are not a substitute for
dump -> defaults -> restore -> save/reboot -> dump comparison on real hardware.

Validation: P4 release/merged-image build passed; 10 Python contract/layout/pin
tests passed; immutable accel event and ICM42688 native tests passed with
ASan/UBSan. No flight validation of this image has been performed.

## Flight 22:39:21: clean-runtime 2 kHz firmware

Input: `XSFC_BLACKBOX_LOG_20261102_223921.BBL` (792,960 bytes), firmware
date `Nov 2 2026 22:31:57`. User reports continued drift, incorrect ANGLE
horizon and forward motion at centered sticks. This flight supersedes the
earlier statement that no flight log exists for the clean-runtime image.

- 27,031 valid main frames, zero parser-reported invalid frames, 27.030 s,
  exactly 2,000 iterations/s, logging stride 2. Most logged intervals are
  1,000 us (23,448); a valid log-end event is present. The previous gross
  loop-rate shortfall is not present in this recording; this is not a bound
  on unlogged individual 500-us intervals.
- `acc_lpf_hz:1500` is NOT evidence of a 1,500-Hz active accel filter:
  `lib/betaflight/src/blackbox/blackbox.c` writes this field as Hz * 100.
  The bridge supplies 15, and post-flight read-only `dump all` confirms
  `BIQUAD`, 15 Hz, cutoff 0, Mahony P50/I5, trims 0/0 and board alignment 0/0/0.
  Do not change the header encoding based on this apparent discrepancy.
- During t=2..26 s (relative to first main frame), processed acceleration
  norm median = 3.265 g, p95 = 6.170 g, maximum = 11.844 g. Mean XYZ =
  [0.1266, 0.0180, 0.8558] g. This is the post-filter, post-calibration
  `accel.adc` used by fusion, not an unfiltered vibration/debug channel.
- t=0..1 s: norm median 1.029 g; t=26.5..27.03 s: median 1.000 g,
  mean XYZ [0.0022, -0.0052, 1.0148] g. Constant gross scaling error is
  therefore not supported. Stationary scale does not validate loaded reads.
- Hann-windowed FFT of t=2..26 s at logged 1 kHz: only 0.113%, 0.118%,
  0.130% of demeaned XYZ power lies above 50 Hz. Large variations are already
  in/near the attitude correction band. A software 15-Hz LPF cannot remove
  vibration that has already aliased into low frequencies before that LPF.
  This supports investigation of sampling/AAF and loaded sensor integrity;
  it does NOT establish aliasing or mechanical vibration as the root cause.
- Adjacent accel vectors repeat in 50.16% of main-frame transitions, consistent
  with a 500-Hz accel update logged at 1 kHz. Iteration phases modulo 4 = 0
  and 2 have identical norm median/p95. This does not look like isolated
  broken predictor frames; independent raw data is still unavailable.
- Pitch RC over t=2..26 s: mean -128.59, median -121, range -260..0;
  roll mean 0.05, median 0. Pitch gyro median 0 deg/s, absolute p95 35 deg/s;
  pitch D absolute p95 5 (logged PID units). These are not grounds to retune
  D/P blindly. Negative pitch is consistent with pilot correction, but the
  pilot's intent, physical pitch and estimator error cannot be recovered
  uniquely: Euler attitude is not logged with debug NONE.

Post-flight USB-only checks (no ARM, motors, calibration or settings save):
fresh 411-setting backup `xsfc-postflight-223921-settings.txt`; 100 read-only
MSP samples saved in `xsfc-postflight-stationary.json` in approved temp storage.
Median RAW_IMU accel [27,8,2050] at 2048/g, gyro [0,0,0]; attitude
[0.2,-0.8,-1] degrees (roll/pitch/yaw); primary RC [1500,1500,1500,1000].
Physical board level and live transmitter validity were not established, so
these are NOT a level calibration or an in-flight RC-centering validation.

Code trace: ICM42688 runs accel at 8 kHz; only due 500-Hz accel snapshots are
read/published. FIR2 and BIQUAD15 operate AFTER this reduction to 500 Hz.
The driver leaves AAF/UI filter registers at reset values; `setDLPFMode` is a
no-op. Thus `gyro_dlpf AUTO` does not configure those hardware filters.
The previously read Bank2 0x03=0x30 alone is insufficient to verify the complete
accel AAF response (Bank2 0x04/0x05 also matter). Do not claim hardware filters
are disabled. Reference INAV explicitly programs all three accel AAF registers.

Conclusion: timing improved, but the loaded processed-acceleration problem
survives the queue ownership fix. Bad accel direction can drive Mahony's
gravity correction and is a credible explanation for wrong ANGLE horizon,
not yet proof of the exact forward-drift mechanism. No blind trim, calibration,
PID, fusion-gain or magnitude-guard change made. Next discriminating test needs
raw loaded accel together with estimated attitude and/or verified sensor AAF
configuration; changing filters without that would mix diagnosis and tuning.
ANGLE is not position hold, but that does not dismiss the reported wrong horizon.

## Flight 23:05:39: accel hardware-filter change

Input `XSFC_BLACKBOX_LOG_20261102_230539.BBL`. User reports best flight so
far, initially vertical/level takeoff for several seconds, still not ideal.
39,700 valid main frames, zero invalid, span39.699s, effective iteration
rate exactly2000Hz, stride2, valid log-end event. Main fields still omit Euler.
No further settings or firmware changes made during this analysis.

Comparison uses t2..26s for old log and t2..38s for new log, excluding the
obvious final landing/throttle-reduction section in each. These are separate
pilot flights, NOT controlled identical trajectories or proof of causality.

| Metric | 22:39 before | 23:05 after |
|---|---:|---:|
| Processed accel norm median | 3.265g | 1.036g |
| Norm p95 | 6.170g | 1.670g |
| Norm maximum in compared window | 11.844g | 2.366g |
| Horizontal accel RMS sqrt(mean(X²+Y²)) | 2.779g | 0.367g |
| XYZ mean, g | 0.1266/0.0180/0.8558 | -0.0063/0.0081/1.0024 |
| XYZ standard deviation, g | 1.374/2.412/2.360 | 0.187/0.316/0.352 |
| Mean absolute pitch RC, logged units | 128.59 | 21.86 |
| Roll+pitch both within5 RC units | 0.32% | 34.35% |
| Roll/pitch rate-error RMS, deg/s | 7.39/6.88 | 5.15/5.30 |

Horizontal RMS reduced7.57x, mean absolute pitch correction5.88x. Especially
relevant to reported takeoff: new t2..6s mean absolute RC roll4.509/pitch0.0385,
horizontal accel RMS0.309g; old matching t2..6s roll24.62/pitch94.53,
horizontal RMS2.466g. New t6..12s RC absolute means31.58/7.51; t12..26s
33.83/21.26; t26..38s36.85/37.00. There is no return to the old multi-g
accel corruption across these windows, although pilot corrections grow.

Important limitations: new norm p5..p95 =0.575..1.670g, so the processed signal
is substantially improved but not quiet. A6.62g peak occurs in final t38..39.699s
landing/throttle-reduction window; exclude it from steady-flight maxima, do not
silently report2.37g as whole-log max. No single dominant residual spectral
line established (broad low-frequency content). No actual attitude/position or
raw loaded accel logged, so residual drift cannot be assigned uniquely to
Mahony, physical acceleration/vibration, calibration or pilot input.

Conclusion: flight report and independent logged acceleration/RC changes agree
with a large improvement after pre-decimation accel filtering. Strong support
for the signal-path intervention, not proof that aliasing alone was the cause
or that horizon/memory issues are solved. Preserve this working firmware and
avoid blind PID/trim/filter changes. For residual horizon diagnosis, use an
explicitly selected existing attitude debug mode in a future authorized test
(DEBUG_AC_CORRECTION logs Euler), rather than restoring always-on probes.

## Flight 23:15:01: estimated attitude recorded

Input `XSFC_BLACKBOX_LOG_20261102_231501.BBL`. No pilot description supplied
yet. 32,025 valid main frames, zero invalid,32.024s, exactly2000 iterations/s,
stride2 and valid end event. Debug mode48=AC_CORRECTION; debug[0..2] contain
estimated Euler in0.1deg, debug[3..7] all zero. Logged dt range982..1018us,
predominantly alternating993/1007 and992/1008us. This small timestamp pattern
does not establish individual PID interval jitter or missed iterations.

Using t2..30s to exclude initial takeoff/final throttle reduction:
- Accel norm p5/median/p95/max=0.593/1.090/1.699/2.589g;
  meanXYZ=[-0.0032,-0.0018,1.0070]g, horizontalRMS0.450g. Improved
  signal regime persists; still substantial residual fluctuations.
- Estimated roll range -7.1..10.0deg, pitch -12.5..10.8deg;
  abs p95 roll4.7/pitch7.1deg. These include pilot-commanded tilts, NOT
  measurements of estimator error.
- Both RP RC within5 units for11,316 frames (~40.4% window); in that subset
  median estimated roll/pitch=-0.2/-0.2deg, abs p95=2.7/4.0deg. Centered
  samples include return transients, so do not call these steady-state errors.
- With the relevant RC exactly0 and |estimated angle|>=0.5deg, rate setpoint
  opposes estimated angle in99.75% roll(n7610)/99.85% pitch(n7811).
  Median -rateSetpoint/angle=4.444/s on both axes, consistent with level P.
  No evidence here for a reversed ANGLE feedback sign.
- Contiguous centered RP examples: t8.085..9.119s pitch -9.2->-0.9deg;
  t19.391..20.418s roll -3.4->-0.1deg; t24.904..25.791s pitch -4.0->-0.5deg;
  t12.737..13.617s RP[2.7,-1.7]->[0.1,-0.2]deg. Estimated attitude visibly
  returns toward zero; physical attitude/position is not independently known.
- Mean absolute RC roll14.13/pitch18.72; rate error RMS3.44/4.04deg/s.

No PID/filter/trim/calibration/settings changes and no board operations made
for this analysis. Need pilot description: whether/when real craft was tilted
while indicating level, drift direction and whether drift accelerates or just
persists. ANGLE levels attitude but does not brake existing horizontal velocity.
This distinction cannot be settled from gyro/accel/Euler alone. Do not infer
that centered estimated angles prove a true physical level or safe firmware.

### Further software audit after pilot reports intermittent forward horizon shift

Verified multicore ownership/path: gyro event filters its immutable sample;
accel event filters its immutable sample then calls fusion exactly once. Mahony
`begin` gets500Hz from accel timer and uses1/500s. Gyro attitude input is in
radians/s, with a PT1 at166Hz updated at2kHz. No simple4x integration-time or
degrees/radians mismatch found. Fusion gain restoration at end of gyro bias
sampling sets `twoKp=1`, `twoKi=0.1` for configured50/5. Do not interpret
Mahony setter parameter as doubled automatically: setters assign it directly.

Added reproducible offline `test/imu/analyze_attitude.py`: reconstructed P
feedback = twoKp *0.5*(normalizedAccel cross estimatedGravity), converted to
deg/s. It approximates gravity from rounded Euler; it is not a replay and
does not recover integralFB. Body gyro is converted to Euler rates before
integration, but logged gyro is rounded and precedes attitude PT1, so residuals
are estimates, not ground-truth attitude errors.

Example t10..12s: observed Euler change minus integrated logged gyro is
[+3.44,+4.40]deg roll/pitch; reconstructed mean P correction is
[+1.30,+2.04]deg/s. Magnitude/sign are consistent with substantial accel-driven
correction. Over t2..30s reconstructed instantaneous absolute P p95 is about
19.4deg/s roll and13.6deg/s pitch. Individual2s mean corrections vary in sign.
This supports examining remaining noisy/accelerated gravity reference, but
does not prove correction was erroneous: it can also compensate real gyro
bias or imperfect logged-signal reconstruction. Physical motion unknown.

Concrete separate AHRS defect found: helper_3dmath `invSqrt` used pointer
punning float->long->float with strict-aliasing warnings suppressed. Native
LP64 ASan/UBSan reproduced an8-byte stack overread from4-byte float. ESP32-P4
long is32bit, so this LP64 overflow is NOT evidence for the P4 rare corruption
or drift. Replaced with uint32_t/memcpy, retaining two Newton steps and exact
positive-input results versus defined32bit legacy equivalent. Added actual
Mahony host test for inverse-root accuracy, legacy equivalence,30deg gyro
integration and convergence to +/-20deg at500/1000/2000Hz. Passed at-O2 with
ASan/UBSan. P4 build and19 Python tests pass. Source change NOT flashed;
board retains best-flight accel-filter image and AC_CORRECTION setting.
No gain/LPF/PID/trim/guard changes. Next step should discriminate correction
behavior offline/bench before selecting a flight-affecting fusion change.

### Production-code replay completed (no board changes)

Added `prepare_mahony_replay.py` and `replay_mahony.cpp`. Exported16,013 rows
at nominal500Hz from23:15 flight, approximating166Hz attitude PT1 with two
half-ms held gyro steps per1kHz logged sample. Native executable runs actual
`lib/AHRS/src/Mahony.cpp` under-O2 ASan/UBSan. Initial tilt converged from first
logged Euler, yaw arbitrary (IMU-only), integralFB initially0; actual pre-log
history unknown. Accel/Euler rounding,1kHz gyro decimation and fusion/log phase
prevent exact replay. Quaternion initial yaw does not supply a heading reference.

| twoKp/twoKi | RP RMS deviation from logged Euler | RP peak deviation |
|---|---|---|
| 1.0/0.1 (current) | 0.207/0.512deg | 0.441/1.026deg |
| 0.5/0.1 | 0.785/1.268deg | 2.144/3.352deg |
| 0.2/0.1 | 1.340/2.830deg | 3.721/6.075deg |
| 0/0 (gyro-only) | 3.568/2.674deg | 7.392/6.888deg |

Deviation from the logged estimate is NOT error against true attitude. These
are open-loop replays of a flight generated by current controller, not predictions
of closed-loop flight with new gains. Close baseline match strongly supports
deterministic estimator response to the recorded input, rather than unexplained
large in-estimator jumps, but does not rule out upstream sensor faults or rare
corruption absent from this recording. It does not justify gain retuning.

Extended production Mahony synthetic test: zero true gyro/level attitude with
accel[0.1,0,1] for10s makes estimated pitch between-5 and-8deg (expected apparent
tilt atan(0.1)~5.71deg, plus integral transient). After60s clean[0,0,1] input,
pitch returns within0.1deg. Passed ASan/UBSan. This demonstrates an inherent
gravity-versus-linear-acceleration ambiguity without magnitude guards, not a
new implementation defect. No always-on instrumentation/firmware/settings changes.

Stopping point for this evidence: cannot establish true intermittent horizon
error or select a corrective gain solely from these logs. Next discriminating
measurement is prop-free repeatable known physical orientation: reference frame
level, pitch +/-20deg and return, yaw/roll changes and return; record live MSP
attitude/accel. A bench mismatch would justify a frame/math fix; if bench passes,
loaded sensor/mechanical/linear-acceleration effects remain. Do not request another
flight merely to gather the same non-independent attitude channels.

### User-assisted prop-free static orientation checks

Read-only MSP101/102/108 via COM26, disarmed checked for every sample;
6s held/8s returned captures using `capture_attitude.py`. Medians below are
settled second-half samples. User moved whole frame, returned to same support.
No motors, calibration, settings save or firmware flash. Capture files
`xsfc-*-held.json` and `xsfc-level-*.json` are in approved temp storage.

| Pose | Estimated roll/pitch, deg | Accel XYZ, 2048/g |
|---|---|---|
| Initial reference | 0.0/-0.1 | 4/1/2050 |
| Nose down held | -0.1/+25.3 | -873/-4/1853 |
| Return | +0.1/+0.1 | 1/1/2049 |
| Nose up held | +0.2/-16.6 | 584/6/1963 |
| Return | 0.0/-0.1 | 7/-1/2048 |
| Right roll held | +15.2/-0.5 | 18/545/1999 |
| Return | +0.3/0.0 | 0/0/2048 |
| Left roll held | -8.3/-0.5 | 16/-296/2014 |
| Final return | -0.2/-0.1 | 2/-5/2048 |

Static gyro medians zero. Each held tilt agrees approximately with gravity
vector; no gross axis/sign/cross-axis error or persistent multi-degree return
offset reproduced. One empty MSP reply after nose-up return; retry successful.
Actual angles were not measured independently with an inclinometer; captures
start after user handling and may reconnect/reset USB, so these results do NOT
measure continuous dynamic response, transient recovery speed, yaw coupling,
or motor-loaded estimator accuracy. No conclusion that flight horizon is fixed.
Current evidence narrows investigation toward loaded acceleration/vibration,
sampling and dynamic fusion behavior rather than a fixed trim/axis error.

### Continuous prop-free movement capture

`xsfc-continuous-tilts-v2.json`:6645 read-only samples across179.98s,
zero MSP gaps, maximum sample interval39.1ms, no reconnect during capture.
User movements were much larger than requested: pitch reached+84.2/-87.6deg,
roll+89.1/-97.0deg. Near-vertical pitch Euler roll can reach123.8deg due to
Euler singularity/coupling; do not interpret that alone as estimator failure.
All four returns to level recovered near baseline. Physically stationary,
near-level accel segments (gyro max<=1logged unit,norm within0.04g,accel-derived
RP within1deg):41.40..60.01s estimatedRP[0.3,0.2]->[0,0];
72.81..97.00s[0.2,-0.4]->[0.1,-0.2];115.30..141.24s[-0.8,0]->[0.1,-0.1];
159.41..179.99s[-1.1,-0.2]->[0.1,-0.2]deg. Last10s estimatedRP median
[0.1,-0.2],range[0.1..0.2,-0.2..-0.2]deg. Thus no persistent multi-degree
offset or gross dynamic failure reproduced unloaded, even after large tilts.
MSP sequential messages are not simultaneous and ~37Hz cannot establish
submillisecond dynamics. No external inclinometer/motor load/independent flight
attitude reference. Board firmware/settings unchanged. Earlier first180s
attempt aborted on missing MSP response and lost in-memory samples; capture
script now journals/flushed samples and saves JSON in finally, retries transient
framing gaps on same connection, stops after5 consecutive failures/ARM detection.
