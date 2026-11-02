"""Read-only disarmed MSP attitude/IMU capture for known-orientation tests."""
import argparse
import json
import statistics
import struct
import time
from pathlib import Path
import serial
from board_baseline_probe import request, drain


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--port', default='COM26')
    ap.add_argument('--seconds', type=float, default=5)
    ap.add_argument('--output', required=True, type=Path)
    ap.add_argument('--markers', type=Path,
                    help='Optional timestamped JSONL movement markers; never sends board commands')
    args = ap.parse_args()
    records = []
    errors = 0
    consecutive = 0
    journal = args.output.with_suffix('.samples.jsonl')
    def read_sample(s, start):
        status = request(s, 101)
        if len(status) < 10:
            raise ValueError('Short MSP status')
        if struct.unpack_from('<I', status, 6)[0] & 1:
            raise RuntimeError('ARMED: stopping read-only test')
        imu = request(s, 102)
        attitude = request(s, 108)
        if len(attitude) != 6 or len(imu) < 12 or len(imu) % 2:
            raise ValueError('Unexpected MSP payload length')
        return dict(t=time.monotonic()-start, unix=time.time(),
                    attitude=struct.unpack('<hhh', attitude),
                    imu=struct.unpack('<'+'h'*(len(imu)//2), imu))
    with serial.Serial(args.port, 115200, timeout=.2, write_timeout=2) as s, journal.open('w') as stream:
        s.reset_input_buffer()
        start = time.monotonic()
        if args.markers:
            args.markers.write_text(json.dumps(dict(event='capture_start', unix=time.time())) + '\n')
        try:
            while time.monotonic() - start < args.seconds:
                try:
                    record = read_sample(s, start)
                except (RuntimeError, ValueError) as exc:
                    if 'ARMED' in str(exc):
                        raise
                    errors += 1
                    consecutive += 1
                    stream.write(json.dumps(dict(t=time.monotonic()-start, error=str(exc))) + '\n')
                    stream.flush()
                    print('MSP gap:', str(exc), flush=True)
                    if consecutive >= 5:
                        raise RuntimeError('Five consecutive MSP failures; stopping capture') from exc
                    # Discard late/partial replies until quiet, on the SAME
                    # connection. Never fabricate samples or reconnect/reset.
                    drain(s, quiet=.25, maximum=1)
                    continue
                consecutive = 0
                records.append(record)
                stream.write(json.dumps(record) + '\n')
                stream.flush()
                if len(records) == 1:
                    print('CAPTURE READY: disarmed verified, keep USB connected', flush=True)
                time.sleep(.02)
        finally:
            args.output.write_text(json.dumps(records, indent=2))
            print('Saved samples:', len(records), 'MSP gaps:', errors, flush=True)
    if not records:
        return
    # Settled tail, rather than initial transient after handling/reconnect.
    tail = records[len(records)//2:]
    for key in ('attitude', 'imu'):
        size = 3 if key == 'attitude' else 6
        print(key, 'tail median', [statistics.median(r[key][i] for r in tail) for i in range(size)],
              'tail min/max', [(min(r[key][i] for r in tail), max(r[key][i] for r in tail)) for i in range(size)])
    print('Samples:', len(records), 'attitude units: roll/pitch 0.1deg, yaw deg; accel 2048/g')


if __name__ == '__main__':
    main()
