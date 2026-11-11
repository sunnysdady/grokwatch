"""Read-only battery experiment. Never derives reference SOC from display SOC.

Uses existing stat/power/bat commands; each transaction retains separate raw
responses because the firmware does not take an atomic power/voltage sample.
Stops on connection loss instead of silently joining different sessions.
"""
import argparse
import datetime
import json
import re
import time
from pathlib import Path


def classify(lines):
    power = [s for s in lines if s.startswith('POWER ')]
    battery = [s for s in lines if s.startswith('now mv=')]
    status = [s for s in lines if s.startswith('STAT ')]
    if len(power) != 1 or len(battery) != 1 or len(status) != 1:
        return {'usable': False, 'reason': 'missing_or_duplicate_response'}
    p = dict(re.findall(r'(\w+)=(-?\d+)', power[0]))
    b = dict(re.findall(r'(\w+)=(-?\d+)', battery[0]))
    required = ('valid', 'vin_mv', 'charge_pin', 'state', 'level')
    if any(k not in p for k in required) or any(k not in b for k in ('mv', 'lvl', 'chg', 'pwr')):
        return {'usable': False, 'reason': 'missing_field'}
    p = {k: int(v) for k, v in p.items()}
    b = {k: int(v) for k, v in b.items()}
    # New firmware brackets VBAT with supply/CHG reads in one POWER command.
    # It is still sequential I2C, not an atomic hardware measurement. Never
    # fall back to the separately timed bat response for a partial new record.
    coherent_fields = ('vbat_mv', 'sample_ms', 'pwr')
    if any(k in p for k in coherent_fields):
        if not all(k in p for k in coherent_fields):
            return {'usable': False, 'reason': 'incomplete_power_sample'}
        expected = -1 if p['vin_mv'] < 0 else 0 if p['vin_mv'] <= 4000 else {0: 1, 1: 2}.get(p['charge_pin'], -1)
        usable = (p['valid'] == 1 and 2500 <= p['vbat_mv'] <= 4500
                  and p['charge_pin'] in (0, 1) and expected >= 0
                  and p['state'] == expected and 0 <= p['sample_ms'] <= 100
                  and 0 <= p['level'] <= 100 and p['pwr'] in (0, 1, 2))
        return {'usable': usable, 'reason': 'ok' if usable else 'invalid_or_transition',
                'vbat_mv': p['vbat_mv'], 'vin_mv': p['vin_mv'], 'state': p['state'],
                'display_percent': p['level'], 'power_mode': p['pwr'],
                'reference_soc_percent': None, 'atomic_sample': False,
                'sample_source': 'bracketed_power', 'sample_ms': p['sample_ms']}
    expected = -1 if p['vin_mv'] < 0 else 0 if p['vin_mv'] <= 4000 else {0: 1, 1: 2}.get(p['charge_pin'], -1)
    usable = (p['valid'] == 1 and 2500 <= b['mv'] <= 4500
              and expected >= 0 and p['state'] == expected
              and b['chg'] == int(expected == 1)
              and 0 <= p['level'] <= 100 and 0 <= b['lvl'] <= 100)
    return {'usable': usable, 'reason': 'ok' if usable else 'invalid_or_transition',
            'vbat_mv': b['mv'], 'vin_mv': p['vin_mv'], 'state': p['state'],
            'display_percent': b['lvl'], 'power_mode': b['pwr'],
            'reference_soc_percent': None, 'atomic_sample': False}


def main():
    import serial
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--seconds', type=int, default=600)
    args = parser.parse_args()
    if not 10 <= args.seconds <= 86400:
        parser.error('--seconds must be 10..86400')
    args.out.parent.mkdir(parents=True, exist_ok=True)
    # Exclusive create prevents replacing an earlier experiment.
    with args.out.open('x') as output:
        def save(row):
            output.write(json.dumps(row, ensure_ascii=False) + '\n')
            output.flush()
        save({'event': 'start', 'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'port': args.port, 'duration_s': args.seconds, 'interval_s': 5,
              'reference_soc_available': False, 'load_note': 'USB polling may change sleep/power draw'})
        start = time.monotonic()
        samples = usable = 0
        try:
            with serial.Serial(args.port, 115200, timeout=.2, write_timeout=2, exclusive=True) as port:
                port.reset_input_buffer()
                while time.monotonic() - start < args.seconds:
                    tick = time.monotonic()
                    port.write(b'\nstat\npower\nbat\n')
                    buf = b''
                    # Complete bat history is bounded to 4 KB; retain it as evidence.
                    while time.monotonic() - tick < 3:
                        buf += port.read(4096)
                        if len(buf) > 32768:
                            raise RuntimeError('unexpected console volume')
                    lines = [s.strip() for s in buf.decode(errors='replace').splitlines()]
                    # Only diagnostic responses; unrelated device logs can contain private data.
                    lines = [s for s in lines if s.startswith(('STAT ', 'POWER ', 'now mv='))
                             or re.fullmatch(r'\d\d-\d\d \d\d:\d\d \d+ \d+ \d+', s)]
                    result = classify(lines)
                    samples += 1
                    usable += result['usable']
                    save({'event': 'sample', 'elapsed_s': round(tick-start, 3),
                          'read_duration_s': round(time.monotonic()-tick, 3), 'raw': lines, **result})
                    time.sleep(max(0, 5-(time.monotonic()-tick)))
        except (OSError, serial.SerialException, RuntimeError, KeyboardInterrupt) as exc:
            save({'event': 'interrupted', 'error': str(exc), 'samples': samples, 'usable': usable})
            raise SystemExit(2)
        save({'event': 'end', 'samples': samples, 'usable': usable,
              'calibration_verified': False})
        print(f'Recorded {samples} transactions, {usable} usable. No reference SOC measured.')


if __name__ == '__main__':
    main()
