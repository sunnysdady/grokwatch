#pragma once
#include <lvgl.h>
#include <stdbool.h>
#include "gw_model.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
  int hh,mm,ss,mon,day,wday;        // wday 0=Sun
  int battery; bool charging; bool wifi; bool ble;
  int n_notes; const gw_note_t* notes;   // newest first, up to GW_MAX_NOTES
  int phone_batt; float ride_km; bool dnd;
  bool nav_active; const char *nav_dist,*nav_dir,*nav_dur,*nav_eta;
  const uint8_t* nav_icon;           // 48x48 1bpp (288 bytes) or NULL
  const char* speed;                 // "" if unknown
  int ride_min;
} gw_state_t;
enum { GW_PAGE_CHAT=0, GW_PAGE_FACE, GW_PAGE_RIDE, GW_PAGE_NOTES, GW_PAGE_NAV, GW_NPAGES };
void gw_faces_set_night(bool on);    // Ultra-style red night mode (call before create, or recreate)
void gw_faces_set_fade(bool on);     // slide+fade (true) or slide-only (false) page transitions
void gw_faces_create(lv_obj_t* parent);
void gw_faces_set_fallback(const lv_font_t* f); // full CJK font for characters outside the built-in set
void gw_faces_show(int page, const gw_state_t* st);
void gw_faces_dir(int d);            // +1 / -1 : direction of next transition
void gw_faces_sheet(const char* title, const char* body); // modal sheet (NULL = hide)
bool gw_faces_sheet_open(void);
void gw_faces_note_open(int i);     // expanded, scrollable notification
// Control Center (swipe up on the watch face)
enum { GW_CC_BRIGHT=0, GW_CC_VOLUME, GW_CC_NIGHT, GW_CC_DND, GW_CC_N };
typedef struct { int bright, volume; bool night, dnd; int battery; bool charging, wifi, ble; int phone_batt; } gw_cc_t;
void gw_cc_open(const gw_cc_t* st, void (*on_tap)(int id)); // NULL st = close
bool gw_cc_is_open(void);
// Grok character (voice page). state: 0 idle 1 listening 2 thinking 3 speaking 4 offline
void gw_grok_create(lv_obj_t* parent);
void gw_grok_state(int st);
void gw_grok_level(int lvl);          // 0..100 mic level (listening)
void gw_grok_text(const char* role, const char* text); // "user" / "assistant"
void gw_grok_status(const char* s);
void gw_grok_visible(bool on);
#ifdef __cplusplus
}
#endif
