import copy
import math
import unittest
from radio import ArmGate, CHANNELS, binding_active, normalize, validate_profile


def profile():
    return dict(version=1, guid='test', channels={
        name: dict(axis=i, min=-1, center=0, max=1, sign=1) for i, name in enumerate(CHANNELS)})


class RadioTest(unittest.TestCase):
    def test_switch_bindings(self):
        self.assertTrue(binding_active(dict(kind='button', index=1, active=1), [], [1, 1, 1]))
        self.assertFalse(binding_active(dict(kind='button', index=1, active=1), [], [1, 0, 1]))
        for active, inactive in ((1., -1.), (-1., 1.)):
            binding = dict(kind='axis', index=0, active=active, inactive=inactive)
            self.assertTrue(binding_active(binding, [active], []))
            self.assertFalse(binding_active(binding, [inactive], []))

    def test_normalization(self):
        self.assertEqual(normalize(profile(), [0, 0, 0, -1]), dict(roll=0, pitch=0, yaw=0, throttle=0))
        self.assertEqual(normalize(profile(), [1, -1, 0, 1]), dict(roll=1, pitch=-1, yaw=0, throttle=1))

    def test_reversed_throttle(self):
        cfg = profile()
        cfg['channels']['throttle']['sign'] = -1
        self.assertEqual(normalize(cfg, [0, 0, 0, 1])['throttle'], 0)

    def test_bad_profile(self):
        cfg = profile()
        cfg['channels']['yaw']['axis'] = 0
        with self.assertRaises(ValueError):
            validate_profile(cfg)

    def test_nonfinite(self):
        with self.assertRaises(ValueError):
            normalize(profile(), [math.nan, 0, 0, 0])

    def test_arm_disconnect_held_reconnect(self):
        gate = ArmGate()
        controls = normalize(profile(), [0, 0, 0, -1])
        self.assertFalse(gate.update(0, True, controls))
        self.assertTrue(gate.update(.01, True, controls, True))
        self.assertFalse(gate.update(.02, False, None, True))
        self.assertFalse(gate.update(.03, True, controls, True))
        gate.update(.04, True, controls, False)
        self.assertTrue(gate.update(.05, True, controls, True))

    def test_high_throttle_and_poll_gap(self):
        gate = ArmGate()
        controls = normalize(profile(), [0, 0, 0, -1])
        gate.update(0, True, controls)
        high = copy.copy(controls)
        high['throttle'] = .8
        self.assertFalse(gate.update(.01, True, high, True))
        gate.update(.02, True, controls)
        self.assertTrue(gate.update(.03, True, controls, True))
        self.assertFalse(gate.update(1, True, controls, True))


if __name__ == '__main__':
    unittest.main()
