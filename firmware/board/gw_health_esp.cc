#pragma GCC diagnostic ignored "-Wmisleading-indentation"
// ESP glue for gw_health: validates a freshly OTA'd image from local boot health only,
// and keeps a boot record in NVS ("gw"/"bootrec") so a pre-validation crash is visible later without serial.
#include "gw_health.h"
#include <esp_ota_ops.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_log.h>
#include <esp_app_desc.h>
#include <nvs.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
static const char* TAG = "gw_health";
static gw_bootrec_t rec;
static char bad_ver[GW_VER_MAX];
static void str_get(const char* k, char* v){ nvs_handle_t h; size_t n=GW_VER_MAX; v[0]=0;
  if(nvs_open("gw", NVS_READONLY, &h)!=ESP_OK) return; if(nvs_get_str(h, k, v, &n)!=ESP_OK) v[0]=0; nvs_close(h); }
static void str_set(const char* k, const char* v){ nvs_handle_t h; if(nvs_open("gw", NVS_READWRITE, &h)!=ESP_OK) return;
  if(v[0]) nvs_set_str(h, k, v); else nvs_erase_key(h, k); nvs_commit(h); nvs_close(h); }
extern "C" const char* gw_ota_bad_version(void){ return bad_ver; }
static void rec_save(){ nvs_handle_t h; if(nvs_open("gw", NVS_READWRITE, &h)!=ESP_OK) return;
  nvs_set_blob(h, "bootrec", &rec, sizeof rec); nvs_commit(h); nvs_close(h); }
static void rec_load(){ nvs_handle_t h; size_t n=sizeof rec; if(nvs_open("gw", NVS_READONLY, &h)!=ESP_OK) return;
  if(nvs_get_blob(h, "bootrec", &rec, &n)!=ESP_OK || n!=sizeof rec) memset(&rec,0,sizeof rec); nvs_close(h); }
extern "C" const gw_bootrec_t* gw_bootrec_get(void){ return &rec; }
static void task(void*){
  for(;;){
    vTaskDelay(pdMS_TO_TICKS(2000));
    gw_health_sample_t s = { (uint32_t)(esp_timer_get_time()/1000000), gw_health_ui_beats(),
      (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (uint32_t)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) };
    gw_bootrec_checkpoint(&rec, s.uptime_s, s.heap_min_free, s.ui_beats); rec_save();   // ~60 small NVS writes per OTA, only while pending
    gw_health_verdict_t v = gw_health_decide(&s);
    if(v == GW_H_VALID){ ESP_LOGW(TAG, "boot healthy (up %lus, ui %lu, heap %lu/%lu): marking app valid", (unsigned long)s.uptime_s, (unsigned long)s.ui_beats, (unsigned long)s.heap_free, (unsigned long)s.heap_min_free);
      esp_ota_mark_app_valid_cancel_rollback(); gw_bootrec_validated(&rec); rec_save();
      if(gw_ota_refuse(bad_ver, esp_app_get_description()->version)){ bad_ver[0]=0; str_set("bad_ver", bad_ver); } break; }
    if(v == GW_H_ROLLBACK){ ESP_LOGE(TAG, "boot unhealthy after %lus (ui %lu, heap min %lu): rolling back", (unsigned long)s.uptime_s, (unsigned long)s.ui_beats, (unsigned long)s.heap_min_free);
      esp_ota_mark_app_invalid_rollback_and_reboot(); }
  }
  vTaskDelete(nullptr);
}
extern "C" void gw_health_start(void){
  const esp_partition_t* p = esp_ota_get_running_partition(); esp_ota_img_states_t st;
  bool pending = p && esp_ota_get_state_partition(p, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY;
  rec_load();
  { char pv[GW_VER_MAX]; str_get("pv_ver", pv); str_get("bad_ver", bad_ver);   // previous boot's pending version
    if(gw_ota_fold_bad(rec.cur_pending, rec.cur_validated, pv, bad_ver)){ str_set("bad_ver", bad_ver); ESP_LOGE(TAG, "OTA %s never validated: refusing to auto-install it again", bad_ver); }
    char now_ver[GW_VER_MAX]; snprintf(now_ver, sizeof now_ver, "%s", pending ? esp_app_get_description()->version : "");
    if(strcmp(pv, now_ver)) str_set("pv_ver", now_ver); }
  gw_bootrec_on_boot(&rec, (uint32_t)esp_reset_reason(), pending); rec_save();
  char j[256]; gw_bootrec_json_ex(&rec, bad_ver, j, sizeof j); ESP_LOGW(TAG, "bootrec %s", j);
  if(!pending) return;
  ESP_LOGW(TAG, "new image pending verify on %s: local self-check started", p->label);
  xTaskCreate(task, "gw_health", 4096, nullptr, 2, nullptr);
}
