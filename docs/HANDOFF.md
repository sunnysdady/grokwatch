# GrokWatch 接力说明（2026-10-25 18:5x，Asia/Shanghai）

## 结论（先读）
- **手表现在跑 2.6.23**，在 `ota_1`，状态 VALID（自检 31 s 通过）。console.bin sha256 `7649613f546fb81a7b5ab6b7a65cd3a47347c34af5699f5d01a6004c44c45b22`。assets 分区已写回原始备份（含旧的 WakeNet「你好小智」模型）。正常开机，不在下载模式。
- **2.6.24 没有刷、也没有编译成功**（编译被中断）。改动在本地分支 `wip/2.6.24-smooth`，commit `10f434b`，未推送、未合并。只在模拟器里验证过。
- 下一步：在 wip 分支上把固件编译通过 → 按刷机流程刷 → 用串口看 `GW-FRAME / GW-SLIDE / GW-BENCH` 数据，确认切页 ≥30 fps、内部堆最低值 ≥24576。

## 1. 项目是什么
StopWatch 圆屏手表（M5Stack StopWatch v1.0.1）上的**骑行大屏副表**，固件基于 xiaozhi-esp32 改造。核心三件事：导航（手机 App 用无障碍读高德，经 BLE 发给手表）、码表（速度/里程/用时）、Grok 对话。视觉风格对标 Apple Watch Ultra / Apple HomeView。

## 2. Sunny 的要求和偏好
- 流畅优先：**每个版本都报帧率和内存**（内部堆最低值、PSRAM 空闲）。
- 审美要求极高：视觉对齐（数字用等宽格、基线对齐），每次改 UI 都要出**白天和夜间**的前后对比图。
- 设备**不能自称「小智」**；Grok 的**眼睛必须是圆的**（不要长条或细缝眼）。
- 18:31 以后的新决定：
  - 彻底删掉语音唤醒（「嗨，Grok」和「你好小智」都不要），**只有按键才开麦录音**；
  - GrokBot 页：去掉「长按说话」，脸放在正中，**不显示字幕，只用声音播报**，用表情区分录音、思考、播报；
  - 主力放在整机流畅度上，尤其是切页；
  - **先读说明书和规格书再动手**（硬件结论见 docs/hardware.md）。
- 回报风格：中文、简短、先说结论。

## 3. 红线（必须有 Sunny 的原话同意）
刷机、推 OTA、推 GitHub、发布、动凭证、付款、删除，都要 Sunny 原话同意。
- 密钥、内网地址、NVS、整片 flash 备份**一律不进仓库**。
- `console.bin` 编进了 Wi‑Fi 凭证，**不能公开**；能公开的只有 `public.bin`。
- 串口只能被动读；不要按键、不要点屏、不要自动切页。

## 4. 硬件（详见 docs/hardware.md）
- 机型 StopWatch **v1.0.1**（盒子标签写 V1.01）。背部扩展口 14 脚是 **BAT**（电池针脚），不是 5V IN。
- SoC ESP32‑S3R8，240 MHz 双核，16 MB Flash，8 MB PSRAM。
- 屏幕 CO5300，466×466 圆形 AMOLED，QSPI，RGB565；触摸 CST820B。
- 按键：
  - **红键**：接在 PMIC M5PM1 上。单击/双击读 PMIC 的 IRQ 状态（0x01 click、0x02 wake、0x04 double）和 `BTN_FLAG`（读后自动清零）。实测 IRQ 位在亮屏时不稳定，所以**单击以 BTN_FLAG 为准**，双击用 IRQ 0x04 或 400 ms 内两次 flag 判定，逻辑在 `gw_redkey.c`。初始化时要关掉 PMIC 的长按复位/关机，并解除 CLICK/DOUBLE/WAKE 的屏蔽。
  - **黄键、蓝键**：GPIO 按键，**黄键 G2、蓝键 G1**（官方说明书，Sunny 确认；与 config.h 的 BUTTON1_GPIO=2、BUTTON2_GPIO=1 一致）。三个键都送进待机状态机 `NavUiKey(SB_EV_…)`。
- 三个上限：
  - 屏幕刷新约 60 Hz；
  - 传输：QSPI 40 MHz 时整屏约 21.7 ms（≈46 fps），80 MHz 时理论约 92 fps；现在用 40 MHz，没接 TE；
  - 渲染：2.6.23 实测切页时约 5–6 fps，空闲时慢帧 45–300 ms。**瓶颈在渲染**，不在屏幕也不在传输。

