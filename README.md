# GrokWatch

> ⚠️ **开发中（pre-1.0）**：功能和接口随时会变。本项目与 xAI 无关联，“Grok” 名称为暂定名。
>
> ⚠️ **Work in progress (pre-1.0)**: expect breaking changes. Not affiliated with xAI; the name “Grok” is provisional.

[中文](#中文) | [English](#english)

## 中文
GrokWatch 是给 M5Stack StopWatch (ESP32-S3) 做的骑行大屏伴侣固件，配合 Android 应用使用，基于 [xiaozhi-esp32](https://github.com/78/xiaozhi-esp32)。
- 导航和码表：高德导航箭头、距离、ETA，转弯前震动，显示速度、骑行计时、里程和骑行总结
- Grok 语音：按蓝色键说话，有语音回复和字幕，离线时会显示
- 上滑打开控制中心：亮度、音量、夜间模式（低亮度暖白）、勿扰、电量、BLE/Wi-Fi
- 待机时钟：息屏后纯黑底翻页卡片时:分，整分钟翻牌，每分钟微移防烧屏
- 表盘背景：在 App 里「表盘 > 更换背景图片」选照片，在圆形模板内拖动、缩放并预览，确认后经 BLE 发到手表，重启后保留。导入保留原图亮度，以抖动量化减轻 RGB565 色带；夜间模式仍会调暗显示
- 切页：手指跟随滑动，QSPI 80 MHz + TE 同步出帧；纯黑无边框页面
- 自动翻转：挂绳倒挂时按 BMI270 加速度计自动把屏幕转 180°
- 不转发普通通知：App 只读取高德 / Google 地图的导航通知

### 编译
1. 把 `firmware/board` 复制到 `xiaozhi-esp32/main/boards/m5stack/stopwatch/`，然后打 `firmware/xiaozhi-esp32.patch`
2. （可选）把 `secrets_gen.h.example` 复制成 `secrets_gen.h` 预置 Wi-Fi；自己的 OTA 地址放在不入库的 `sdkconfig.private`
3. `idf.py -B build -D SDKCONFIG=sdkconfig.ble build`（需要 `CONFIG_LV_USE_FONT_COMPRESSED=y`）
4. Android：`cd android && ./gradlew assembleRelease`（签名用 `GW_KEYSTORE_PROPS`）

### OTA
把 `build/xiaozhi.bin` 放进 xiaozhi-server 的 `data/bin/<model>_<version>.bin`，设备启动时检查到新版本就会升级。已开启回滚。

### 隐私
App 只读取地图导航通知，只通过 BLE 发给手表，不上传；普通通知不读取、不转发。仓库里不含密钥。

### 模拟器
`sim/run.sh`，截图在 `docs/screens/`。

### 本地回归测试
- `sh firmware/board/test/run.sh`：导航/骑行模型、启动健康检查、输入、方向和完整待机唤醒生命周期（需要 C 编译器）。
- `sh android/test/run.sh`：图片 RGB565 编码和裁切坐标回归（需要 JDK 17+，无需 Android SDK）。此入口不替代 Android APK 构建和真机预览验收。

## English
GrokWatch is cycling big-screen companion firmware for the M5Stack StopWatch (ESP32-S3), with an Android companion app. It is based on xiaozhi-esp32.
- Navigation and cycling computer: AMap arrows, distance, ETA, pre-turn haptics, plus speed, ride timer, km and a ride summary.
- Grok voice: push the blue button to talk; replies are spoken with subtitles, and offline state is shown.
- Swipe up for the Control Center: brightness, volume, night mode (dim warm white), DND, battery, BLE/Wi-Fi.
- Standby clock: pure-black flip-card HH:MM when idle (digits flip on the minute), shifted a few pixels each minute against burn-in.
- Watch-face background: pick a photo in the app (Face > Change background), sent over BLE, kept across reboots.
- Page slides follow the finger, with QSPI at 80 MHz and TE-synced frames; pages are pure black and frameless.
- Auto 180° flip from the BMI270 accelerometer when the watch hangs upside down on the lanyard.
- No notification forwarding: the app only reads AMap / Google Maps navigation notifications.

Build: same steps as the Chinese section above. OTA: drop the bin into the server's `data/bin`; app rollback is enabled. Privacy: only map navigation notifications are read, sent over BLE only and never uploaded; no secrets are in the repo.

License: MIT (see LICENSE), compatible with xiaozhi-esp32 (MIT).

Android 完整验证（需 JDK 21、Android SDK 36）：

```sh
sh android/test/verify.sh
# 仅对指定本地模拟器执行页面测试，不操作 USB 手表
ANDROID_SERIAL=emulator-5554 sh android/test/verify.sh --emulator
```

首页保留连接状态和表盘入口；连接权限、骑行设置可展开查看，开发者选项默认收起，集中放置测试导航、更新源与解析日志。

固件电量/充电与唤醒画面回归（LVGL 9.5.0 源码就绪后）：
```sh
GW_TEST_UI=1 SIM64=1 LVGL_DIR=/absolute/path/to/lvgl \
SIM_WORK=/tmp/grokwatch-sim GW_UI_OUT=/tmp/grokwatch-ui \
sh firmware/board/test/run.sh
```
该入口同时运行常规逻辑测试、照片日夜亮度、电量/充电截图、闪电动画帧差、点击关闭，以及翻页中唤醒后的画面一致性检查。主机截图不能代替屏幕实机验收。
