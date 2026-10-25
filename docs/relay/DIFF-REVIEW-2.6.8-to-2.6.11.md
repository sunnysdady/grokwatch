# 差异审计 2.6.8 (5ad4d97, 手表现跑) → 2.6.11（本地分支 fw/2.6.11-subtitles，未刷）

范围：2.6.9 + 2.6.10 已有审计（DIFF-REVIEW-2.6.8-to-2.6.10.md）；本文件补 2.6.10 → 2.6.11 的改动并复核字幕问题。

## 真机字幕重叠根因（已确认）
2.6.8 的 `SetChatMessage` 先调用原版 `SpiLcdDisplay::SetChatMessage`，原版会重新显示底部深色字幕条（大字），
与 gw_grok 自己的问句/回答小字叠在同一区域；大字条宽于圆屏安全区被截断。照片里的「主上/主人」= 大字「能连上」
与小字叠印的视觉错觉，服务器、prompt、TTS 均未产生「主人」。2.6.9 起不再调用原版（已在 2.6.10 中）。

## 2.6.10 → 2.6.11 改动
| 文件 | 改动 | 风险 |
|---|---|---|
| gw_grok.c | 字幕宽 284→260（GW_SUB_W，圆屏弦宽内留 18 px）；说话态清空顶部状态字，避免状态与字幕双显；新句替换不叠加（原逻辑保持） | 低：纯布局 |
| gw_grok.c | 所有变体/状态统一圆眼 30×30（聆听 33×33），眨眼改为闭眼弧线，不再出现扁条；思考态最扁 30×25 | 低：绘制参数 |
| gw_model.c/.h | 0x11 可选第 5 段 `L=秒;C=R/G/Y`；新增 0x50 用量帧（RAM）；`gw_model_light_left()`；导航结束清红绿灯 | 低：越界已检查（v 1..300、长度受 cs() 限制），旧 App 无第 5 段时字段为 0 |
| gw_faces.h | gw_state_t 增加 usage_pct/usage_reset/light_s/light_c（grok_credit 保留不用） | 低 |
| gw_ultra.c | 导航页红绿灯胶囊（箭头右侧，默认隐藏）；无 ETA 时只显示「剩余 X」，全空则隐藏；主表盘胶囊改为「Grok Bot 42% · 18:00」/「Grok Bot —」，宽度自适应 180–300 | 低 |
| nav_ui.cc | 渲染时填新字段（每秒 tick 已在导航页重绘，倒计时随之递减） | 低 |
| sim/sim.c, test/test_gw_2611.c | 模拟场景与单测 | 无（不进固件） |
| CMakeLists.txt（构建树） | PROJECT_VER 2.6.11 | 无 |

未改：网络/OTA/NVS 键/BLE 服务 UUID/待办/其他页面布局。

## 验证
- host 单测：原有 25+16 项通过；新增 test_gw_2611 共 12 项 0 失败（红绿灯解析、递减、非法值、旧帧兼容、0x50 正常/未知、导航结束清零）。
- 模拟器（LVGL 9.5，box -m32）：GrokBot 页 idle 62/300 帧重绘、speaking 300/300，每帧脏区 22.3%（与 2.6.10 一致）；LVGL 堆 31.5 KB，峰值 51.8 KB，碎片 1%；滑页传输约 25 fps 上限（未变）。
- 构建：console 0x3394C0 B（82%），public 0x338B70 B（82%），均 18% 余量。public.bin 敏感串扫描仅命中上游常量 `ESP_ERR_WIFI_PASSWORD` 与配网页 `http://192.168.4.1`（与 2.6.10 相同）。
- 未验证：真机渲染与帧率、红绿灯实车数据（需 App 0.6.2 无障碍读取）、App 侧 0x50 尚未实现。

## 刷机（待 Sunny 审图点头后）
沿用安全流程：只写 ota_1 + otadata，WATCHDOG 复位，不碰 NVS；回滚见 work/device-backup/RECOVERY.md。
