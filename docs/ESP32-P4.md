# ESP32-P4 (experimental)

Build with `python -m platformio run -e esp32p4`.

This target follows the ESP32-S3 architecture: four RMT motor outputs,
USB Serial/JTAG MSP, SPI sensors and two FreeRTOS application cores. The
LP core is not used. Existing ESP32 and ESP32-S3 environments are unchanged.

## Toolchain and differences

- Pinned pioarduino 55.03.31 (Arduino 3.3.1 / ESP-IDF 5.5.1).
- DFRobot FireBeetle 2 ESP32-P4 V1.0 (DFR1172): 360 MHz, 16 MB flash,
  32 MB PSRAM. Reuses the matching PlatformIO EV Board build profile;
  application pin assignments are specific to FireBeetle.
- Hardware I2C instead of the Xtensa-timed software I2C implementation.
- No native Wi-Fi, ESP-NOW or wireless serial. A companion radio is not
  automatically supported by this port.
- Five HP UARTs are exposed. UART3/4 are disabled and unassigned by default;
  configure `pin_serial_3_tx/rx`, `pin_serial_4_tx/rx` and `serial_3/4` in CLI.
- Uses the portable frequency analyzer rather than enabling the S3 DSP FFT
  path. Sensor loop limits remain at the S3 defaults until bench testing.
- Uses the legacy RMT driver with P4/IDF5 register helpers. Do not mix it with
  another library using the new RMT driver on this target.

## Default wiring

Assignments use GPIO exposed on the FireBeetle V1.0 side headers, based on
the manufacturer's pinout. SD, microphone, C6, camera and display are unused.
GPIO numbers below are chip GPIO, not sequential header pin numbers.
Edit `lib/Xsfc/src/Target/TargetESP32p4.h` or remap pins in CLI.

| Function | GPIO |
| --- | --- |
| Motors 0–3 | 33, 49, 50, 52 |
| UART0 TX/RX | 37 / 38 |
| UART1 TX/RX | 4 / 5 |
| UART2 TX/RX (receiver) | 22 / 23 |
| UART3/4 TX/RX | unassigned |
| SPI SCK/MOSI/MISO | 28 / 30 / 29 |
| Gyro / barometer CS | 32 / unassigned (-1) |
| I2C SCL/SDA | 8 / 7 |
| PPM / buzzer | 36 / 34 |
| Status LED / BOOT button | 3 / 35 |
| ADC voltage / current | 20 / 21 |

USB Serial/JTAG uses GPIO24/25; it is not the separate USB OTG connector.
The target uses the 16 MiB no-OTA layout with a 4 MiB application slot.
Board/chip revision, flash and PSRAM settings must match the actual module.

Detailed wiring and existing-config migration: [ESP32-P4-PINS.md](ESP32-P4-PINS.md).

Sources: [DFRobot pinout/specifications](https://wiki.dfrobot.com/dfr1172/),
[V1.0 schematic](https://dfimg.dfrobot.com/wiki/21103/DFR1172_firebeetle-esp32-p4r32-development-board_schematics_V1.0.pdf).

## Hardware validation required

Before flight, with propellers removed, verify USB/MSP, all UARTs (including
inverted SBUS), sensor discovery and axis orientation, ADC scaling, PWM and
DShot pulse timing on a logic analyzer, bidirectional DShot telemetry and
RPM filtering, then task timing and failsafe behavior. A successful build
alone does not establish flight readiness. Do not reuse an S3 configuration
blob: pin and serial configuration layouts differ.
# Memory and ICM20948 update

P4 uses `partitions_16M_nota.csv` covering the full 16 MiB flash: 4 MiB app,
11.875 MiB blackbox/data, 64 KiB coredump and the existing NVS/OTA metadata.
There is one app slot; OTA updates are not supported by this layout.
Flash the partition table together with the firmware (PlatformIO Upload or
merged image at 0x0). Old blackbox logs are not compatible with the moved data
partition: export them first, then erase/format the blackbox data before use.
NVS stays at its old address; a full-chip erase is not required by this change.

The board profile enables PSRAM. The bundled SDK uses PSRAM in malloc with a
4096-byte internal-allocation preference. `devinfo` reports actual flash,
internal heap and PSRAM sizes/free bytes; expected PSRAM is 33554432 bytes.
Do not move timing-critical state, task stacks or ISR buffers to PSRAM merely
to fill it. Firmware static RAM percentages do not describe the PSRAM heap.

ICM20948 supports I2C (0x68/0x69, <=400 kHz) and SPI (explicit CS,
1 MHz setup / requested 7 MHz sample reads, actual SCK rounded by hardware).
Startup verifies reset, power/configuration register readback and full samples.
Gyro menu modes 256/EX select DLPF bypass (9 kHz sensor ODR); other modes
enable DLPF (1125 Hz at divider zero). Numeric menu names are legacy MPU
bandwidths: the driver maps them to the closest ICM bandwidth rather than
pretending their register encodings are identical. Accel retains its own
DLPF at 1125 Hz ODR. The P4 scheduler separately caps polling at 4 kHz SPI /
2 kHz I2C using integer divisors of the sensor clock (9 kHz becomes 3 kHz SPI /
1.8 kHz I2C); this is not DRDY-synchronized acquisition of every sample.
Its internal AK09916
magnetometer is not exposed through the incompatible MPU slave-bus driver.
`devinfo` includes checked-read diagnostics; `stats` shows time per subsystem.
CPU load includes synchronous bus waiting, not just executed arithmetic.
All changes still require hardware validation without propellers.
