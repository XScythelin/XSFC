"""Production barometer calibration/altitude/hold and stale-sensor regression."""
from pathlib import Path
import math
import subprocess

ROOT = Path(__file__).resolve().parents[2]
exe = subprocess.check_output(['wsl.exe', '--exec', 'wslpath', '-a',
                               str(ROOT / 'sitl/build/host/xsfc-host')], text=True).strip()


def sample(height=0., arm=0, hold=0, healthy=1):
    pressure = 101325 * (1 - height / 44330) ** 5.254999
    return f'0 0 0 0 0 9.80665 0 0 0 -0.44 {arm} 1 {pressure} {hold} {healthy}'


samples = [sample()] * 4000 + [sample(2., 1)] * 3000
samples += [sample(2., 1, 1)] * 1000
samples += [sample(2., 1, 1, 0)] * 500
samples += [sample()] * 100
result = subprocess.run(['wsl.exe', '--exec', 'env', 'XSFC_HOST_ALTITUDE=1',
                         'XSFC_HOST_DIAGNOSTICS=1', exe, '--raw-imu'],
                        input='\n'.join(samples) + '\n', text=True, capture_output=True,
                        check=True, timeout=30)
rows = [list(map(float, line.split())) for line in result.stdout.splitlines()]
assert len(rows) == len(samples) and all(len(row) == 19 for row in rows)
assert all(math.isfinite(v) for row in rows for v in row)
assert rows[3999][18] == 1, rows[3999]
assert abs(rows[6999][14] - 2) < .15, rows[6999]
assert rows[7999][17:19] == [1, 1], rows[7999]
assert rows[8499][17:19] == [0, 0], rows[8499]
assert all(row[:4] == [1000] * 4 for row in rows[-100:])
print('Production barometer calibration, altitude convergence, hold engagement, stale-sensor exit and disarm PASS')
