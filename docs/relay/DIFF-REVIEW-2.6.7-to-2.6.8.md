# Diff review 2.6.7 (45fde91) -> 2.6.8 (align)
Files: firmware/board/gw_ultra.c, gw_grok.c, m5stack_stopwatch.cc; PROJECT_VER 2.6.8. No OTA/health/NVS/partition/server changes.
- Second hand: invalidation is 8 small boxes along the hand plus the hub instead of one diagonal bounding box (host dirty area per tick frame 1.9% -> 0.7%).
- To-do: all rows 320 px on one left edge; list block vertically centred for 1..5 rows (was always top-anchored).
- To-do empty state and Control Center groups moved down (+18 / +10 px) to the optical centre (measured ink bbox mid -40 / -20 px).
- GrokBot: character -30 -> -22 px, mic bars/think dots follow (+8), subtitles unchanged.
- SetEmotion is a no-op on this board: the stock emoji box is hidden, so no emoji image is decoded (removes the boot-time robot_2 lookup). Risk: none for UI (box hidden since 2.6.7); returns true like a successful set.
