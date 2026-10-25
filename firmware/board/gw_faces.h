#pragma once
#include <lvgl.h>
#include <stdbool.h>
#include "gw_model.h"
#define GW_TODO_ROWS 5
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
  int hh,mm,ss,mon,day,wday;        // wday 0=Sun
  int battery; bool charging; bool wifi; bool ble;
  int phone_batt; float ride_km; bool dnd;
  bool nav_active; const char *nav_dist,*nav_dir,*nav_dur,*nav_eta;
  const uint8_t* nav_icon;           // 48x48 1bpp (288 bytes) or NULL
  const char* speed;                 // "" if unknown
  int ride_min;
  // voice to-do (server JSON store via Wi-Fi). todo_state: 0 ok, 1 loading, 2 offline
  int n_todo, todo_open, todo_state; const char* todo_text[GW_TODO_ROWS]; bool todo_done[GW_TODO_ROWS];
  const char* grok_credit;          // legacy, unused since 2.6.11
  int usage_pct; const char* usage_reset; // Grok Bot usage from the app (0x50); usage_pct<0 = unknown -> "—"
  const char* nav_turn;              // vector turn code (2.6.12) or NULL/"" = smoothed bitmap
  int light_s; char light_c;         // nav traffic-light countdown (0 = none), colour R/G/Y (0 = unknown)
} gw_state_t;
// carousel = CHAT..NAV (GW_NCORE pages, shown in dots).
enum { GW_PAGE_CHAT=0, GW_PAGE_FACE, GW_PAGE_RIDE, GW_PAGE_TODO, GW_PAGE_NAV, GW_NCORE, GW_NPAGES=GW_NCORE };
void gw_faces_set_night(bool on);    // Ultra-style red night mode (call before create, or recreate)
void gw_faces_set_slide(bool on);    // false: direct page switch (set by the fps governor when slides drop < 30 fps)
void gw_faces_set_fade(bool on);     // slide+fade (true) or slide-only (false) page transitions
void gw_faces_create(lv_obj_t* parent);
int  gw_faces_sec(int ss);           // 2.6.14: wall-clock second tick (call every ~50 ms)
void gw_dial_finish(lv_layer_t* L,const lv_area_t* a,bool night); // 2.6.14 edge vignette + inner ring + light catch
void gw_faces_set_fallback(const lv_font_t* f); // full CJK font for characters outside the built-in set
void gw_faces_show(int page, const gw_state_t* st);
#define GW_SLIDE_PX 140
#define GW_SLIDE_MS 240
int  gw_faces_page_x(void);         // x offset of the current page (test hook: >0 entering from right)
void gw_faces_dir(int d);            // +1 / -1 : direction of next transition
void gw_faces_sheet(const char* title, const char* body); // modal sheet (NULL = hide)
bool gw_faces_sheet_open(void);
void gw_faces_set_todo_cb(void (*on_tap)(int row)); // tap a to-do row = check off     // expanded, scrollable notification
// Control Center (swipe up on the watch face)
enum { GW_CC_BRIGHT=0, GW_CC_VOLUME, GW_CC_NIGHT, GW_CC_DND, GW_CC_N };
typedef struct { int bright, volume; bool night, dnd; int battery; bool charging, wifi, ble; int phone_batt; } gw_cc_t;
void gw_cc_open(const gw_cc_t* st, void (*on_tap)(int id)); // NULL st = close
bool gw_cc_is_open(void);
// Standby clock (screen idle). minute_count drives the once-a-minute anti-burn-in step.
void gw_standby_create(lv_obj_t* parent);
void gw_standby_set(int hh,int mm,uint32_t minute_count);
void gw_standby_show(bool on);
bool gw_standby_is_on(void);
// Grok character (voice page). state: 0 idle 1 listening 2 thinking 3 speaking 4 offline
void gw_grok_create(lv_obj_t* parent);
void gw_grok_state(int st);
void gw_grok_level(int lvl);          // 0..100 mic level (listening)
void gw_grok_text(const char* role, const char* text); // "user" / "assistant"
void gw_grok_status(const char* s);
void gw_grok_visible(bool on);
void gw_grok_set_night(bool on);
void gw_grok_variant(int v);      // face variant (review only; default 2)    // teal/mint by day, dimmed warm white at night
#ifdef __cplusplus
}
#endif
