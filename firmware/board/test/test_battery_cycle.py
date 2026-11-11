import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]/'tools'))
from battery_cycle import Cycle
from battery_capture import classify


def sample(t, state=1, mv=4200, boots=73, mode=1):
    pin = 0 if state == 1 else 1
    vin = 0 if state == 0 else 5000
    lines = [f'STAT ver=2.6.40-qa part=ota_0 ota=valid boots={boots} up={int(t)+100}s',
             f'POWER valid=1 level=99 vin_mv={vin} charge_pin={pin} state={state}',
             f'now mv={mv} lvl=99 chg={int(state==1)} pwr={mode}, 0 log entries (15 min)']
    return {'raw': lines, **classify(lines)}


class CycleTests(unittest.TestCase):
    def test_partial_discharge_waits_for_real_supply_removal(self):
        c = Cycle(partial_discharge=True)
        c.feed(sample(0, 1, 4190), 0)
        c.feed(sample(5, 2, 4110), 5)
        self.assertEqual(c.phase, 'await_disconnect')
        c.gap()
        c.feed(sample(10, 0, 4000), 10)
        self.assertEqual(c.phase, 'discharging')
        self.assertTrue(c.partial_discharge)
        c.feed(sample(15, 2, 4100), 15)
        self.assertEqual(c.error, 'external_power_during_discharge')

    def test_partial_discharge_rejects_restart_and_records_low_endpoint(self):
        c = Cycle(partial_discharge=True)
        c.feed(sample(0, 1), 0)
        c.feed(sample(5, 0, boots=74), 5)
        self.assertEqual(c.error, 'device_reboot_or_firmware_change')
        c = Cycle(partial_discharge=True)
        c.feed(sample(0, 0, 3390), 0)
        for t in range(5, 65, 5):
            c.feed(sample(t, 0, 3390), t)
        self.assertEqual(c.phase, 'discharging')
        c.feed(sample(65, 0, 3390), 65)
        self.assertEqual(c.phase, 'usable_low_endpoint')

    def test_bracketed_power_does_not_mix_later_battery_reading(self):
        row = sample(0)
        row['raw'][1] += ' vbat_mv=4190 sample_ms=12 pwr=2'
        row['raw'][2] = 'now mv=4110 lvl=88 chg=0 pwr=0'
        result = classify(row['raw'])
        self.assertTrue(result['usable'])
        self.assertEqual(result['vbat_mv'], 4190)
        self.assertEqual(result['display_percent'], 99)
        self.assertEqual(result['power_mode'], 2)
        self.assertFalse(result['atomic_sample'])

    def test_partial_failed_or_slow_power_sample_never_falls_back(self):
        for suffix in (' vbat_mv=4190', ' vbat_mv=4190 sample_ms=101 pwr=2',
                       ' vbat_mv=0 sample_ms=12 pwr=2',
                       ' vbat_mv=4190 sample_ms=-1 pwr=2',
                       ' vbat_mv=4190 sample_ms=12 pwr=3'):
            row = sample(0)
            row['raw'][1] += suffix
            self.assertFalse(classify(row['raw'])['usable'])
        row = sample(0)
        row['raw'][1] = row['raw'][1].replace('valid=1', 'valid=0')
        row['raw'][1] += ' vbat_mv=4190 sample_ms=12 pwr=2'
        self.assertFalse(classify(row['raw'])['usable'])

    def charged(self):
        c = Cycle()
        for t in range(0, 65, 5):
            c.feed(sample(t), t)
        for t in range(65, 370, 5):
            c.feed(sample(t, 2), t)
        self.assertEqual(c.phase, 'await_unplug')
        return c

    def test_full_display_does_not_mean_full(self):
        c = Cycle()
        for t in range(0, 700, 5):
            c.feed(sample(t, 2), t)
        self.assertEqual(c.phase, 'await_charge')

    def test_charge_termination_requires_five_minutes(self):
        c = Cycle()
        for t in range(0, 65, 5):
            c.feed(sample(t), t)
        c.feed(sample(65, 2), 65)
        c.feed(sample(70, 2), 70)
        self.assertEqual(c.phase, 'charging')
        self.charged()

    def test_low_voltage_with_released_pin_not_full(self):
        c = Cycle()
        for t in range(0, 65, 5):
            c.feed(sample(t), t)
        for t in range(65, 800, 5):
            c.feed(sample(t, 2, 3900), t)
        self.assertEqual(c.phase, 'charging')

    def test_unknown_and_time_gaps_reset_termination_dwell(self):
        for explicit in (True, False):
            c = Cycle()
            for t in range(0, 65, 5):
                c.feed(sample(t), t)
            c.feed(sample(65, 2), 65)
            if explicit:
                c.feed({'usable': False}, 70)
            c.feed(sample(600, 2), 600)
            self.assertEqual(c.phase, 'charging')
            self.assertEqual(c.anchor, 600)

    def test_discharge_endpoint_is_sustained_and_distinct_from_calibration(self):
        c = self.charged()
        c.feed(sample(370, 0, 4150), 370)
        self.assertEqual(c.phase, 'discharging')
        for t in range(375, 435, 5):
            c.feed(sample(t, 0, 3390), t)
        self.assertEqual(c.phase, 'discharging')
        c.feed(sample(435, 0, 3390), 435)
        self.assertEqual(c.phase, 'usable_low_endpoint')

    def test_reboot_and_charger_reconnect_rejected(self):
        c = self.charged()
        c.feed(sample(370, 0, 4100, boots=74), 370)
        self.assertEqual(c.error, 'device_reboot_or_firmware_change')
        c = self.charged()
        c.feed(sample(370, 0, 4100), 370)
        c.feed(sample(375, 1, 4150), 375)
        self.assertEqual(c.error, 'external_power_during_discharge')

    def test_invalid_partial_and_inconsistent_readings(self):
        self.assertFalse(classify([])['usable'])
        row = sample(0)
        self.assertIsNone(row['reference_soc_percent'])
        self.assertFalse(classify(row['raw']+row['raw'])['usable'])
        row['raw'][2] = 'now mv=4200 lvl=99 chg=0 pwr=1'
        self.assertFalse(classify(row['raw'])['usable'])


if __name__ == '__main__':
    unittest.main()
