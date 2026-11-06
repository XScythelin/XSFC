"""Memory-only settings overlays change production behavior and fail closed."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def linux(path):
    return subprocess.check_output(['wsl.exe', '--exec', 'wslpath', '-a', str(path)], text=True).strip()


exe = linux(ROOT / 'sitl/build/host/xsfc-host')
sample = '0 0 0 0 0 0 .3 0 0 -.4 1 0\n' * 30


def run(settings):
    with tempfile.TemporaryDirectory(dir=ROOT / 'sitl/build') as directory:
        path = Path(directory) / 'settings.txt'
        path.write_text(settings)
        return subprocess.run(['wsl.exe', '--exec', 'env', 'XSFC_HOST_SETTINGS=' + linux(path),
                               exe, '--bridge'], input=sample, text=True, capture_output=True, timeout=20)


normal = run('input_roll_rate 100\n')
lower = run('input_roll_rate 20\n')
assert normal.returncode == lower.returncode == 0
assert normal.stdout != lower.stdout, 'Production rates did not change'
for bad in ('pin_motor_0 5\n', 'input_roll_rate 0\n', 'input_roll_rate 999\n',
            'pid_roll_p -1\n', 'input_roll_rate 10 garbage\n', 'save\n'):
    result = run(bad)
    assert result.returncode == 6 and not result.stdout, (bad, result)
print('SITL rate overlay changes production output; unknown, hardware and invalid settings refused PASS')