## 5. 代码和版本
- MBA 仓库：`~/Documents/ChatGPT/Grokwatch`（remote `github.com/sunnysdady/grokwatch`）。
  - 板级代码（我们写的）：`firmware/board/` → 编译前 rsync 到 `work/firmware-build/main/boards/m5stack/stopwatch/`。
  - 上游 xiaozhi 工程：`work/firmware-build`（`work/` 不进 git）。对上游的改动直接改在里面；2.6.24 的上游改动另存为 `firmware/patches/2.6.24-app-no-wake-o2.patch`。
- 编译：`work/build-2623.sh`（2.6.24 对应 `work/build-2624.sh`）。它会 `. activate-idf.sh`，再 `ninja -C build-console` 和 `ninja -C build-public`，产物拷到 `work/candidates/<版本>/console.bin|public.bin` 并生成 sha256 文件。sdkconfig 分别是 `sdkconfig.console` 和 `sdkconfig.ble`。
- 模拟器（Mac 上跑）：`cd work/firmware-build && SIM64=1 SIM_WORK=/tmp/gwsim sh main/boards/m5stack/stopwatch/sim/build.sh /tmp/gwsimout`。截图输出为 ppm；Mac 没装 Pillow，转 png/gif 要在 box 上做。单元测试：`sim/test_sbsm.c`、`sim/test_redkey.c`。
- 版本变更：
  - 2.6.19：统一的待机状态机 `gw_sbsm`（规格见 docs/relay/STANDBY-SPEC.md），把散落的唤醒/熄屏逻辑全部改走它。
  - 2.6.20：红键诊断日志 GW-KEY；`normal()` 回调挪到显示锁外执行。
  - 2.6.21：把 PMIC 的 BTN_WAKE（0x02）也当作单击。
  - 2.6.22：新增 `gw_redkey` 解码器（以 BTN_FLAG 为主）；闲置进入待机的渐暗过程中按红键不再唤醒。
  - 2.6.23：骑行速度显示 0.1 km/h，整数和小数分成两个 label，整数用固定宽度格（commit d55b734）；之后补了 docs/hardware.md（2c989a9）。
- 闲置进入待机的时间保持 20 秒（Sunny 要求先不改）。

## 6. 刷机流程和坑
- 只写 `ota_1` 和 `otadata`。写之前先备份并回读 `ota_1`。用 RTC 看门狗复位（用三次 `write-mem` 武装约 2 s 的 WDT），不要用 esptool 的 `--after watchdog-reset`。
- **打开 USB 串口会让芯片复位。** 新固件在 31 秒自检通过前如果被复位，bootloader 会回滚到 ota_0 的原厂 2.5.3。所以刷完 60 秒内不要开新串口；刷机脚本本身在复位前就开着被动串口，而且全程不关。
- 脚本：`work/hardware-test/flash-2612.py`（通用版），用法参照 `work/hardware-test/run-2623.sh`：
  `python3 work/hardware-test/flash-2612.py --bin work/candidates/<v>/console.bin --tag <tag> --expect-ota1-sha <当前ota_1 sha> --expect-ota1-len <长度> --cur-backup <备份名> --want-log GW-KEY --obs-extra 60`
  先跑 `--selftest`。失败时会自动写回备份。一定要用 `nohup setsid` 跑：2.6.21 那次 shell 被打断，结果回滚到了 2.5.3。
- 绝不能让手表停在下载模式；参数要硬编码（之前 zsh 拆参出错，覆盖过备份）。
- 下一次刷 2.6.24 时，当前 ota_1 是 2.6.23：sha 是 `7649613f…5b22`，长度看 `work/candidates/2.6.23/console.bin`；回滚用的备份先读出来，命名为 backup-ota1-2.6.23.bin。
- 备份（都在 MBA，**不进仓库**）：
  - `work/device-backup/full-flash.bin` 整片 sha `d7a01c21f3b574f2f574ba650438022bcfe753db283da3dacb4b525957b97766`
  - `work/device-backup/app-2.5.3.bin` 原厂 sha `41be2146cebdcb578975bd94f631e40f48f40d13c829d9183d42ba3983439a00`
  - `work/device-backup/otadata-original.bin` sha `8ba3b110139f45443d4f268d1a3373ef99a1718b71d51664531b83ee2d4b91a3`
  - `work/hardware-test/backup-ota1-2.6.22.bin` sha `9e44bffd8052209e8015debc530a3e282a90e5746b75af5136c86347d3b29199`（以及更早各版本的 backup-ota1-*.bin）
  - 恢复说明：`work/device-backup/RECOVERY.md`

