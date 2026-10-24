// Pure input / animation policy (host-testable). ESP glue lives in nav_ui.cc and gw_console.cc.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
// ---- Grok (chat) trigger rule ----
// Old rule: ANY tap on the chat page toggled listening (incl. the tap that wakes the screen, taps right after a
// swipe lands on the page, and grazes on the bezel edge). New rule: a deliberate LONG press near the centre.
typedef struct {
  int64_t now_us;
  int64_t press_us;          // how long the finger was down
  int x, y;                  // press point (panel coords, 466x466, centre 233,233)
  int64_t since_gesture_us;  // time since the last recognised swipe
  int64_t since_wake_us;     // time since the screen left standby / was woken
  int64_t since_page_us;     // time since the current page was entered
  bool long_press;           // LVGL reported LONG_PRESSED (not just CLICKED)
} gw_tap_t;
typedef enum { GW_TAP_OK=0, GW_TAP_SHORT, GW_TAP_EDGE, GW_TAP_AFTER_SWIPE, GW_TAP_AFTER_WAKE, GW_TAP_PAGE_SETTLE } gw_tap_verdict_t;
#define GW_CHAT_MIN_PRESS_US   500000   /* hold >= 0.5 s */
#define GW_CHAT_MAX_R          170      /* inside a 170 px radius of the centre (panel radius 233) */
#define GW_CHAT_GESTURE_GAP_US 800000
#define GW_CHAT_WAKE_GAP_US    1000000
#define GW_CHAT_PAGE_GAP_US    600000
gw_tap_verdict_t gw_chat_tap_decide(const gw_tap_t* t);
const char* gw_tap_verdict_str(gw_tap_verdict_t v);
// ---- Page-transition animation governor ----
// Measures frame time during each slide; if the average is worse than 33.3 ms (< 30 fps) for 2 slides in a row,
// slides are switched off (direct page switch). Re-enabled only after 3 consecutive good slides would be measured
// -- which cannot happen while off, so once off it stays off for this boot (honest: no flapping).
typedef struct { bool slide_on; int bad_streak; int last_fps; int samples; } gw_anim_gov_t;
void gw_anim_gov_init(gw_anim_gov_t* g);
void gw_anim_gov_slide_done(gw_anim_gov_t* g, int frames, int64_t dur_us); // feed one finished slide
#ifdef __cplusplus
}
#endif
