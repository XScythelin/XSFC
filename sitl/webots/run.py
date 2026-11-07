"""Launch the Webots development world in interactive USB mode."""
import os
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
exe = Path.home() / 'AppData/Local/Programs/Webots/msys64/mingw64/bin/webots.exe'
environment = os.environ.copy()
environment.pop('XSFC_WEBOTS_TEST', None)
raise SystemExit(subprocess.run([str(exe), '--mode=realtime', '--stdout', '--stderr',
                                str(HERE / 'worlds/mark4_development.wbt')], env=environment).returncode)
