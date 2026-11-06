"""XSFC host compile probe using WSL g++."""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[2]
BUILD = Path(os.environ.get('XSFC_HOST_BUILD_DIR', str(ROOT / 'sitl/build/host'))).resolve()
BUILD.mkdir(parents=True, exist_ok=True)
includes = ['sitl/host/include', 'lib/Xsfc/src', 'lib/AHRS/src', 'lib/EscDriver/src']
def linux(path):
    return subprocess.check_output(['wsl.exe', '--exec', 'wslpath', '-a', str(path)], text=True).strip()

sources = ['sitl/host/main.cpp', 'lib/Xsfc/src/Control/Controller.cpp', 'lib/Xsfc/src/Output/Mixer.cpp',
           'lib/Xsfc/src/Control/Pid.cpp', 'lib/Xsfc/src/Control/Rates.cpp',
           'lib/Xsfc/src/Utils/Filter.cpp',
           'lib/Xsfc/src/Utils/Stats.cpp', 'lib/EscDriver/src/EscDriverBase.cpp',
           'lib/Xsfc/src/Utils/Timer.cpp', 'lib/Xsfc/src/Connect/Msp.cpp',
           'lib/Xsfc/src/Connect/StatusLed.cpp', 'lib/AHRS/src/Kalman.cpp', 'lib/Xsfc/src/Utils/Crc.cpp']
for source in sources:
    command = ['wsl.exe', '--exec', 'g++', '-std=c++17', '-O2', '-DXSFC_SITL', '-fmax-errors=6',
               *['-I' + linux(ROOT / path) for path in includes], '-c',
               linux(ROOT / source), '-o', linux(BUILD / (Path(source).stem + '.o'))]
    result = subprocess.run(command)
    if result.returncode:
        raise SystemExit(result.returncode)
    print('Compiled production source:', source, flush=True)
command = ['wsl.exe', '--exec', 'g++', *[linux(BUILD / (Path(source).stem + '.o')) for source in sources],
           '-o', linux(BUILD / 'xsfc-host')]
raise SystemExit(subprocess.run(command).returncode)
