"""Do not teach a new horizon from disarmed freefall/impact acceleration."""
from pathlib import Path
import math
import subprocess

ROOT = Path(__file__).resolve().parents[2]
exe = subprocess.check_output(['wsl.exe', '--exec', 'wslpath', '-a', str(ROOT / 'sitl/build/host/xsfc-host')], text=True).strip()
for axis in (0, 1):
    rows = []
    def sample(n, armed, gyro=(0., 0., 0.), accel=(0., 0., 9.80665)):
        rows.extend([' '.join(map(str, (*gyro, *accel, 0, 0, 0, -1, armed, 1)))] * n)
    sample(2000, 0)
    sample(500, 1)
    # The aircraft is rotating in freefall, then sees a sideways impact force.
    g = [0., 0., 0.]
    g[axis] = .7
    sample(250, 0, g, (0., 0., 0.))
    impact = [0., 0., 19.6133]
    impact[1 if axis == 0 else 0] = 15.
    sample(500, 0, accel=impact)
    sample(500, 1, accel=(0., 0., 0.))
    result = subprocess.run(['wsl.exe', '--exec', 'env', 'XSFC_HOST_DIAGNOSTICS=1', exe, '--raw-imu', '2'],
                            input='\n'.join(rows)+'\n', text=True, capture_output=True, check=True, timeout=30)
    attitude = list(map(float, result.stdout.splitlines()[-1].split()))[4:7]
    assert abs(attitude[axis] - .35) < .04, (axis, attitude)
print('Repeated ARM: disarmed freefall rotation/impact preserves gravity reference PASS')
