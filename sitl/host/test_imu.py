"""Regression at the raw IMU boundary; no supplied ideal Euler attitude."""
from pathlib import Path
import math
import subprocess

ROOT = Path(__file__).resolve().parents[2]
exe = subprocess.check_output(['wsl.exe', '--exec', 'wslpath', '-a',
                               str(ROOT / 'sitl/build/host/xsfc-host')], text=True).strip()


def run(samples):
    result = subprocess.run(['wsl.exe', '--exec', exe, '--raw-imu'],
                            input='\n'.join(samples) + '\n', text=True,
                            capture_output=True, check=True, timeout=20)
    rows = [list(map(int, row.split())) for row in result.stdout.splitlines()]
    assert all(row == [1000] * 4 for row in rows), 'Disarmed output changed'
    parts = result.stderr.split()
    assert parts[0] == 'IMU' and parts[4] == 'GYRO', result.stderr
    values = list(map(float, parts[1:4] + parts[5:8]))
    assert all(math.isfinite(v) for v in values)
    return values


level = '0 0 0 0 0 9.80665 0 0 0 -1 0 1'
rest = run([level] * 4000)
assert max(abs(v) for v in rest[:3]) < .01, rest
angle = .2
tilted = f'0 0 0 0 {9.80665 * math.sin(angle)} {9.80665 * math.cos(angle)} 0 0 0 -1 0 1'
response = run([level] * 4000 + [tilted] * 6000)
assert abs(response[0] - angle) < .03, response
assert abs(response[1]) < .01, response
print('Raw IMU production path: stationary level, gravity tilt, finite fusion, disarmed outputs PASS', response)

# Dynamic pure roll: gravity expressed in rotating body axes, not ideal Euler input.
rate = .3
rotation = [f'{rate} 0 0 0 {9.80665 * math.sin(rate * i * .001)} '
            f'{9.80665 * math.cos(rate * i * .001)} 0 0 0 -1 0 1' for i in range(1, 1001)]
response = run([level] * 6000 + rotation)
assert abs(response[0] - rate) < .04, response
freefall = run([level] * 6000 + ['0 0 0 0 0 0 0 0 0 -1 0 1'] * 1000)
assert max(abs(v) for v in freefall[:3]) < .02, freefall

# Observe actual production filter input/output after calibration. Stimulus is
# sampled at 1kHz and held for eight 8kHz reads, NOT an 8kHz sensor fidelity test.
vibration = [f'{math.sin(2 * math.pi * 180 * i * .001)} 0 0 0 0 9.80665 0 0 0 -1 0 1'
             for i in range(2000)]
result = subprocess.run(['wsl.exe', '--exec', 'env', 'XSFC_HOST_DIAGNOSTICS=1',
                         exe, '--raw-imu'], input='\n'.join([level] * 6000 + vibration) + '\n',
                        text=True, capture_output=True, check=True, timeout=20)
rows = [list(map(float, row.split())) for row in result.stdout.splitlines()[-1000:]]
assert all(len(row) == 14 and row[:4] == [1000] * 4 for row in rows)
raw_rms = math.sqrt(sum(row[7] ** 2 for row in rows) / len(rows))
filtered_rms = math.sqrt(sum(row[10] ** 2 for row in rows) / len(rows))
assert .1 < raw_rms < .8 and filtered_rms < raw_rms * .8, (raw_rms, filtered_rms)
print('Dynamic roll, freefall and production 180Hz attenuation PASS',
      dict(prefiltered_scaled_rms=raw_rms, filtered_rms=filtered_rms))
