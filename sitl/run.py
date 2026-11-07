"""Build and run the small P4 algorithm SITL; Python standard library only."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--rate', type=int, choices=(2000, 4000, 8000))
    args = ap.parse_args()
    root = Path(__file__).resolve().parent.parent
    build = root / 'sitl' / 'build'
    build.mkdir(exist_ok=True)
    compiler = shutil.which('g++')
    wsl = not compiler and sys.platform == 'win32' and shutil.which('wsl.exe')
    if not compiler and not wsl:
        raise RuntimeError('Install a C++17 g++ compiler, or WSL with g++ on Windows.')

    def path(p):
        if wsl:
            return subprocess.check_output(
                ['wsl.exe', '--exec', 'wslpath', '-a', str(p)], text=True
            ).strip()
        return str(p)

    prefix = ['wsl.exe', '--exec'] if wsl else []
    exe = build / ('xsfc-p4-sitl' if wsl or sys.platform != 'win32' else 'xsfc-p4-sitl.exe')
    command = prefix + [compiler or 'g++', '-std=c++17', '-O2']
    for include in ('lib/AHRS/src', 'lib/Xsfc/src', 'test/imu/stubs'):
        command += ['-I' + path(root / include)]
    for source in ('sitl/main.cpp', 'lib/AHRS/src/Mahony.cpp',
                   'lib/Xsfc/src/Control/Pid.cpp', 'lib/Xsfc/src/Utils/Filter.cpp'):
        command.append(path(root / source))
    command += ['-o', path(exe)]
    print('Building production PID / filters / Mahony...', flush=True)
    subprocess.run(command, check=True)
    for rate in ([args.rate] if args.rate else [2000, 4000, 8000]):
        csv = build / f'pitch-{rate}.csv'
        subprocess.run(prefix + [path(exe), '--rate', str(rate), '--csv', path(csv)], check=True)
        print('CSV:', csv, flush=True)


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError, OSError) as exc:
        print(f'SITL failed: {exc}', file=sys.stderr)
        sys.exit(1)
