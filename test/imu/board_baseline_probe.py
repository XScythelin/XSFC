"""Explicit, disarmed-only CLI backup/apply and read-only MSP bench probe."""
import argparse
import functools
import json
import operator
from pathlib import Path
import statistics
import struct
import time
import serial


def drain(s, quiet=0.25, maximum=15):
    result = bytearray()
    deadline = time.monotonic() + maximum
    last = time.monotonic()
    while time.monotonic() < deadline:
        chunk = s.read(max(1, s.in_waiting))
        if chunk:
            result.extend(chunk)
            last = time.monotonic()
        elif time.monotonic() - last >= quiet:
            break
    return bytes(result)


def request(s, cmd):
    s.write(bytes([36, 77, 60, 0, cmd, cmd]))
    header = s.read(3)
    if header != b'$M>':
        raise RuntimeError('Invalid/missing MSP header: ' + repr(header))
    size, response = s.read(2)
    body = s.read(size + 1)
    if response != cmd or len(body) != size + 1:
        raise RuntimeError('Incomplete MSP reply')
    if functools.reduce(operator.xor, bytes([size, response]) + body, 0):
        raise RuntimeError('MSP checksum mismatch')
    return body[:-1]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('action', choices=['backup', 'apply', 'probe'])
    ap.add_argument('--port', default='COM26')
    ap.add_argument('--output', required=True)
    args = ap.parse_args()
    with serial.Serial(args.port, 115200, timeout=0.1, write_timeout=2) as s:
        s.reset_input_buffer()
        status = request(s, 101)
        if struct.unpack_from('<I', status, 6)[0] & 1:
            raise RuntimeError('Board is ARMED; refusing operation')
        if args.action == 'probe':
            records = []
            for _ in range(400):
                start = time.perf_counter()
                imu = request(s, 102)
                elapsed = (time.perf_counter() - start) * 1000
                records.append({'rtt_ms': elapsed, 'imu': struct.unpack('<' + 'h' * (len(imu)//2), imu)})
                time.sleep(0.01)
            result = {'status_before': status.hex(), 'status_after': request(s, 101).hex(),
                      'dataflash': request(s, 70).hex(), 'records': records}
            Path(args.output).write_text(json.dumps(result, indent=2))
            times = sorted(r['rtt_ms'] for r in records)
            print('Valid RAW_IMU replies:', len(records), 'payload:', len(imu))
            print('RTT ms median/p99/max:', statistics.median(times), times[int(len(times)*.99)], max(times))
            print('status:', result['status_after'], 'dataflash:', result['dataflash'])
            return
        s.write(b'#')
        transcript = drain(s)
        if b'CLI' not in transcript:
            raise RuntimeError('CLI entry not confirmed')
        if args.action == 'apply':
            baseline = Path(__file__).resolve().parents[2] / 'config/p4_2khz_baseline.cli'
            for line in baseline.read_text().splitlines():
                if not line or line.startswith('#'):
                    continue
                s.write(line.encode() + b'\r\n')
                reply = drain(s, quiet=0.08)
                transcript += reply
                if any(word in reply.lower() for word in (b'error', b'invalid', b'unknown', b'not found')):
                    Path(args.output).write_bytes(transcript)
                    raise RuntimeError('CLI rejected: ' + line + ': ' + repr(reply))
        s.write(b'dump all\r\n')
        dump = drain(s, quiet=0.7, maximum=30)
        transcript += dump
        Path(args.output).write_bytes(transcript)
        if b'set gyro_rate ' not in dump or b'set pin_' not in dump:
            raise RuntimeError('Dump completeness not confirmed; not saving')
        print('Dump saved:', args.output, len(dump), 'bytes')
        if args.action == 'apply':
            expected = dict(line.split()[1:] for line in baseline.read_text().splitlines() if line.startswith('set '))
            actual = dict((m[1], m[2]) for line in dump.decode(errors='replace').splitlines()
                          if len(m := line.split()) == 3 and m[0] == 'set')
            mismatch = {k: (v, actual.get(k)) for k, v in expected.items() if actual.get(k) != v}
            if mismatch:
                raise RuntimeError('Readback mismatch; not saving: ' + repr(mismatch))
            s.write(b'save\r\n')
            reply = drain(s, maximum=4)
            with open(args.output, 'ab') as out:
                out.write(reply)
            print('Save response:', repr(reply))
        else:
            s.write(b'exit\r\n')


if __name__ == '__main__':
    main()
