"""DRAFT, NOT DEPLOYED. Remaining xAI prepaid credit for the watch face pill.

Data source (docs.x.ai Management API, Billing):
  GET https://management-api.x.ai/v1/billing/teams/{team_id}/prepaid/balance
  Authorization: Bearer <MANAGEMENT key>   (an inference key for api.x.ai does NOT work)
  -> {"total": {"val": "<USD cents>"}, ...}  (sign convention per xAI docs; verify against Console)
Needs two NEW env vars on the server (none exist today): XAI_MGMT_KEY, XAI_TEAM_ID.
Cached 10 min. Returns None when unavailable; the watch then shows an em dash, never a guess.
Suggested wiring: add "grok_credit" to the existing /gw/todo JSON so the watch gets it on its normal fetch.
"""
import json, os, time, urllib.request
_cache = {"t": 0, "v": None}
def credit_usd():
    key, team = os.environ.get("XAI_MGMT_KEY"), os.environ.get("XAI_TEAM_ID")
    if not key or not team: return None
    if time.time() - _cache["t"] < 600: return _cache["v"]
    try:
        req = urllib.request.Request(f"https://management-api.x.ai/v1/billing/teams/{team}/prepaid/balance",
                                     headers={"Authorization": "Bearer " + key})
        with urllib.request.urlopen(req, timeout=8) as r: cents = int(json.load(r)["total"]["val"])
        v = "$%.2f" % (abs(cents) / 100.0)
    except Exception:
        v = None
    _cache.update(t=time.time(), v=v); return v
