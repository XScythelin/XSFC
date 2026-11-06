"""Exercise real Input + Actuator with synthetic 250Hz receiver frames."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
exe = subprocess.check_output(['wsl.exe', '--exec', 'wslpath', '-a', str(ROOT / 'sitl/build/host/xsfc-host')], text=True).strip()


def frame(arm=0, throttle=-1, healthy=1):
    return f'0 0 0 0 0 9.80665 0 0 0 {throttle} {arm} 1 {healthy}'


samples = [frame()] * 4000
samples += [frame(1)] * 300
samples += [frame(1, -.4)] * 300
samples += [frame(1, -.4, 0)] * 2500
samples += [frame()] * 100
for exchange_ms in (1, 2):
    result = subprocess.run(['wsl.exe', '--exec', 'env', 'XSFC_HOST_RECEIVER=1',
                             'XSFC_HOST_DIAGNOSTICS=1', exe, '--raw-imu', str(exchange_ms)], input='\n'.join(samples)+'\n',
                            text=True, capture_output=True, timeout=30, check=True)
    rows = [list(map(float, line.split())) for line in result.stdout.splitlines()]
    assert len(rows) == len(samples) and all(len(row) == 18 for row in rows)
    assert all(row[:4] == [1000]*4 for row in rows[:4000])
    assert any(max(row[:4]) > 1100 and int(row[14]) & 1 for row in rows[4300:4600]), rows[4500]
    assert all(row[:4] == [1000]*4 for row in rows[-100:]), rows[-1]
    assert any(row[16] != 0 for row in rows[5000:7000]), 'Production failsafe never activated'
    assert all(row[:4] == [1000]*4 and not (int(row[14]) & 1) for row in rows[6800:7100]), 'Loss did not disarm'
    print(f'Production Input/Actuator {exchange_ms}ms: calibration, AUX arm, filtered throttle, RX loss failsafe and disarm PASS')
