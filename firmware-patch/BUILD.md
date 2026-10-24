# GrokWatch firmware — reproducible build

Pinned toolchain / inputs (verified on the build box, 2026-10-24):

| Item | Pin |
|---|---|
| Upstream xiaozhi-esp32 | `39d23aee5b8682f77bbdf9cdc66799456b5ad408` (base of branch) |
| ESP-IDF | v6.1.0, commit `9a97f6c54ec638111ce55cd36581b3c192f15207` |
| Xtensa GCC | esp-15.2.0_20251204 |
| Managed components | `dependencies.lock` (committed) |
| sdkconfig | `sdkconfig.ble` (BLE on, board `CONFIG_BOARD_TYPE_M5STACK_STOPWATCH`) |
| Partition table | `partitions/v2/16m.csv` (ota_0/ota_1 = 0x3f0000 each, assets 8 MB) |

Android app (separate repo): Gradle 8.14.3 (wrapper), AGP 8.13.0, Kotlin 2.2.20, compileSdk/targetSdk 36,
build-tools 36.0.0, JDK 21 (toolchain), junit 4.13.2. Release signing is read from
`~/.grokwatch-release/keystore.properties` (outside the repo).

## Firmware
```sh
. ~/esp/esp-idf/export.sh                 # ESP-IDF v6.1.0
cp main/boards/m5stack/stopwatch/secrets_gen.h.example main/boards/m5stack/stopwatch/secrets_gen.h   # optional
idf.py -B build -D SDKCONFIG=sdkconfig.ble build
```
`sdkconfig.ble` (committed) uses the upstream default `CONFIG_OTA_URL`. For your own OTA server keep a
git-ignored copy: `cp sdkconfig.ble sdkconfig.private`, edit `CONFIG_OTA_URL`, build with `-D SDKCONFIG=sdkconfig.private`.
(The URL can also be set at runtime in NVS `wifi/ota_url`, which takes precedence.)

## Size budget
OTA slot 0x3f0000 = 4,128,768 B; target < 85 %. 2026-10-24: 3,465,472 B = 83.9 %
(was 95.1 %: removed the bold 21 px CJK font gz_21b (~408 KB; headers now use gz_20) and set log level WARN).

## Tests (host, no hardware)
```sh
sh main/boards/m5stack/stopwatch/test/run.sh              # nav/ride model unit + property tests
PYTHON=python3 sh main/boards/m5stack/stopwatch/sim/build.sh /tmp/simout   # LVGL simulator, 28 screens + contact sheet
```
Simulator prereqs: cc with 32-bit multilib (`-m32`), python3 + Pillow, and `managed_components/` populated
(any `idf.py build/reconfigure` does it). No other paths or env vars are used.

## OTA rollback
- Config: `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` / `CONFIG_APP_ROLLBACK_ENABLE=y`; anti-rollback off.
- Two app slots `ota_0`/`ota_1` in `partitions/v2/16m.csv`, `otadata` at 0xd000.
- New image boots as `ESP_OTA_IMG_PENDING_VERIFY`. On this board validation is LOCAL: `gw_health_start()`
  (`gw_health_esp.cc`, started in the board constructor) samples every 2 s and calls
  `esp_ota_mark_app_valid_cancel_rollback()` once uptime >= 30 s, the LVGL 1 s tick has run >= 20 times and
  internal heap (current and minimum) >= 24 KB. If that is not reached by 120 s it calls
  `esp_ota_mark_app_invalid_rollback_and_reboot()`. A crash/reset before validation -> bootloader boots the old slot.
- OTA server reachability is no longer a criterion: the upstream server-based `MarkCurrentVersionValid()` call in
  `main/application.cc` is compiled out for `CONFIG_BOARD_TYPE_M5STACK_STOPWATCH`.
- Tests: `test/test_gw_health.c` (decision logic, host). QEMU drill attempted (Espressif QEMU 9.2.2, esp32s3):
  the image aborts in `cpu_start` because QEMU has no octal PSRAM, so the drill could not reach app code.
  Rollback on device: NOT verified.
