"""Verify actual production call counts, not just configured timer rates."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
exe = subprocess.check_output(['wsl.exe', '--exec', 'wslpath', '-a',
                               str(ROOT / 'sitl/build/host/xsfc-host')], text=True).strip()
for exchange_ms in (1, 2):
    count = 1000 // exchange_ms
    result = subprocess.run(['wsl.exe', '--exec', exe, '--raw-imu', str(exchange_ms)],
                            input='0 0 0 0 0 9.80665 0 0 0 -1 0 1\n' * count,
                            text=True, capture_output=True, check=True, timeout=20)
    expected = f'SCHEDULE exchange_ms={exchange_ms} exchanges={count} gyro=8000 pid=4000 mixer=4000'
    assert expected in result.stderr.splitlines(), result.stderr
    assert len(result.stdout.splitlines()) == count
print('Production call schedule: gyro 8000 / PID 4000 / mixer 4000 per simulated second PASS (1/2ms exchange)')
