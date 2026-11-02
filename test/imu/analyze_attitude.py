"""Offline attitude/acceleration diagnostic; requires numpy, no board access.

Input is configurator parser JSON, optionally preceded by parser warnings.
P correction is reconstructed approximately from rounded Euler/logged accel;
it is NOT ground-truth attitude error or a full Mahony replay.
"""
import argparse
import json
from pathlib import Path
import numpy as np


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('input', type=Path)
    ap.add_argument('--start', type=float, default=2)
    ap.add_argument('--stop', type=float, default=30)
    ap.add_argument('--two-kp', type=float, default=1.0,
                    help='Mahony twoKp after restoreGain; config P50 gives 1.0')
    args = ap.parse_args()
    raw = args.input.read_bytes()
    text = raw.decode('utf-16' if raw.startswith(b'\xff\xfe') else 'utf-8')
    data = json.loads(text[text.index('{"config"'):])
    if data['config'].get('debug_mode') != 48:
        raise ValueError('Requires AC_CORRECTION Euler debug, not another debug mode')
    f = np.asarray(data['frames'], dtype=float)
    idx = {name: i for i, name in enumerate(data['fields'])}
    def get(prefix):
        return f[:, [idx[f'{prefix}[{i}]'] for i in range(3)]]
    t = (f[:, idx['time']] - f[0, idx['time']]) / 1e6
    acc = get('accSmooth') / data['config']['acc_1G']
    att = get('debug') / 10
    gyro = get('gyroADC')
    rc = get('rcCommand')
    rate = get('setpoint')
    roll, pitch = np.deg2rad(att[:, :2]).T
    gravity = np.column_stack((-np.sin(pitch),
                               np.sin(roll) * np.cos(pitch),
                               np.cos(roll) * np.cos(pitch)))
    norm = np.linalg.norm(acc, axis=1)
    unit = np.divide(acc, norm[:, None], out=np.zeros_like(acc),
                     where=norm[:, None] != 0)
    correction = np.rad2deg(np.cross(unit, gravity) * (0.5 * args.two_kp))
    # Body angular velocity -> Euler rates. Logged gyro is not the final
    # PT1-filtered attitude.rate; rounding/phase/decimation limit this estimate.
    euler_rate = np.column_stack((gyro[:, 0] + np.sin(roll)*np.tan(pitch)*gyro[:, 1]
                                 + np.cos(roll)*np.tan(pitch)*gyro[:, 2],
                                 np.cos(roll)*gyro[:, 1] - np.sin(roll)*gyro[:, 2]))
    integral = np.vstack((np.zeros(2), np.cumsum(
        (euler_rate[1:] + euler_rate[:-1]) * 0.5 * np.diff(t)[:, None], axis=0)))

    for lo in np.arange(args.start, args.stop, 2):
        mask = (t >= lo) & (t < min(lo + 2, args.stop))
        ids = np.flatnonzero(mask)
        if len(ids) < 2:
            continue
        first, last = ids[0], ids[-1]
        center = mask & np.all(np.abs(rc[:, :2]) <= 5, axis=1)
        result = dict(start_s=float(lo),
                      mean_acc_g=acc[mask].mean(0).tolist(),
                      mean_attitude_deg=att[mask].mean(0).tolist(),
                      norm_p05_median_p95=np.quantile(norm[mask], [.05, .5, .95]).tolist(),
                      approximate_p_correction_mean_dps=correction[mask].mean(0).tolist(),
                      approximate_p_correction_abs_p95_dps=np.quantile(abs(correction[mask]), .95, axis=0).tolist(),
                      angle_change_minus_gyro_integral_deg=(att[last, :2] - att[first, :2]
                          - (integral[last] - integral[first])).tolist(),
                      centered_fraction=float(center.sum() / mask.sum()))
        print(json.dumps(result))

    mask = (t >= args.start) & (t < args.stop)
    for axis in range(2):
        zero = mask & (rc[:, axis] == 0) & (abs(att[:, axis]) >= .5)
        print(json.dumps(dict(axis=axis, zero_rc_nonzero_angle_count=int(zero.sum()),
                              feedback_opposes_angle_fraction=float(np.mean(
                                  rate[zero, axis] * att[zero, axis] < 0)) if zero.any() else None)))


if __name__ == '__main__':
    main()
