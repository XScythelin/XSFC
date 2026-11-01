"""Run with: python -m unittest discover -s test/imu -p test_*.py"""
import csv
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


class PartitionTests(unittest.TestCase):
    def test_p4_layout(self):
        with (ROOT / 'partitions_16M_nota.csv').open() as source:
            rows = [r for r in csv.reader(source) if r and not r[0].startswith('#')]
        end = 0x9000
        for row in rows:
            offset, size = int(row[3], 0), int(row[4], 0)
            self.assertEqual(offset, end)
            self.assertEqual(offset % 4096, 0)
            self.assertEqual(size % 4096, 0)
            end = offset + size
        self.assertEqual(end, 16 * 1024 * 1024)
        app = next(r for r in rows if r[1].strip() == 'app')
        self.assertEqual(int(app[3], 0) % 65536, 0)
        self.assertEqual(int(app[4], 0), 4 * 1024 * 1024)


if __name__ == '__main__':
    unittest.main()
