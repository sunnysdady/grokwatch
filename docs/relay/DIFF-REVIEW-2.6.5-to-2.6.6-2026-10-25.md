# Differential review: running 2.6.5 (d1d6f8e, ota_1) -> candidate 2.6.6

Scope: everything between d1d6f8e and the closeout commit under firmware/, firmware-patch/, server/.
Status: 已写代码 + 已过自动测试 + 已过模拟器(主机 LVGL 预览). **未刷机，未过真机。**

## Candidate
| build | file | size | slot 0x3f0000 |
|---|---|---|---|
| public (GW_TEST_CONSOLE=0, sdkconfig.ble) | xiaozhi-2.6.6-public.bin | 3,354,624 | 81.2% |
| console (GW_TEST_CONSOLE=1) | xiaozhi-2.6.6-console.bin | 3,357,088 | 81.3% |
SHA256 in the private candidates dir (work/candidates/2.6.6/SHA256SUMS) and the closeout report.
Running baseline console app 2.6.5: 3,349,136 bytes, sha256 e7ed8a83…69667. Growth ~+8 KB (Aora contour data ~10.7 KB const).

## Changes and findings
1. **gw_grok.c – Aora face (from 9d7199a) + seamless eyes (this round).**
   - Old: 46 lv_draw_triangle per eye -> AA seams on shared edges (visible stripes). New: both eyes scanline-rasterized
     (4 vertical subsamples, exact horizontal coverage) into one A8 mask, blended once via lv_draw_image (A8 + recolor path,
     lv_draw_sw_img.c:240). No internal edges -> no seams. Float vertices -> sub-pixel motion.
   - Body gradient removed (RGB565 gave ~4 visible bands over 200 px); flat tonal body per state.
   - Memory: mask 220*220 = 48,400 B allocated once in gw_grok_create from PSRAM (heap_caps MALLOC_CAP_SPIRAM), never freed,
     never per frame. Internal RAM: +~0.9 KB BSS (coverage row) + 768 B static vertex scratch (kept off the LVGL task stack).
     If the PSRAM alloc fails, eyes are skipped (body still drawn) – degraded, not a crash.
   - Correctness assumptions checked in the build config: LV_CACHE_DEF_SIZE=0 (mask image not cached stale),
     LV_DRAW_SW_DRAW_UNIT_CNT=1 (mask rewritten only between sequential draw passes). **If either changes, revisit.**
   - Cost per frame: only bbox rows of the eyes are cleared/rasterized; timer 33 ms, paused when hidden
     (preview asserts zero flushes while hidden, 250 hide/show cycles, ASan/UBSan clean, heap delta -8 B).
   - Severity: none blocking. Risk: device render time of the avatar not measured (P1 fps is stage 3).
2. **OTA re-delivery guard – firmware (gw_health.*, m5stack_stopwatch.cc, application.cc).**
   - NVS "gw"/"pv_ver" = version of a PENDING_VERIFY boot; next boot folds: pending && !validated -> "gw"/"bad_ver".
     Covers panic, WDT, brownout, health-check rollback and bootloader rollback (fold runs on the image that boots next,
     which must be >= 2.6.6; the current 2.6.5 cannot record a failure of 2.6.6 itself – server side covers that).
   - application.cc (public build only; console build never installs OTA): refuses HasNewVersion when version == bad_ver.
     Verified string "Refusing OTA" present in public bin, absent in console bin.
   - bad_ver cleared only when that same version later validates (e.g. after a USB fix flash).
   - board.gw_boot JSON gains "bad_ver" (charset-restricted, buffer raised to 256 B; truncation keeps JSON closed).
   - Host tests: test_gw_health 25/25, all board tests pass (32 + 25 + 16).
3. **OTA guard – server (server/ota_guard.py + server/ota-guard.patch). NOT DEPLOYED.**
   - Per device: block a version if reported as bad_ver, or if gw_boot.pv_failed rose after we offered it, or after 3 offers
     without the device reaching it (covers old firmware without boot record). State data/ota_guard.json, atomic write.
   - Tested: 6 unit tests in temp; patched handler py_compile in work/server-ota-guard/new. Not exercised through a live
     aiohttp request against the real config (deliberately not touching the running service).
   - Original ota_handler.py sha256 5889d9dd…6c48 (kept in work/server-ota-guard/orig).
4. **Version** PROJECT_VER 2.5.x/2.6.5 -> 2.6.6 (outside-board diff: firmware-patch/2.6.6-outside-board.diff).

## Not covered / open
- Real-device boot, 30 s validation, network asset load, long run, rollback drill of 2.6.6: 待真机验收.
- Real swipe fps and Grok-page render time: 待真机验收 (fps60 field not acceptance evidence).
