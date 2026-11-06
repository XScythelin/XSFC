"""Diagnostic: separate gyro integration from accelerometer thrust ambiguity.

Not a pass/fail regression: records estimator error for known synthetic motion.
"""
from pathlib import Path
import math
import subprocess

ROOT = Path(__file__).resolve().parents[2]
exe = subprocess.check_output(['wsl.exe', '--exec', 'wslpath', '-a', str(ROOT / 'sitl/build/host/xsfc-host')], text=True).strip()
angle = .35
level = '0 0 0 0 0 9.80665 0 0 0 -1 0 1'
for kind in ('gravity', 'thrust'):
    samples = [level] * 6000
    for i in range(1, 1001):
        a = angle * i / 1000
        ay, az = (9.80665 * math.sin(a), 9.80665 * math.cos(a)) if kind == 'gravity' else (0., 9.80665 / math.cos(a))
        samples.append(f'{angle} 0 0 0 {ay} {az} 0 0 0 -1 0 1')
    ay, az = (9.80665 * math.sin(angle), 9.80665 * math.cos(angle)) if kind == 'gravity' else (0., 9.80665 / math.cos(angle))
    samples += [f'0 0 0 0 {ay} {az} 0 0 0 -1 0 1'] * 6000
    result = subprocess.run(['wsl.exe', '--exec', exe, '--raw-imu'], input='\n'.join(samples)+'\n',
                            text=True, capture_output=True, timeout=30, check=True)
    estimate = float(result.stderr.split()[1])
    print(dict(case=kind, truth_roll_rad=angle, estimated_roll_rad=estimate, error_rad=estimate-angle))
