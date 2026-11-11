"""Bounded read-only usable-range experiment, not a coulomb counter.

Charge completion is a candidate anchor, requiring prior charging and five
continuous minutes of VIN present / CHG released / VBAT >= 4.10 V. End discharge
at the product's existing 3.40 V display endpoint; never bypass protection or
label this as physical empty. All timing is monotonic, not the watch RTC.
"""
import argparse
import datetime
import json
import os
import re
import time
from pathlib import Path
from battery_capture import classify


class Cycle:
    def __init__(self, partial_discharge=False):
        self.phase = 'await_disconnect' if partial_discharge else 'await_charge'
        self.partial_discharge = partial_discharge
        self.last = None
        self.anchor = None
        self.charging_s = 0
        self.identity = None
        self.uptime = None
        self.load_modes = set()
        self.error = None
        self.previous_state = None

    def gap(self):
        self.last = self.anchor = None
        self.previous_state = None

    def feed(self, row, now):
        if not row.get('usable'):
            self.gap()
            return
        stat = next((s for s in row['raw'] if s.startswith('STAT ')), '')
        fields = dict(re.findall(r'(\w+)=(\S+)', stat))
        identity = tuple(fields.get(k) for k in ('ver', 'part', 'boots'))
        up = re.fullmatch(r'(\d+)s', fields.get('up', ''))
        if None in identity or up is None or fields.get('ota') != 'valid':
            self.error = 'missing_identity_or_unvalidated_firmware'
            return
        uptime = int(up.group(1))
        if self.identity and (identity != self.identity or uptime < self.uptime):
            self.error = 'device_reboot_or_firmware_change'
            return
        self.identity, self.uptime = identity, uptime
        delta = 0 if self.last is None else now-self.last
        if delta < 0 or delta > 15:
            self.gap()
            delta = 0
        self.last = now
        state, mv = row['state'], row['vbat_mv']
        previous_state = self.previous_state
        self.previous_state = state
        if self.phase == 'await_charge' and state == 1:
            self.phase = 'charging'
        if self.phase == 'await_disconnect':
            if state == 0:
                self.phase = 'discharging'
                self.load_modes.add(row['power_mode'])
        elif self.phase == 'charging':
            if state == 1 and previous_state == 1:
                self.charging_s += delta
            if state == 2 and mv >= 4100 and self.charging_s >= 60:
                if self.anchor is None:
                    self.anchor = now
                if now-self.anchor >= 300:
                    self.phase, self.anchor = 'await_unplug', None
            else:
                self.anchor = None
        elif self.phase == 'await_unplug':
            if state == 0:
                self.phase = 'discharging'
                self.load_modes.add(row['power_mode'])
            elif state == 1 or mv < 4100:
                self.phase, self.anchor = 'charging', None
        elif self.phase == 'discharging':
            if state != 0:
                self.error = 'external_power_during_discharge'
                return
            self.load_modes.add(row['power_mode'])
            if mv <= 3400:
                if self.anchor is None:
                    self.anchor = now
                if now-self.anchor >= 60:
                    self.phase = 'usable_low_endpoint'
            else:
                self.anchor = None


def main():
    import serial
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--hours', type=float, default=12)
    parser.add_argument('--partial-discharge', action='store_true',
                        help='Separate diagnostic run: no full-charge anchor or capacity calibration')
    args = parser.parse_args()
    if not 0 < args.hours <= 24:
        parser.error('--hours must be > 0 and <= 24')
    args.out.mkdir(parents=True, exist_ok=True)
    status = args.out/'status.json'
    log = (args.out/'samples.jsonl').open('x')
    model = Cycle(partial_discharge=args.partial_discharge)
    start = time.monotonic()
    port = None
    lost_at = None
    count = valid = 0
    finished = False
    invalid_since = None

    def save(event, **values):
        log.write(json.dumps({'event': event, 'elapsed_s': round(time.monotonic()-start, 3),
                              'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(), **values})+'\n')
        log.flush()

    def publish():
        data = {'pid': os.getpid(), 'phase': model.phase, 'error': model.error,
                'samples': count, 'usable_samples': valid, 'elapsed_s': round(time.monotonic()-start),
                'connected': port is not None, 'finished': finished,
                'identity': model.identity, 'load_modes': sorted(model.load_modes),
                'reference_soc_available': False, 'calibration_verified': False,
                'experiment_kind': 'partial_discharge' if args.partial_discharge else 'full_cycle',
                'full_charge_anchor_available': False,
                'updated_utc': datetime.datetime.now(datetime.timezone.utc).isoformat()}
        temporary = status.with_suffix('.tmp')
        temporary.write_text(json.dumps(data, indent=2))
        temporary.replace(status)

    save('start', port=args.port, max_hours=args.hours,
         experiment_kind='partial_discharge' if args.partial_discharge else 'full_cycle',
         full_charge_anchor_available=False,
         firmware_changes=False, reference_soc_available=False,
         note='USB polling affects load; 3400mV is usable-range endpoint, not physical empty')
    publish()
    try:
        while time.monotonic()-start < args.hours*3600:
            tick = time.monotonic()
            try:
                if port is None:
                    port = serial.Serial(args.port, 115200, timeout=.2, write_timeout=2, exclusive=True)
                    port.reset_input_buffer()
                    save('connected')
                    lost_at = None
                port.write(b'\nstat\npower\nbat\n')
                buf = b''
                while time.monotonic()-tick < 3:
                    buf += port.read(4096)
                    if len(buf) > 32768:
                        raise RuntimeError('unexpected_console_volume')
                lines = [s.strip() for s in buf.decode(errors='replace').splitlines()]
                lines = [s for s in lines if s.startswith(('STAT ', 'POWER ', 'now mv='))]
                row = {'raw': lines, **classify(lines)}
                if row['usable']:
                    invalid_since = None
                elif invalid_since is None:
                    invalid_since = tick
                elif tick-invalid_since > 120:
                    model.error = 'no_valid_telemetry_over_120s'
                before = model.phase
                model.feed(row, tick-start)
                count += 1
                valid += row['usable']
                save('sample', phase=model.phase, **row)
                if before != model.phase:
                    save('phase', previous=before, phase=model.phase)
                if model.phase == 'usable_low_endpoint' or model.error:
                    break
            except (OSError, serial.SerialException) as exc:
                if port:
                    port.close()
                port = None
                model.gap()
                if lost_at is None:
                    lost_at = time.monotonic()
                    save('disconnected', error=str(exc))
                if time.monotonic()-lost_at > 120:
                    model.error = 'connection_lost_over_120s'
                    break
            publish()
            time.sleep(max(0, 5-(time.monotonic()-tick)))
        else:
            model.error = 'experiment_time_limit'
    except (KeyboardInterrupt, RuntimeError) as exc:
        model.error = str(exc) or 'operator_interrupted'
    finally:
        if port:
            port.close()
        port = None
        finished = True
        save('end', phase=model.phase, error=model.error, calibration_verified=False)
        publish()
        log.close()
    return 2 if model.error else 0


if __name__ == '__main__':
    raise SystemExit(main())
