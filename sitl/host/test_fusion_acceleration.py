"""Known gyro trajectories with unobservable thrust-axis specific force.

The estimator must not use simulator truth; truth is only an assertion here.
"""
from pathlib import Path
import math
import subprocess

ROOT = Path(__file__).resolve().parents[2]
exe = subprocess.check_output(['wsl.exe', '--exec', 'wslpath', '-a', str(ROOT / 'sitl/build/host/xsfc-host')], text=True).strip()

for axis in (0, 1):
    for angle in (-.35, .35):
        rows = ['0 0 0 0 0 9.80665 0 0 0 -1 0 1'] * 4000
        truth = 0.
        for target, duration in ((angle, 1.), (angle, 6.), (0., 1.), (0., 2.)):
            count = round(duration * 1000)
            rate = (target - truth) / duration
            for _ in range(count):
                truth += rate * .001
                gyro = [0., 0., 0.]
                gyro[axis] = rate
                rows.append(f'{gyro[0]} {gyro[1]} 0 0 0 {9.80665 / math.cos(truth)} 0 0 0 -1 1 1')
        result = subprocess.run(['wsl.exe', '--exec', 'env', 'XSFC_HOST_DIAGNOSTICS=1', exe, '--raw-imu'],
                                input='\n'.join(rows) + '\n', text=True, capture_output=True, timeout=30, check=True)
        output = [list(map(float, line.split())) for line in result.stdout.splitlines()]
        assert abs(output[10999][4 + axis] - angle) < .035, output[10999]
        assert max(abs(v) for v in output[-1][4:6]) < .035, output[-1]
        # After disarm, stationary gravity must recover an initially wrong
        # estimate rather than remaining permanently locked to gyro prediction.
        gravity = [0., 0., 9.80665 * math.cos(angle)]
        gravity[1 if axis == 0 else 0] = (1 if axis == 0 else -1) * 9.80665 * math.sin(angle)
        recovery = '0 0 0 ' + ' '.join(map(str, gravity)) + ' 0 0 0 -1 0 1'
        result = subprocess.run(['wsl.exe', '--exec', 'env', 'XSFC_HOST_DIAGNOSTICS=1', exe, '--raw-imu'],
                                input='\n'.join(rows + [recovery] * 12000) + '\n', text=True,
                                capture_output=True, timeout=30, check=True)
        last = list(map(float, result.stdout.splitlines()[-1].split()))
        assert abs(last[4 + axis] - angle) < .035, last
print('Armed production fusion: signed roll/pitch sustained thrust acceleration and recovery PASS')

# Slow ramp is an adversarial case for innovation rejection: correction can
# follow a false observation before it ever exceeds the rejection threshold.
rows = ['0 0 0 0 0 9.80665 0 0 0 -1 0 1'] * 4000
for i in range(20000):
    rate = .035 if i < 10000 else 0.
    angle = min(i * .001 * .035, .35)
    rows.append(f'{rate} 0 0 0 0 {9.80665 / math.cos(angle)} 0 0 0 -1 1 1')
result = subprocess.run(['wsl.exe', '--exec', 'env', 'XSFC_HOST_DIAGNOSTICS=1', exe, '--raw-imu'],
                        input='\n'.join(rows) + '\n', text=True, capture_output=True, timeout=30, check=True)
last = list(map(float, result.stdout.splitlines()[-1].split()))
assert abs(last[4] - .35) < .035, last
print('Slow sustained acceleration ramp PASS')
