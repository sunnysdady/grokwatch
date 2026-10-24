// Boot health self-check for OTA rollback. Pure decision logic (host-testable) + ESP glue in gw_health_esp.cc.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { GW_H_WAIT = 0, GW_H_VALID = 1, GW_H_ROLLBACK = 2 } gw_health_verdict_t;
typedef struct {
  uint32_t uptime_s;      // seconds since boot
  uint32_t ui_beats;      // number of LVGL 1 s ticks observed (UI task alive and rendering loop running)
  uint32_t heap_free;     // current free internal heap (bytes)
  uint32_t heap_min_free; // minimum ever free internal heap (bytes)
} gw_health_sample_t;
#define GW_H_MIN_UPTIME_S   30      /* must stay up this long without a reset */
#define GW_H_MIN_UI_BEATS   20      /* UI must have ticked at least this often */
#define GW_H_MIN_HEAP       (24*1024)
#define GW_H_DEADLINE_S     120     /* not healthy by then -> roll back */
// Network/OTA reachability is deliberately NOT a criterion: an offline boot must still validate.
gw_health_verdict_t gw_health_decide(const gw_health_sample_t* s);
void gw_health_ui_beat(void);       // call from a 1 s LVGL timer
uint32_t gw_health_ui_beats(void);
void gw_health_start(void);         // ESP only: spawns the checker if the running image is PENDING_VERIFY
// ---- Boot record (persisted in NVS "gw"; survives OTA). Lets us see a crash that happened BEFORE validation
// even though the bootloader then rolls back: the next boot of a new image reads what the failed boot left.
typedef struct {
  uint32_t boots;          // total boots of GrokWatch images that carry this record
  uint32_t pv_boots;       // boots that started as PENDING_VERIFY (fresh OTA)
  uint32_t pv_failed;      // pending boots that never reached "valid" (reset/crash before validation)
  uint32_t last_rr;        // esp_reset_reason() of THIS boot
  uint32_t fail_up_s;      // uptime the last failed pending boot reached (last 2 s checkpoint)
  uint32_t fail_heap_min;  // its internal-heap minimum at that checkpoint
  uint32_t fail_beats;     // its UI ticks at that checkpoint
  uint32_t crash_rr_count; // boots whose own reset reason was panic / any watchdog / brownout
  // in-flight state of the current boot (written at checkpoints)
  uint32_t cur_pending, cur_validated, cur_up_s, cur_heap_min, cur_beats;
} gw_bootrec_t;
// rr codes follow esp_reset_reason_t: 4 PANIC, 5 INT_WDT, 6 TASK_WDT, 7 WDT, 9 BROWNOUT
bool gw_bootrec_is_crash_rr(uint32_t rr);
void gw_bootrec_on_boot(gw_bootrec_t* r, uint32_t reset_reason, bool pending); // fold previous boot, start new one
void gw_bootrec_checkpoint(gw_bootrec_t* r, uint32_t up_s, uint32_t heap_min, uint32_t beats);
void gw_bootrec_validated(gw_bootrec_t* r);
int  gw_bootrec_json(const gw_bootrec_t* r, char* out, int cap);  // {"boots":..,"pv_failed":..,...}
const gw_bootrec_t* gw_bootrec_get(void);  // ESP only: current record (after gw_health_start)
// ---- OTA re-delivery guard. A version whose pending boot never validated (crash, watchdog, health rollback,
// or bootloader rollback) is remembered in NVS ("gw"/"bad_ver") and is never auto-installed again; the server
// also sees it in board.gw_boot.bad_ver. Cleared only when that exact version later validates (e.g. USB flash).
#define GW_VER_MAX 32
// Pure: true if `offered` must be refused because it equals the remembered failed version.
bool gw_ota_refuse(const char* bad_ver, const char* offered);
// Pure: fold step. prev_pending/prev_validated describe the previous boot; pv_ver is the version it ran.
// Writes the new bad version into bad (cap GW_VER_MAX) and returns true when it changed.
bool gw_ota_fold_bad(bool prev_pending, bool prev_validated, const char* pv_ver, char* bad);
const char* gw_ota_bad_version(void);       // ESP only: "" if none
int  gw_bootrec_json_ex(const gw_bootrec_t* r, const char* bad_ver, char* out, int cap);
#ifdef __cplusplus
}
#endif
