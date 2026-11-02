"""Export decoded AC_CORRECTION flight for approximate native Mahony replay.

Not full-fidelity: gyro is rounded, decimated to 1kHz and before attitude PT1;
startup quaternion/integral feedback and exact fusion/log phase are unavailable.
"""
import argparse
import json
from pathlib import Path
import math


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('input', type=Path)
    ap.add_argument('output', type=Path)
    args = ap.parse_args()
    raw = args.input.read_bytes()
    text = raw.decode('utf-16' if raw.startswith(b'\xff\xfe') else 'utf-8')
    d = json.loads(text[text.index('{"config"'):])
    if d['config'].get('debug_mode') != 48:
        raise ValueError('AC_CORRECTION required')
    idx = {name: i for i, name in enumerate(d['fields'])}
    get = lambda f, prefix: [f[idx[f'{prefix}[{i}]']] for i in range(3)]
    # Approximate the production attitude PT1 (166Hz at2kHz) with two
    # equal-value half-ms steps per logged gyro snapshot.
    gain = (2 * math.pi * 166 / 2000) / (1 + 2 * math.pi * 166 / 2000)
    gyro = [0., 0., 0.]
    rows = []
    phase = d['frames'][0][idx['loopIteration']] % 4
    for f in d['frames']:
        for _ in range(2):
            gyro = [v + gain * (u * math.pi / 180 - v)
                    for u, v in zip(get(f, 'gyroADC'), gyro)]
        if f[idx['loopIteration']] % 4 != phase:
            continue
        acc = [v / d['config']['acc_1G'] for v in get(f, 'accSmooth')]
        angles = [v / 10 for v in get(f, 'debug')[:2]]
        rows.append(' '.join(f'{v:.9g}' for v in gyro + acc + angles))
    args.output.write_text('\n'.join(rows) + '\n')
    print('Exported', len(rows), '500-Hz rows; replay is approximate, not ground truth')


if __name__ == '__main__':
    main()
