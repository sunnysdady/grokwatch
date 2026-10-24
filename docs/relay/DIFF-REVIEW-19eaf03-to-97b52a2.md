# Differential review: 2.6.6 (19eaf03, last flashed to ota_1) -> 97b52a2 (candidate)
Commits: cb93e70 (ring only on face; nav arrow re-apply after rebuild; chat clock y), 2d80261 (Grok page character skin, mouth, night skin, no title), 97b52a2 (HomeView port).
Scope: gw_ultra.c, gw_grok.c, gw_faces.h, sim/ test harness only. No network, NVS, OTA, health, partition or sdkconfig changes. Version string stays 2.6.6.
Findings:
- Rendering only. New draw paths: hands_draw (2 lines + 2 discs, invalidated only on minute change), Grok mouth (1 primitive per frame inside the 220x220 avatar). No new heap per frame; static PSRAM mask unchanged.
- gw_faces_set_night() now also re-skins the Grok page (gw_grok_set_night) - safe before/after gw_grok_create (null-checked).
- Retired Ultra widgets (digital time, gauges, ride arc) are still created but hidden: small RAM cost (~10 objects), removes risk of dangling references in update code. Follow-up: delete them.
- v_has reset on gw_faces_create fixes missing nav arrow after night toggle mid-navigation (bug found in sim).
- Nav distance split at last space into number/unit; strings without a space show number only (unit empty) - acceptable.
- Speed shows integer (gu_hero has no '.'); mockup '24.6' not reproduced to avoid a new font asset.
- Todo keeps 5 compact rows (mockup shows 3 tall rows) to avoid changing tap geometry.
Severity: no blocker found. Unverified on device: real frame time, AMOLED colours, touch.
