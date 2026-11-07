"""Calibrated radio input and simulation-only arming gates (no hardware I/O)."""
import math
from dataclasses import dataclass


CHANNELS = ('roll', 'pitch', 'yaw', 'throttle')


def binding_active(binding, axes, buttons):
    """Read an explicit USB binding (including legacy button indices)."""
    if isinstance(binding, int):
        return bool(buttons[binding])
    index = binding['index']
    if binding['kind'] == 'button':
        return buttons[index] == binding['active']
    if binding['kind'] == 'axis':
        value = axes[index]
        if not math.isfinite(value):
            raise ValueError('Invalid switch sample')
        midpoint = (binding['active'] + binding['inactive']) / 2
        return value > midpoint if binding['active'] > binding['inactive'] else value < midpoint
    raise ValueError('Unknown USB binding')


def validate_profile(profile):
    if profile.get('version') != 1 or not profile.get('guid'):
        raise ValueError('Invalid radio profile header')
    used = set()
    for name in CHANNELS:
        channel = profile['channels'][name]
        axis = channel['axis']
        if not isinstance(axis, int) or axis < 0 or axis in used:
            raise ValueError('Invalid or duplicate axis')
        used.add(axis)
        lo, center, hi = (channel[key] for key in ('min', 'center', 'max'))
        if not all(math.isfinite(v) and -1.05 <= v <= 1.05 for v in (lo, center, hi)):
            raise ValueError('Invalid axis endpoints')
        if hi - lo < 1.0 or not lo < center < hi:
            raise ValueError('Axis range too small')
        if channel['sign'] not in (-1, 1):
            raise ValueError('Invalid axis direction')
    return profile


def normalize(profile, axes):
    validate_profile(profile)
    output = {}
    for name in CHANNELS:
        cfg = profile['channels'][name]
        raw = axes[cfg['axis']]
        if not math.isfinite(raw) or not -1.1 <= raw <= 1.1:
            raise ValueError('Invalid joystick sample')
        if name == 'throttle':
            value = (raw - cfg['min']) / (cfg['max'] - cfg['min'])
            output[name] = max(0., min(1., value if cfg['sign'] == 1 else 1. - value))
        else:
            delta = raw - cfg['center']
            scale = cfg['max'] - cfg['center'] if delta >= 0 else cfg['center'] - cfg['min']
            value = max(-1., min(1., delta / scale)) * cfg['sign']
            deadband = .025
            output[name] = math.copysign(max(0., (abs(value) - deadband) / (1. - deadband)), value)
    return output


@dataclass
class ArmGate:
    armed: bool = False
    previous_request: bool = False
    last_valid: float = -math.inf
    timeout: float = .25

    def update(self, now, connected, controls, arm_request=False):
        valid = connected and controls is not None and all(
            math.isfinite(controls.get(key, math.nan)) and
            (0 <= controls[key] <= 1 if key == 'throttle' else -1 <= controls[key] <= 1)
            for key in CHANNELS)
        rising = arm_request and not self.previous_request
        self.previous_request = arm_request
        if not valid:
            self.armed = False
            return False
        # A gap in polling also disarms. A held request cannot re-arm.
        if now - self.last_valid > self.timeout:
            self.armed = False
            self.last_valid = now
            return False
        self.last_valid = now
        if rising:
            if self.armed:
                self.armed = False
            elif controls['throttle'] <= .05:
                self.armed = True
        return self.armed
