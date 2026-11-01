"""Guard the P4 wiring defaults against motor/bus pin conflicts."""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


class P4PinsTests(unittest.TestCase):
    def test_p4_spi_rate_and_pid_defaults(self):
        target = (ROOT / 'lib/Xsfc/src/Target/TargetESP32p4.h').read_text()
        self.assertRegex(target, r'#define\s+XSFC_GYRO_SPI_RATE_MAX\s+8000\b')
        self.assertRegex(target, r'#define\s+XSFC_GYRO_I2C_RATE_MAX\s+2000\b')
        config = (ROOT / 'lib/Xsfc/src/ModelConfig.h').read_text()
        self.assertRegex(config, r'#if defined\(ESP32P4\)\s+int8_t loopSync = 2;')
        self.assertRegex(config, r'#else\s+int8_t loopSync = 8;')

    def test_dynamic_filter_enabled_by_default(self):
        source = (ROOT / 'lib/Xsfc/src/Target/TargetESP32p4.h').read_text()
        mask = re.search(r'^#define\s+XSFC_FEATURE_MASK\s+(.+)$', source, re.MULTILINE)
        self.assertIsNotNone(mask)
        self.assertIn('FEATURE_DYNAMIC_FILTER', mask.group(1))
        config = (ROOT / 'lib/Xsfc/src/ModelConfig.h').read_text()
        self.assertRegex(config, r'featureMask\s*=\s*XSFC_FEATURE_MASK\s*;')

    def test_motor_defaults_do_not_overlap_bus_pins(self):
        source = (ROOT / 'lib/Xsfc/src/Target/TargetESP32p4.h').read_text()
        pins = {name: int(value) for name, value in re.findall(
            r'^#define\s+(XSFC_\w+)\s+(-?\d+)\b', source, re.MULTILINE)}
        motors = [pins[f'XSFC_OUTPUT_{i}'] for i in range(4)]
        self.assertEqual(motors, [33, 49, 50, 52])
        self.assertEqual(len(set(motors)), 4)
        self.assertEqual(pins['XSFC_SPI_CS_BARO'], -1)
        bus_names = ['XSFC_SPI_0_SCK', 'XSFC_SPI_0_MOSI', 'XSFC_SPI_0_MISO',
                     'XSFC_SPI_CS_GYRO', 'XSFC_SPI_CS_BARO',
                     'XSFC_I2C_0_SDA', 'XSFC_I2C_0_SCL']
        bus_names += [f'XSFC_SERIAL_{i}_{direction}' for i in range(5)
                      for direction in ('TX', 'RX')]
        for name in bus_names:
            self.assertNotIn(pins[name], motors, name)


if __name__ == '__main__':
    unittest.main()
