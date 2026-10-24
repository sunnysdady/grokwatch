"""GrokWatch OTA re-delivery guard (server side). Pure logic + small JSON state file.

Stops offering a firmware version to a device once it is known to fail there:
  * the device reports it in board.gw_boot.bad_ver (firmware >= 2.6.6), or
  * the device's gw_boot.pv_failed counter increased after we offered that version, or
  * we offered it MAX_OFFERS times and the device still reports an older version.
State is per device id, stored in data/ota_guard.json. Delete an entry (or the file) to re-allow.
"""
import json
import os
import tempfile
import threading

MAX_OFFERS = 3


class OtaGuard:
    def __init__(self, path, max_offers=MAX_OFFERS):
        self.path = path
        self.max_offers = max_offers
        self._lock = threading.Lock()
        self._state = self._load()

    def _load(self):
        try:
            with open(self.path, "r", encoding="utf-8") as f:
                data = json.load(f)
            return data if isinstance(data, dict) else {}
        except (OSError, ValueError):
            return {}

    def _save(self):
        d = os.path.dirname(self.path) or "."
        os.makedirs(d, exist_ok=True)
        fd, tmp = tempfile.mkstemp(dir=d, prefix=".ota_guard.")
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            json.dump(self._state, f, separators=(",", ":"))
        os.replace(tmp, self.path)

    @staticmethod
    def _boot(data_json):
        board = data_json.get("board") if isinstance(data_json, dict) else None
        boot = board.get("gw_boot") if isinstance(board, dict) else None
        return boot if isinstance(boot, dict) else {}

    def observe(self, device_id, current_version, data_json):
        """Fold one OTA check request into the device state. Returns the set of blocked versions."""
        boot = self._boot(data_json)
        with self._lock:
            st = self._state.setdefault(device_id, {"blocked": [], "offered": None, "offers": 0, "pv_failed": None})
            changed = False
            bad = boot.get("bad_ver")
            if isinstance(bad, str) and bad and bad not in st["blocked"]:
                st["blocked"].append(bad); changed = True
            pvf = boot.get("pv_failed")
            if isinstance(pvf, int):
                if st["pv_failed"] is not None and pvf > st["pv_failed"] and st["offered"] and st["offered"] not in st["blocked"]:
                    st["blocked"].append(st["offered"]); changed = True
                if pvf != st["pv_failed"]:
                    st["pv_failed"] = pvf; changed = True
            if st["offered"] and current_version == st["offered"]:
                st["offered"], st["offers"] = None, 0; changed = True   # upgrade took
            if changed:
                self._save()
            return set(st["blocked"])

    def allow(self, device_id, version):
        with self._lock:
            st = self._state.get(device_id)
            if not st:
                return True
            if version in st["blocked"]:
                return False
            if st["offered"] == version and st["offers"] >= self.max_offers:
                st["blocked"].append(version); self._save()
                return False
            return True

    def record_offer(self, device_id, version):
        with self._lock:
            st = self._state.setdefault(device_id, {"blocked": [], "offered": None, "offers": 0, "pv_failed": None})
            if st["offered"] == version:
                st["offers"] += 1
            else:
                st["offered"], st["offers"] = version, 1
            self._save()
