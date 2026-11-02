"""Read-only USB/MSP soak; retains raw bytes including unsolicited panic output."""
import argparse
import json
from pathlib import Path
import struct
import time
import serial
from board_baseline_probe import request, drain

p = argparse.ArgumentParser()
p.add_argument('--seconds', type=int, default=300)
p.add_argument('--output', required=True)
p.add_argument('--stats', action='store_true', help='Capture CLI stats before closing USB; exit reboots without saving')
a = p.parse_args()
raw = bytearray()
pending = bytearray()
valid = bad = 0
timeouts = []
latencies = []
with serial.Serial('COM26', 115200, timeout=0.003, write_timeout=1) as s:
    s.reset_input_buffer()
    status = request(s, 101)
    assert not struct.unpack_from('<I', status, 6)[0] & 1, 'ARMED'
    start = time.monotonic()
    while time.monotonic() - start < a.seconds:
        sent = time.monotonic()
        s.write(bytes([36, 77, 60, 0, 102, 102]))
        received = False
        while time.monotonic() - sent < 0.3:
            chunk = s.read(max(1, s.in_waiting))
            raw.extend(chunk)
            pending.extend(chunk)
            while len(pending) >= 5:
                idx = pending.find(b'$M>')
                if idx < 0:
                    pending[:] = pending[-2:]
                    break
                del pending[:idx]
                length = pending[3] + 6
                if len(pending) < length:
                    break
                frame = bytes(pending[:length])
                del pending[:length]
                checksum = 0
                for b in frame[3:]:
                    checksum ^= b
                if checksum or frame[4] != 102:
                    bad += 1
                else:
                    valid += 1
                    latencies.append((time.monotonic() - sent) * 1000)
                    received = True
            if received:
                break
        if not received:
            timeouts.append(time.monotonic() - start)
        time.sleep(0.005)
    final_status = request(s, 101).hex()
    if a.stats:
        assert not struct.unpack_from('<I', bytes.fromhex(final_status), 6)[0] & 1, 'ARMED'
        s.write(b'#')
        transcript = drain(s)
        if b'CLI' not in transcript:
            raise RuntimeError('CLI entry not confirmed')
        s.write(b'stats\r\n')
        transcript += drain(s, quiet=0.5, maximum=10)
        Path(a.output + '.stats.txt').write_bytes(transcript)
        print(transcript.decode(errors='replace'))
        s.write(b'exit\r\n')
Path(a.output + '.bin').write_bytes(raw)
result = dict(valid=valid, bad=bad, timeouts=timeouts, latencies_ms=latencies,
              initial_status=status.hex(), final_status=final_status)
Path(a.output + '.json').write_text(json.dumps(result))
print(json.dumps({k: v for k, v in result.items() if k != 'latencies_ms'}))
for marker in (b'Guru', b'panic', b'ESP-ROM', b'assert'):
    idx = raw.find(marker)
    if idx >= 0:
        print(marker, repr(raw[max(0, idx-100):idx+5000]))
