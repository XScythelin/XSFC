"""Exercise production range processing/HoldSafety without hardware or storage."""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[2]
exe = subprocess.check_output(['wsl.exe', '--exec', 'wslpath', '-a',
                               str(Path(os.environ.get('XSFC_HOST_BUILD_DIR', ROOT / 'sitl/build/host')) / 'xsfc-host')], text=True).strip()


def run(samples):
    result = subprocess.run(['wsl.exe', '--exec', 'env', 'XSFC_HOST_DIAGNOSTICS=1',
                             'XSFC_HOST_ALTITUDE=1', 'XSFC_HOST_RANGE=1', exe, '--raw-imu', '2'],
                            input='\n'.join(samples)+'\n', text=True, capture_output=True, check=True, timeout=20)
    return [list(map(float, line.split())) for line in result.stdout.splitlines()]


def row(arm=0, hold=0, distance=1200, healthy=1):
    return f'0 0 0 0 0 9.80665 0 0 0 0 {arm} 1 101325 {hold} 1 {distance} {healthy}'


base = [row()] * 2000
valid = run(base + [row(1, 1)] * 100)
assert valid[-1][17] == valid[-1][21] == valid[-1][22] == 1, valid[-1]
assert abs(valid[-1][19]-1.2) < .02, valid[-1]
for invalid in (row(1, 1, healthy=0), row(1, 1, distance=7000), row(1, 1, distance=-1)):
    result = run(base + [row(1, 1)]*100 + [invalid]*200)
    assert result[-1][17] == result[-1][21] == 0, result[-1]
    assert result[-1][23] == 1, result[-1]
    # A healthy feed alone must not silently re-enable hold after loss.
    still_latched = run(base + [row(1, 1)]*100 + [invalid]*200 + [row(1, 1)]*200)
    assert still_latched[-1][17] == 0 and still_latched[-1][21] == 1, still_latched[-1]
    resumed = run(base + [row(1, 1)]*100 + [invalid]*200 + [row(1, 0)]*200 + [row(1, 1)]*100)
    assert resumed[-1][17] == resumed[-1][22] == 1, resumed[-1]
print('Production RANGEHOLD: height, engage, stale, limit, invalid distance PASS')
