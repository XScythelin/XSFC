# Webots integration work in progress

The installed Mavic demo is only an environment/network/rendering check.
It is not XSFC SITL. The rejected MuJoCo viewer is not used here.

User component specification: `../profiles/mark4-v2.json`.
Unknown mass, diagonal motor spacing, inertia and propulsion characteristics
must not be silently presented as measured values.

`prepare.py` copies the installed rural world to an isolated development world
and replaces the Mavic with a schematic 5.1-inch quad and centered IMU devices.
Requires explicit mass/spacing arguments, writes an assumptions sidecar.
It currently has **no propulsion or XSFC adapter and is not flyable**.
The controller `xsfc_webots` uses the production host executable and cannot
auto-arm in interactive mode or start the installed Mavic PID.
The schematic is not a CAD model of the Mark4 frame.

USB radio diagnostic (from repository root):

```powershell
sitl/.venv/Scripts/python.exe sitl/joystick.py --list
sitl/.venv/Scripts/python.exe sitl/joystick.py
```

Guided single-window calibration (Russian instructions):

```powershell
sitl/.venv/Scripts/python.exe sitl/calibrate_radio.py
```

Center both sticks INCLUDING throttle for initial capture, sweep all four axes,
then identify positive roll, pitch, yaw and throttle individually as prompted.
Review normalized channels and lower throttle before saving. Saves atomically
to `sitl/profiles/tango2-usb.json`; does not change transmitter settings.
Pitch calibration identifies stick-toward-pilot as positive RC input; the
eventual body-frame adapter must explicitly map this into firmware conventions.
Calibration does not arm or launch Webots. Escape exits; R restarts capture.

`radio.py` provides validated normalization and a simulation arm gate tested via
`python -m unittest discover -s sitl -p test_radio.py -v`. The gate is not yet
connected to a flight controller. No claim of working USB flight control yet.

Select USB Joystick/HID on Tango II. Monitor supports hotplug and prints raw
axes/buttons. Axis order, sign, center and endpoints must be confirmed before
flight input is enabled. Throttle must never be inferred from an arbitrary axis.
Planned safety: disarmed startup, low-throttle arm gate, explicit arm action,
disarm on device loss; no automatic re-arm on reconnect.

## First closed-loop development endpoint

Build `python sitl/host/build.py` first, then generate the world with explicit
provisional mass/spacing, e.g. `python sitl/webots/prepare.py --mass-kg 0.65
--wheelbase-mm 225`. Launch the generated world in Webots. Runtime uses the
existing Python venv (runtime.ini currently points to this checkout).

SPACE requests simulation arm/disarm with low-throttle gating. ESC disarms.
R resets the body and restarts the host controller only when disarmed.
USB loss disarms; reconnect does not arm.
USB profile GUID must match; no guessed fallback mapping.

Controller/Rates/PID/Mixer are production sources, but the endpoint currently
injects ideal Webots attitude and gyro directly at 1kHz, holding measurements
for four 4kHz control steps. RC goes into ModelState after calibrated
normalization, not through InputManager. Mode selection uses Angle/Acro with
the simulation arm gate, not production mode management. This is NOT the full
firmware sensor/input pipeline, and not a filter/IMU validation environment.

Thrust is synthetic 0..5N per motor with 25ms lag; torque/thrust ratio .012m;
inertia .003/.003/.005kg m². Propulsion is not measured XING/Gemfan data.
Development damping is linear .02 / angular .001 (arbitrary, not identified).
Do not tune real PID here.
No P4/SPI execution timing emulation.

Setting `XSFC_WEBOTS_TEST=1` explicitly selects scripted virtual-flight test
instead of USB (test auto-arms and exits Webots after 6 simulated seconds).
Never set this variable for an interactive user session.

Launch interactive mode with `python sitl/webots/run.py`; the launcher clears
the test variable explicitly. Click the 3D view before using SPACE/ESC/R.
Bind USB buttons with `sitl/.venv/Scripts/python.exe sitl/bind_buttons.py`:
first ARM/DISARM, then Angle/Acro. USB buttons toggle on each press; bindings
reported as axes follow switch position: ARM inactive disarms immediately,
mode inactive selects Angle and active selects Acro. Startup mode
is Angle. Both buttons must be released after connection before accepting
requests; USB loss returns to disarmed Angle. High throttle blocks arming.
ESC remains an emergency keyboard disarm.
For the current synthetic model, hover is approximately 28% throttle; increase
gently. This is not a prediction of the real quad's hover throttle.

The installed R2025a Python `Node.addTorque` incorrectly calls the force C API.
Our endpoint calls `wb_supervisor_node_add_torque` directly as a local workaround;
installed Webots files are untouched. The scripted test checks takeoff, signed
roll/pitch/yaw response, level recovery and disarmed motor outputs, and saves
the trajectory to `sitl/build/webots-flight-test.json`.
