"""A tilted boot must observe gravity, never redefine the level reference."""
from pathlib import Path
import math
import subprocess

ROOT = Path(__file__).resolve().parents[2]
exe = subprocess.check_output(['wsl.exe', '--exec', 'wslpath', '-a',
                               str(ROOT / 'sitl/build/host/xsfc-host')], text=True).strip()
for axis in (0, 1):
    for angle in (-.35, .35):
        accel = [0., 0., 9.80665 * math.cos(angle)]
        accel[1 if axis == 0 else 0] = 9.80665 * math.sin(angle) * (1 if axis == 0 else -1)
        sample = f'0 0 0 {accel[0]} {accel[1]} {accel[2]} 0 0 0 -1 0 1\n'
        result = subprocess.run(['wsl.exe', '--exec', 'env', 'XSFC_HOST_DIAGNOSTICS=1',
                                 exe, '--raw-imu', '2'], input=sample * 2500,
                                text=True, capture_output=True, check=True, timeout=20)
        attitude = list(map(float, result.stdout.splitlines()[-1].split()[4:7]))
        assert abs(attitude[axis] - angle) < .025, (axis, angle, attitude)
        assert abs(attitude[1 - axis]) < .01, attitude
print('Tilted boot: gravity-referenced roll/pitch at +/-20 degrees PASS')
