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
