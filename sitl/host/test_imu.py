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
