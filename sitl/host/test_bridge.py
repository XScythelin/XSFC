"""Integration checks against the compiled production Controller/Mixer."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
exe = subprocess.check_output(['wsl.exe', '--exec', 'wslpath', '-a', str(ROOT / 'sitl/build/host/xsfc-host')], text=True).strip()
samples = ['0 0 0 0 0 0 0 0 0 -1 0', '0 0 0 0 0 0 0 0 0 0 1',
           '0 0 0 0 0 0 .1 0 0 0 1', '0 0 0 0 0 0 0 .1 0 0 1',
           '0 0 0 0 0 0 0 0 .1 0 1', '0 0 0 0 0 0 0 0 0 -1 0']
result = subprocess.run(['wsl.exe', '--exec', exe, '--bridge'], input='\n'.join(s + ' 1' for s in samples) + '\n',
                        text=True, capture_output=True, check=True, timeout=10)
rows = [list(map(int, line.split())) for line in result.stdout.splitlines()]
assert len(rows) == len(samples)
assert rows[0] == rows[-1] == [1000] * 4
assert all(len(row) == 4 and all(1000 <= p <= 2000 for p in row) for row in rows)
assert rows[2][2] > rows[2][0] and rows[2][3] > rows[2][1]
assert rows[3][0] > rows[3][1] and rows[3][2] > rows[3][3]
print('Production bridge: disarm, arm, bounds, roll/pitch allocation and re-disarm PASS')

def tilted_reply(angle):
    result = subprocess.run(['wsl.exe', '--exec', exe, '--bridge'],
                            input=f'0 0 0 .2 0 0 0 0 0 0 1 {angle}\n',
                            text=True, capture_output=True, check=True, timeout=10)
    return list(map(int, result.stdout.split()))

angle = tilted_reply(1)
acro = tilted_reply(0)
assert angle[0] > angle[2] and angle[1] > angle[3]
assert len(set(acro)) == 1, acro
print('Production modes: Angle corrects tilt; Acro does not self-level PASS')
