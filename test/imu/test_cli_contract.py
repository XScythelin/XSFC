"""Source-level registry coverage; runtime dump/restore still needs the FC."""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


class CliContractTests(unittest.TestCase):
    def test_2khz_baseline_is_registered_and_non_destructive(self):
        cli = (ROOT / 'lib/Xsfc/src/Connect/Cli.cpp').read_text()
        names = set(re.findall(r'Param\(PSTR\("([^"]+)"\)', cli))
        lines = (ROOT / 'config/p4_2khz_baseline.cli').read_text().splitlines()
        settings = {}
        for line in lines:
            if not line.strip() or line.startswith('#'):
                continue
            command, name, value = line.split()
            self.assertEqual(command, 'set')
            self.assertIn(name, names)
            self.assertNotIn(name, settings)
            settings[name] = value
        for name, value in {'gyro_rate': '2000', 'pid_sync': '1', 'mixer_sync': '1',
                            'gyro_lpf_freq': '100', 'gyro_lpf2_freq': '213',
                            'gyro_dyn_lpf_min': '170', 'gyro_dyn_lpf_max': '425',
                            'accel_lpf_freq': '15', 'gyro_rpm_harmonics': '0'}.items():
            self.assertEqual(settings[name], value)

    def test_p4_keeps_historical_gyro_low_pass_defaults(self):
        config = (ROOT / 'lib/Xsfc/src/ModelConfig.h').read_text()
        for declaration in ('filter{FILTER_PT1, 100}', 'filter2{FILTER_PT1, 213}',
                            'dynLpfFilter{FILTER_PT1, 425, 170}'):
            self.assertIn(declaration, config)
        self.assertNotIn('gyro.filter.freq = gyro.filter2.freq = 0', config)
        self.assertNotIn('gyro.dynLpfFilter.freq = gyro.dynLpfFilter.cutoff = 0', config)

    def test_missing_configuration_fields_are_registered(self):
        cli = (ROOT / 'lib/Xsfc/src/Connect/Cli.cpp').read_text()
        for field in ('featureMask', 'buzzer.beeperMask', 'blackbox.fieldsMask',
                      'input.serialRxProvider', 'input.ppmMode', 'input.minCheck',
                      'input.maxCheck', 'telemetry', 'gyroRate'):
            self.assertIn('&c.' + field, cli, field)

    def test_every_filter_component_is_registered(self):
        cli = (ROOT / 'lib/Xsfc/src/Connect/Cli.cpp').read_text()
        for filter_ in ('gyro.filter', 'gyro.filter2', 'gyro.filter3', 'gyro.notch1Filter',
                        'gyro.notch2Filter', 'gyro.dynLpfFilter', 'accel.filter',
                        'mag.filter', 'baro.filter', 'rangefinder.filter', 'input.filter',
                        'input.filterDerivative', 'level.ptermFilter', 'posHold.velFilter',
                        'yaw.filter', 'dterm.filter', 'dterm.filter2', 'dterm.notchFilter',
                        'dterm.dynLpfFilter'):
            for component in ('type', 'freq', 'cutoff'):
                self.assertIn('&c.' + filter_ + '.' + component, cli)

    def test_all_pid_slots_are_restorable(self):
        cli = (ROOT / 'lib/Xsfc/src/Connect/Cli.cpp').read_text()
        for slot in ('ROLL', 'PITCH', 'YAW', 'ALT', 'POS', 'POSR', 'NAVR', 'LEVEL', 'MAG', 'VEL'):
            for component in 'PIDF':
                self.assertIn(f'&c.pid[FC_PID_{slot}].{component}', cli)

    def test_names_are_unique(self):
        cli = (ROOT / 'lib/Xsfc/src/Connect/Cli.cpp').read_text()
        names = re.findall(r'Param\(PSTR\("([^"]+)"\)', cli)
        self.assertEqual(len(names), len(set(names)))

    def test_flash_mode_is_persisted_without_shifting_legacy_fields(self):
        config = (ROOT / 'lib/Xsfc/src/ModelConfig.h').read_text()
        self.assertLess(config.index('uint16_t gyroRate'), config.index('uint8_t blackboxFlashMode'))
        storage = (ROOT / 'lib/Xsfc/src/Utils/Storage.cpp').read_text()
        self.assertIn('version == 0x08 && size == aligned(gyroRateEnd)', storage)
        self.assertIn('polling ? gyroRateEnd', storage)
        cli = (ROOT / 'lib/Xsfc/src/Connect/Cli.cpp').read_text()
        self.assertIn('PSTR("blackbox_flash_mode"), &c.blackboxFlashMode', cli)
        self.assertIn('PSTR("DIRECT"), PSTR("PSRAM")', cli)

    def test_direct_mode_bypasses_staging(self):
        source = (ROOT / 'lib/Xsfc/src/Blackbox/BlackboxFlashfs.cpp').read_text()
        self.assertIn('if(!modeInitialized)', source)
        self.assertIn('if(useStaging && staging)', source)
        direct = source.split('if(!useStaging)', 1)[1].split('if(!staging', 1)[0]
        self.assertIn('esp_partition_write_raw', direct)
        self.assertIn('return;', direct)

    def test_flash_startup_uses_mapping_and_writes_use_bounce_buffer(self):
        source = (ROOT / 'lib/Xsfc/src/Blackbox/BlackboxFlashfs.cpp').read_text()
        self.assertNotIn('esp_partition_read_raw(partition, 0, staging', source)
        self.assertNotIn('esp_partition_write_raw(p, dataBegin, staging', source)
        startup = source.split('int flashfsInit(void)', 1)[1].split('bool IRAM_ATTR flashfsFlushAsync', 1)[0]
        self.assertNotIn('esp_partition_read_raw', startup)
        self.assertIn('esp_partition_mmap(partition', startup)
        self.assertNotIn('memcpy(staging + offset', startup)
        self.assertIn('savedImage = static_cast<const uint8_t*>(mapped)', startup)
        self.assertIn('esp_partition_write_raw(p, begin, chunk, count)', source)

    def test_accel_filter_runs_on_consumer(self):
        sensor = (ROOT / 'lib/Xsfc/src/SensorManager.cpp').read_text()
        producer = sensor.split('SensorManager::read()', 1)[1].split('SensorManager::', 1)[0]
        self.assertNotIn('_accel.update()', producer)
        self.assertIn('Event(EVENT_ACCEL_READ, _model.state.accel.raw)', producer)
        consumer = (ROOT / 'lib/Xsfc/src/Xsfc.cpp').read_text()
        self.assertRegex(consumer, r'case EVENT_ACCEL_READ:\s+_sensor.processAccel\(e.accel\);\s+_sensor.fusion\(\);')
        self.assertNotIn('AccelTrust', (ROOT / 'lib/Xsfc/src/Control/Fusion.cpp').read_text())

    def test_gyro_consumer_uses_queued_snapshot(self):
        producer = (ROOT / 'lib/Xsfc/src/SensorManager.cpp').read_text()
        consumer = (ROOT / 'lib/Xsfc/src/Xsfc.cpp').read_text()
        self.assertIn('Event event(EVENT_GYRO_READ, _model.state.gyro.sampled, _model.state.gyro.raw)', producer)
        self.assertIn('_model.state.appQueue.send(event)', producer)
        self.assertIn('_sensor.preLoop(e.gyro, e.accel)', consumer)

    def test_pending_commit_is_incremental(self):
        source = (ROOT / 'lib/Xsfc/src/Blackbox/BlackboxFlashfs.cpp').read_text()
        service = source.split('void flashfsCommitPending()\n{', 1)[1].split('#endif', 1)[0]
        self.assertNotIn('commitStaging();', service)
        self.assertNotIn('while(', service)
        self.assertIn('uint8_t chunk[512]', service)
        self.assertIn('blackboxFlightActive()', service)
        self.assertIn('esp_partition_read_raw(p, *cursor, verified, count)', service)
        self.assertIn('memcmp(chunk, verified, count)', service)

    def test_no_temporary_runtime_instrumentation(self):
        for file in ('Xsfc.cpp', 'ModelState.h', 'Target/Queue.h', 'Connect/Cli.cpp'):
            source = (ROOT / 'lib/Xsfc/src' / file).read_text()
            for token in ('esp_cpu_set_watchpoint', 'TimingProbe', 'sampledAtUs', 'state.timing'):
                self.assertNotIn(token, source)

    def test_rc_memory_bounds(self):
        source = (ROOT / 'lib/Xsfc/src/Input.cpp').read_text()
        self.assertIn('channelCount > INPUT_CHANNELS', source)
        self.assertIn('if(static_cast<size_t>(i) >= INPUT_CHANNELS) return;', source)
        self.assertIn('uint16_t channels[INPUT_CHANNELS]{};', source)
        self.assertIn('if(static_cast<size_t>(ich.map) >= _model.state.input.channelCount)', source)


if __name__ == '__main__':
    unittest.main()
