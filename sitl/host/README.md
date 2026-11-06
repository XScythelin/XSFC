# Production XSFC host target — integration in progress

Build on Windows using installed WSL g++:

```powershell
python sitl/host/build.py
wsl.exe --exec /mnt/c/Users/Scythelin/Documents/XSFC/sitl/build/host/xsfc-host
```

The second command is an example for this checkout; adjust for other paths.

This target defines `XSFC_SITL`, **not** `UNIT_TEST`, and compiles the real
`Model`, `ModelConfig`, `ModelState`, `Controller.cpp`, `Rates.cpp`, `Mixer.cpp`,
PID, filters, timers and supporting sources. There are no replacement Model,
Controller or Mixer classes. Target selection leaves ESP32 targets unchanged
when XSFC_SITL is absent.

Platform shims implement Arduino time/string/stream interfaces, memory-only ESC
outputs, inert IPAddress and no-op GPIO. Time advances explicitly in simulated
microseconds, not host wall time. Storage/network/physical buses are not used.
Shims are an incomplete compatibility layer, not full Arduino emulation.

Current executable initializes production controller/mixer and checks 4000
disarmed 4kHz ticks: finite control output and four ESC pulses at 1000.
This is a core integration smoke test, **not yet flyable SITL**. It does not run
the full firmware boot, sensor filtering/fusion, InputManager or mode manager.
It uses default configuration, not the user's onboard settings.

Next: production input/mode/sensor pipeline, framed sensor/motor transport,
explicit Webots/firmware axis transformations and automated sign tests, then
USB radio integration and closed-loop physics. The old `sitl/bridge.cpp` is a
separate rejected experiment and is not part of this build.
