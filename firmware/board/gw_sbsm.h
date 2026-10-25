// 2.6.19 standby state machine (pure C, no LVGL/IDF): one owner for entering/leaving the standby clock.
// Spec: docs/relay/STANDBY-SPEC.md. Board/UI code feeds events + context and executes the returned actions.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { SB_ACTIVE=0, SB_ENTERING, SB_STANDBY, SB_WAKING } sb_state_t;
typedef enum { SB_EV_TICK=0, SB_EV_ACTIVITY, SB_EV_RED_CLICK, SB_EV_TOUCH, SB_EV_KEY_YELLOW, SB_EV_KEY_BLUE, SB_EV_RED_DOUBLE,
               SB_EV_NAV_START, SB_EV_NAV_TURN, SB_EV_VOICE, SB_EV_SUMMARY, SB_EV_ANIM_DONE } sb_event_t;
enum { SB_A_SHOW=1, SB_A_HIDE=2, SB_A_DIM=4, SB_A_RESTORE=8, SB_A_SWALLOW=16, SB_A_GOTO_NAV=32, SB_A_GOTO_PREV=64,
       SB_A_CLOSE_OVERLAY=128, SB_A_HAPTIC=256, SB_A_PASS=512 /* event also does its normal job (blue = talk, red double = power menu) */, SB_A_GOTO_CHAT=1024 };
typedef struct { bool nav_active, ride_active, voice_busy, overlay_open; int page; } sb_ctx_t;
enum { SB_IDLE_MS=20000, SB_IDLE_RIDE_MS=0 /* never */, SB_ENTER_MS=360, SB_EXIT_MS=220, SB_DEBOUNCE_MS=250 };
typedef struct { sb_state_t st; int64_t last_input_ms, last_change_ms; int prev_page; } gw_sbsm_t;
void gw_sbsm_init(gw_sbsm_t* m, int64_t now_ms);
int  gw_sbsm_event(gw_sbsm_t* m, sb_event_t ev, const sb_ctx_t* c, int64_t now_ms);  // returns SB_A_* mask
const char* gw_sbsm_state_str(sb_state_t s);
#ifdef __cplusplus
}
#endif
