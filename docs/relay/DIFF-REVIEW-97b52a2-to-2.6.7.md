# Diff review 97b52a2 -> 2.6.7 (fix2)

Scope: firmware/board/{gw_grok.c,gw_ultra.c,gw_faces.h,m5stack_stopwatch.cc,sim/sim.c}, CMake PROJECT_VER 2.6.7. No changes to application.cc, OTA, health, NVS, partitions, or the server (server/gw_quota.py is an undeployed draft).

## Root cause: the character was never shown on the device
- `gw_grok_create()` creates the page HIDDEN and the timer paused. On the device nothing ever called `gw_grok_visible/state/text`: grep of main/ shows the only callers are sim/sim.c and sim/aora_preview.c. So since 2.6.x the chat page has shown the stock xiaozhi SpiLcdDisplay UI (emoji_box_ robot emoji, orange status, chat_message_label_), confirmed by the serial line `EmojiCollection: Emoji not found: robot_2` in observe-hv.raw. Every change to gw_grok.c was only ever visible in the simulator.
- Fix: `gw_faces_show()` calls `gw_grok_visible(page==GW_PAGE_CHAT)` on page enter; `RoundLcdDisplay` forwards `SetStatus` (STANDBY->idle, or offline when Wi-Fi is down; LISTENING; CONNECTING->thinking; SPEAKING) and `SetChatMessage`/`ClearChatMessages` (user/assistant) to gw_grok under DisplayLockGuard, and hides emoji_box_/status_bar_/bottom_bar_.

## Other changes
- Speed shows one decimal: gu_hero has no '.' glyph (cmap ofs 0 for U+002E), so the dot is a 16 px disc between two gu_hero labels in a flex row. No new font data.
- Nav unit: drawn in gu_num (same face as the numeral) scaled 144/256 (=54/96, the mockup ratio), pivoted on the baseline, 11 px gap, group centred by measured width.
- GrokBot hint "长按说话"; the question is 1 line (ellipsis), the reply has its own 2-line scrolling window under it, so it can no longer overlap the question.
- Face: thin accent second hand with a 180 ms overshoot tick, invalidating only its own bounding box; 12 static hour numerals (gu_sm, ASCII digits all present; 3/6/9/12 brighter); the "today km" pill became "Grok 额度 —" (s->grok_credit, NULL today -> em dash).
- To-do empty hint "长按说话 / 添加待办" (it was 「」 text with glyphs the sim font lacks).

## Risk review
- Thread safety: the new display overrides take DisplayLockGuard like the existing SetEmotion override. gw_grok_* guard g_root==NULL (before NavUi builds the UI).
- Memory: no new allocations; LVGL host heap 49.6 KB used, 100 KB peak (unchanged class).
- Transformed v_unit label renders through a small layer only when the nav area redraws.
- Second hand: host dirty area 1.9 %/frame during the tick vs 0 % idle; est. QSPI 0.4 ms/frame.