## 7. assets 分区和唤醒词实验
- assets 分区在 0x800000，大小 8 MB（SPIFFS，放字体、表情和 SR 模型）。
- 实验：刷入 MultiNet「嗨，Grok」后，**内部堆最低值掉到 16 字节**，已中止。随后用 `flash-assets.py --restore` 写回了原备份 `work/hardware-test/backup-assets-20261025-183045.bin`（sha `63cba002e0d00997b866a307b96ca813d15ff2df2fa8ff6130786961bf6de89c`），校验通过。
- Sunny 决定不要语音唤醒。2.6.24 在代码里关掉了唤醒（不用改 assets）。调研资料在 `work/wakeword-research`。

## 8. 2.6.24 状态（分支 wip/2.6.24-smooth，commit 10f434b）
已做完（只在模拟器验证过）：
- 打点：`GW-FRAME`（每帧拆成渲染、flush_cb、等 DMA 三段，加上实测总线 MB/s 和估算的传输时间）；每次切页打一条 `GW-SLIDE`（帧率、两次快照的耗时、落地帧）；开机 45 秒后跑一次 `GW-BENCH`（同内容整屏重绘 3 次）。
- 去掉唤醒和 AFE：在 stopwatch 板上 `EnableWakeWordDetection` 强制为关、忽略 WN/MN 模型、`kUseAfeForVoiceProcessing=false`（只有按键开启的会话才开麦）。见 patches 文件。
- GrokBot 页：脸居中，没有任何文字，录音条和思考点淡入淡出，播报时嘴型做了缓动，切页时不重绘脸。
- 快照平移切页：`gw_slide.c`，新旧两屏各渲染成 PSRAM 里的 RGB565 位图（每张 434 KB），300 ms ease-out，旧页有 140 px 视差。去掉了帧率守护。模拟器估算整屏传输 21.7 ms/帧，传输上限约 46 fps。
- 只给 LVGL 开了 -O2（顶层 CMakeLists）。PROJECT_VER 已改成 2.6.24。
- 对比图：box 上的 `/workspace/gw-face-polish/after24/`（compare_grok_day/night.png、a24slide_*.gif 和 _strip.png、a24st_*.gif）。

没做或未验证：
- 固件**没编译通过**（上一次的错误 `gov` 未定义已经修了，但还没重新编译）；没刷；没有真机数据。
- 模拟器里从 Grok 页切到表盘时，脏区是 145%，原因未查。
- 每次切页要先做两次快照，真机上的延迟未知（看 GW-SLIDE 里的 snap_old/snap_new）。
- SIMD 混合（`CONFIG_LV_DRAW_SW_ASM_CUSTOM` + esp_lvgl_port 的 simd）、更大的绘制缓冲（内部堆紧张）、QSPI 80 MHz、接 TE（G38，44h scanline）、LVGL 绑核，都还没开始。
- 优化顺序：打点 → 去掉唤醒和 AFE → 切页快照平移 → -O2 和 SIMD → QSPI 80 MHz → 接 TE → 绑核。目标是切页 ≥30 fps，内部堆最低值 ≥24576。

## 9. 真机上还没验证的
黄键和蓝键唤醒、点屏唤醒、导航唤醒、红键双击（电源菜单）、骑行速度 0.1 显示、待机功耗。

## 10. 其他
- App `io.grokwatch.app`：在本仓库 `android/`，main 上是 versionName 0.6.2（versionCode 62）。另外有 `app/0.6.3`、`app/0.7.1` 分支（哪个是最新发布版**未确认**）。无障碍读高德的实现在 `NavA11y.kt`，导航帧格式见 docs/nav-0x11-ext.md。
- xiaozhi 服务器和 relay：仓库 `server/` 里有插件（gw_todo.py、gw_quota.py、ota_guard.py）和 INSTALL.md。MBA 上服务器的实际目录、启动命令，仍**未确认**（Slack 令牌的钥匙串条目已确认，见下一条），请向 Sunny 确认，不要猜。
- Slack 语音链路：手表上的提问经服务器转到 Slack，再由 Grok Bot 回复，转成语音播报。走私有频道 **#grokwatch-voice**，令牌在 MBA 钥匙串条目 **`grokwatch-slack`**（Sunny 确认；令牌本身不进仓库、不进日志）。其余细节见 docs/relay/RELAY-2026-10-25.md。
- GitHub `sunnysdady/grokwatch`：开源前要检查 git 历史里有没有密钥、内网地址、console.bin、NVS 或备份；公开的固件只能是 public.bin。当前推送状态**未确认**；不经 Sunny 原话同意不要推。
- 宣传片 v4 在 box 的 `/workspace/gw-promo/v4`。
