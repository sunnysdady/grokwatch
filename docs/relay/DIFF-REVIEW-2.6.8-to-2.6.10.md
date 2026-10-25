# Differential review 2.6.8 (5ad4d97, on device) -> 2.6.10 (not flashed)

Scope: firmware/board UI only (gw_grok.c, gw_ultra.c, m5stack_stopwatch.cc via 2.6.9, fonts gn_*.c, sim). No change to BLE (gw_ble.c), phone model (gw_model.c), nav_ui.cc, OTA, NVS, partitions or server. Diffstat:  12 files changed, 10892 insertions(+), 108 deletions(-)

## Behaviour changes
- GrokBot: face v2 (oval eyes, highlight, blush by day), night body dimmed mint; avatar redrawn only while moving (sim: idle 71/300 frames vs 300/300; 2.6.8 device stat fps60=13 on this page). Reply width balanced (no orphan last line); question/reply hierarchy; group lifted clear of page dots.
- Never calls stock SetChatMessage/ClearChatMessages (removes the dark duplicate subtitle bar); SetEmotion no-op (2.6.9).
- Clock: numerals 12/3/6/9 only (gn_hour 40 px, digits 1-9 subset, fallback gu_sm); second hand systemOrange.
- Speed/nav numerals Nunito SemiBold 112/96.
- Control Center: HomeView surface, 104 px tiles, icon scale 1.35 (transform), Apple system colour per function, filled tile when on; night accents dimmed to 52 % keeping hue (is_accent() in col()).
- To-do: <=4 items wrap to 2 lines with variable row height (lv_text_get_size); done items main text at 150/255 opacity.
- Nav empty state: nav arrow + 暂无导航 + 手机开始导航后显示 / App 未连接. Ride hint: small dim App 未连接 / 等待 GPS (was orange 打开 GrokWatch App).

## Risk review
- New fonts all set .fallback=&gu_sm (2.6.4 crash class). Sim AUDIT missing glyphs: 0.
- lv_text_get_size called on every gw_grok_text and todo render: O(log 164) and O(n<=GW_TODO_ROWS) measurements, cheap.
- Avatar invalidation is now conditional: a missed invalidate would freeze a frame, never crash; state change, variant and night toggle still invalidate (morph window 400 ms / explicit).
- Bug found and fixed during sim: style set on g_reply before creation (NULL deref) -- caught by sim segfault, fixed before build.
- Page order unchanged (CHAT, FACE, RIDE, TODO, NAV). Proposal only: NAV after RIDE.
- App partition 0x3f0000; console 3,379,648 B (18 % free), public 3,377,248 B (18 % free).

## Bins
- console e15968320050b64e1c6b880f4f40dccce5fd358b995738810d087458814c56c9 3379648 B
- public 1dd067b76612f99af003bbee19ace48dd2c9c94995d262801b0814c0ac66a8e9 3377248 B
Console bin contains Wi-Fi credentials: never publish.
