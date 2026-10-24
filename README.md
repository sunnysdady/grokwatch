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
- 待机时钟：息屏后纯黑底大字时:分，每分钟微移防烧屏
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

## English
GrokWatch is cycling big-screen companion firmware for the M5Stack StopWatch (ESP32-S3), with an Android companion app. It is based on xiaozhi-esp32.
- Navigation and cycling computer: AMap arrows, distance, ETA, pre-turn haptics, plus speed, ride timer, km and a ride summary.
- Grok voice: push the blue button to talk; replies are spoken with subtitles, and offline state is shown.
- Swipe up for the Control Center: brightness, volume, night mode (dim warm white), DND, battery, BLE/Wi-Fi.
- Standby clock: pure-black big HH:MM when idle, shifted a few pixels each minute against burn-in.
- No notification forwarding: the app only reads AMap / Google Maps navigation notifications.

Build: same steps as the Chinese section above. OTA: drop the bin into the server's `data/bin`; app rollback is enabled. Privacy: only map navigation notifications are read, sent over BLE only and never uploaded; no secrets are in the repo.

License: MIT (see LICENSE), compatible with xiaozhi-esp32 (MIT).
