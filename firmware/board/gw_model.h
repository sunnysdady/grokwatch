// GrokWatch phone-link data model: pure C, no ESP/LVGL deps (shared by firmware and host simulator).
#pragma once
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
#define GW_MAX_NOTES 9
typedef struct { char app[32], title[96], body[640], time[6]; } gw_note_t;
enum { GW_EV_NAV_START=1, GW_EV_NAV_END=2, GW_EV_NAV_DATA=4, GW_EV_ICON=8, GW_EV_NOTE=16, GW_EV_TURN_SOON=32, GW_EV_TURN_NOW=64,
       GW_EV_SUMMARY=128, GW_EV_SPEED=256, GW_EV_TIME=512 };
typedef struct {
  int64_t last_rx, last_nav, last_spd, last_move, ride_start, spd_t;
  bool nav_active; char dist[32], road[96], dur[32], eta[16]; uint8_t icon[288], icon_buf[288]; bool has_icon;
  char speed[8]; float ride_max, ride_km; int last_m; int phone_batt;
  gw_note_t* notes; int n_notes; uint32_t time_unix; int tz_min;   // notes on heap (PSRAM: >1 KB malloc) uint32_t time_unix; int tz_min;
  char summary[160];                       // filled when GW_EV_SUMMARY fires
} gw_model_t;
extern gw_model_t gw_m;
int  gw_model_rx(const uint8_t* d, int n, int64_t now_us);   // returns GW_EV_* mask
int  gw_model_tick(int64_t now_us);                          // timeouts; returns GW_EV_* mask
bool gw_model_linked(bool ble_link, int64_t now_us);
int  gw_model_ride_min(int64_t now_us);
void gw_model_clear_notes(void);
// UTF-8 safe copy: copies at most cap-1 bytes, never splitting a code point.
void gw_utf8_copy(char* dst, int cap, const char* src, int srclen);
int  gw_frame(uint8_t* out, uint8_t type, const void* payload, int len); // encoder (sim/tests)
#ifdef __cplusplus
}
#endif
