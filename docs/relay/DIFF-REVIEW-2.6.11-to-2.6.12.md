# 差异审计 2.6.11 (5b521bb) → 2.6.12（本地分支 fw/2.6.12-smooth，未刷）

| 文件 | 改动 | 风险 |
|---|---|---|
| gw_model.c/.h | 0x11 扩展新增可选 `T=` 转向码（≤3 字符，越界忽略） | 低 |
| gw_faces.h / nav_ui.cc / sim.c | gw_state_t.nav_turn | 低 |
| gw_ultra.c | 导航：T 码已知 → 矢量箭头（统一 18 px 笔画、圆头线帽、圆角头部 3.5 px，S/L/R/SL/SR/HL/HR/UL/UR/A）；未知 → 1-bit 位图 3×3 柔化 + 2× 双线性 → 96×96 A8 + 图像抗锯齿（不再最近邻放大 2.5×） | 低：新增 9 KB 静态缓冲（v_px 48²→96²），绘制只在转向变化时重做 |
| gw_ultra.c | 转向变化 200 ms ease-out：透明度 0→1 + 尺寸 0.94→1（矢量在几何里缩放、位图用 image scale，不开 transform layer，无额外大缓冲） | 低 |
| gw_ultra.c | 距离/单位/路名文字未变就不 set_text（不闪） | 无 |
| gw_ultra.c | 红绿灯胶囊：出现 180 ms 淡入、消失 140 ms 淡出后隐藏（退出快于进入） | 低：动画完成回调只隐藏对象 |
| gw_ultra.c | 秒针跳秒 overshoot → ease-out（不弹） | 无 |
| gw_grok.c | 新字幕/问句 180 ms 淡入（替换不叠）；状态文字 160 ms 淡入；聆听时字幕淡出避开音量条（2.6.11 前音量条与问句重叠）；眼睛大小随状态约 200 ms 缓动 | 低 |
| CMakeLists.txt（构建树） | 2.6.12 | 无 |

未改：配色、布局方案、网络/OTA/NVS/BLE UUID/待办。

## 验证
- host 单测 12 + 32/25/16 项全过。
- 模拟器（与 2.6.11 同脚本）：转向变化+胶囊淡入 18 帧平均脏区 5.7%/帧（≈1.2 ms QSPI）；聆听 24.7%（2.6.11 24.2%）；GrokBot 待机 10 s 重绘 81/300 帧（2.6.11 62，多出的是字幕/状态淡入帧），每帧脏区不变 22.3%；LVGL 堆 31.8 KB 峰值 52.0 KB（2.6.11 31.6/51.8）。
- 构建：console 0x33A530（82%），public 0x339C20（82%），18% 余量；public 敏感串扫描与 2.6.11 相同（仅上游常量）。
- 未验证：真机帧率；App 尚未发送 T= 码，现阶段真机走"平滑位图"分支。

## 注意（2.6.11 刷机结果）
2.6.11 写入 ota_1 校验通过，但首启未被确认（otadata ota_1 项 = ABORTED），bootloader 回到 ota_0 2.5.3；已按流程写回 2.6.8 并校验。根因未确认，2.6.12 刷前需先查清（见回报）。

## 2.6.12b (polish + 2.6.11 first-boot ABORTED fix) — 2026-10-25

### Root cause analysis of 2.6.11 first boot ABORTED (computer-side only)
- otadata: select-console-new = ota_1 state NEW(0); after the 2.6.11 boot ota_1 = ABORTED(4). The bootloader only writes
  ABORTED when it finds PENDING_VERIFY at the *next* boot, i.e. 2.6.11 was reset at least once before gw_health marked it
  valid (>=30 s). 2.5.3 was already requesting OTA ~10 s after the watchdog reset -> the extra reset was within seconds.
- Unchanged 2.6.8 -> 2.6.11: sdkconfig.console (mtime before the 2.6.8 build), partition table, bootloader, rollback,
  PSRAM, WDT, gw_health / esp_ota_mark_app_valid path (gw_health_esp.cc not in the diff); no new NVS writes (0x50 usage
  is RAM only); app 0x339xxx < 0x3f0000 partition; image_info valid, same entry; segment split differs only in how
  DRAM/IRAM were merged (same totals).
- Candidate 1 (tooling): esptool `--after watchdog-reset` raised "Device not configured" (chip reset mid-command; WDT
  armed with a 2000-cycle timeout), and its exception/teardown path closed the port while the app was already
  PENDING_VERIFY -> possible second reset. flash-2612.py removes that path.
- Candidate 2 (firmware, FIXED here): 2.6.10 added `gw_f_status()` which hands the *theme* font pointer
  (f20_rt.fallback) straight to the grok status label `g_stat`. The 2.6.5 font-UAF fix only re-points the
  fallback of the f20_rt/f21_rt copies; `g_stat` kept the raw pointer. When the theme font is swapped at boot
  (assets apply) the old font is freed and the next layout of `g_stat` calls a dangling get_glyph_dsc -> the same
  InstrFetchProhibited as 2.6.4. Fix: `gw_grok_font_changed()` re-points `g_stat` from the SetTextFont callback
  (before the old font is released).
- Not verifiable on the computer: QEMU has no octal PSRAM model; no coredump partition. Confirmation needs the
  first-boot serial log, which flash-2612.py now captures.

### Visual polish
- Vector arrows: own SDF rasterizer -> 120x120 A8 (lazy lv_malloc, rebuilt only when T changes), turtle path with
  true circular-arc turns (concentric inner/outer edge), round caps, rounded-tip head (exact offset triangle, r=3),
  3x3 supersampled coverage, ink-bbox auto-centring; turn_in scale applied via image scale.
- HL/HR redrawn (135 deg bend r=17, longer exit leg); UL/UR now bend toward their own side (UL = left, as CN U-turn);
  A = ring r30 w12 + dot r12.5, centred.
- Bitmap fallback: 3x3 soften -> 2x bilinear -> steep threshold (x3.5) => ~1.2 px AA edge (~1.5 px on screen), no blur.
- Exact Amap 48x48 -> vector mapping: NOT added (no captured real Amap icons to match against).
