# XSFC P4 algorithm SITL — first smoke prototype

## Interactive 3D flight (Windows)

An initial MuJoCo six-degree-of-freedom prototype is available:

```powershell
sitl/.venv/Scripts/python.exe sitl/fly.py
```

Install once if the isolated environment is absent (Python3.11, WSL g++):

```powershell
py -3.11 -m venv sitl/.venv
sitl/.venv/Scripts/python.exe -m pip install -r sitl/requirements.txt
```

The single pygame window displays MuJoCo-rendered3D physics and a control HUD.
Keep this window focused. SPACE arms/disarms the **virtual** quad;
W/S increases/decreases throttle, arrows command roll/pitch, A/D yaw,
R resets physics AND controller, ESC quits. Hover is approximately32% throttle,
slightly more is required to take off. Camera follows the quad. Close the
window to stop. No real serial ports or motors are accessed.

`--rate 2000|4000|8000` selects controller frequency. For automated check:

```powershell
sitl/.venv/Scripts/python.exe sitl/fly.py --headless --seconds 8
```

Uses actual production PID/filter/Mahony sources through a C++ stdin/stdout
bridge built in WSL. MuJoCo supplies gyro and specific-force accelerometer.
World/body useFLU (x forward,y left,z up), not an established full firmware
frame adapter. Four-motor allocation in bridge is a simplified implementation,
NOT production Mixer.cpp. Assumed650g quad, inertia .003/.003/.005kg*m2,
110mm motor coordinates, linear0..5N thrust/motor,25ms lag; not identified
user hardware. No wind/voltage/propwash or calibrated thrust curve.

Physics/sensor exchange500Hz, controller substeps selected2/4/8kHz with held
sensor input. This explicitly does NOT emulate ICM42688 filtering,4/8kHz raw
sampling or P4 execution timing. No full Controller initialization, settings
import, production rate curves or full sensor filter chain. Window can run
slower than real time; selected loop rate refers to simulation time.
USB joystick detection exists but control is intentionally not enabled before
axis mapping/calibration; keyboard is currently the usable input.

Separate host executable, no changes to the ESP32-P4 firmware target. Uses the
production `Pid.cpp`, `Filter.cpp` and `Mahony.cpp`, not reimplementations.
This is **not yet full firmware SITL**, chip emulation or a flight-tuning tool.

## Build (Windows with CMake and a C++17 compiler, or WSL)

Quick start from the repository root (Python + g++, or Windows Python + WSL g++):

```text
python sitl/run.py
python sitl/run.py --rate 4000
```

The first command tests all three rates. CSV files go to `sitl/build/`.
No Python packages are needed. No board connection, flash or motor commands.
This launcher does not install dependencies or download anything.

Alternatively, with CMake:

```text
cmake -S sitl -B sitl/build
cmake --build sitl/build --config Release
ctest --test-dir sitl/build -C Release --output-on-failure
```

Single-config executable: `sitl/build/xsfc-p4-sitl` (Windows adds `.exe`).
With Visual Studio generator: `sitl/build/Release/xsfc-p4-sitl.exe`.

```text
xsfc-p4-sitl --rate 4000 --csv pitch.csv
```

Run with 2000/4000/8000. CSV is500Hz: true and estimated pitch, target,
gyro, controller output and injected acceleration. Runs faster than real time.
Scenario:15deg pitch command1..3s, return, world horizontal acceleration0.1g
at7..10s, recovery until18s. Acceleration is externally prescribed, not integrated
translation: suitable for estimator sensitivity, not aerodynamic prediction.

## What matches P4 and what does not

-8kHz simulation clock; selected gyro/PID/mixer rate; accel/fusion500Hz.
- Production pitch PID coefficient scales and Angle P45; Mahony50/5 equivalent.
- Actual production filter classes; simplified100Hz gyro and15Hz accel chain.
- One pitch axis, assumed inertia/torque/damping/motor lag. No measured frame data.
- No full Controller initialization, config loading, feedforward, dynamic/RPM
  filters, four-motor mixer, actual DShot, RC/USB, FreeRTOS or inter-core queues.
- No ICM42688 register/AAF/UI/delay/quantization model yet. Fusion uses ideal
  angular velocity snapshots, not the full firmware attitude-rate filter chain.
- Host execution time says nothing about P4 deadlines or race conditions.

Next useful step: identified mass/inertia/geometry and motor response, then
four-motor mixer and full sensor chain. Webots/Tango integration comes later.
