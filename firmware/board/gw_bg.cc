// 2.6.35: watch-face background photo from the app (BLE 0x60-0x63). 466x466 RGB565 lives in PSRAM and is persisted in the
// unused tail of the assets partition (assets.bin ends at ~6.0 MB of 8 MB). Header [magic][fnv] is written last.
#include <string.h>
#include "esp_partition.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "application.h"
#include "board.h"
#include "display.h"
#include "gw_faces.h"
#include "gw_mem.h"
#define BG_N (466*466*2)
#define BG_OFF 0x780000   // ponytail: fixed slot at 7.5 MB; move it if assets.bin ever grows past that
static const uint32_t MAGIC=0x47574247;   // "GWBG"
static uint8_t* px; static bool rx_on;
static uint32_t fnv(const uint8_t* d,int n){ uint32_t h=2166136261u; while(n--){ h^=*d++; h*=16777619u; } return h; }
static const esp_partition_t* part(){ return esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_ANY,"assets"); }
static void show(const void* p){ Application::GetInstance().Schedule([p](){ DisplayLockGuard g(Board::GetInstance().GetDisplay()); if(!g) return; gw_faces_set_bg(p); }); }
// flash erase+write takes seconds: own task, never the BLE host or UI task
static void save_task(void* a){ bool on=a!=nullptr; const esp_partition_t* pt=part();
  if(pt){ esp_partition_erase_range(pt,BG_OFF,on?((8+BG_N+4095)&~4095):4096);
    if(on){ uint32_t h[2]={MAGIC,fnv(px,BG_N)}; esp_partition_write(pt,BG_OFF+8,px,BG_N); esp_partition_write(pt,BG_OFF,h,8); } }
  ESP_LOGI("GW-BG","%s",on?"saved":"cleared"); vTaskDelete(NULL); }
extern "C" void gw_bg_load(void){ const esp_partition_t* pt=part(); uint32_t h[2];
  if(!pt||esp_partition_read(pt,BG_OFF,h,8)!=ESP_OK||h[0]!=MAGIC) return;
  px=(uint8_t*)gw_psram_calloc(BG_N); if(!px) return; esp_partition_read(pt,BG_OFF+8,px,BG_N);
  if(fnv(px,BG_N)!=h[1]){ ESP_LOGW("GW-BG","bad hash"); return; } ESP_LOGI("GW-BG","loaded"); show(px); }
// Called from the BLE host task. Returns true if the frame was a background frame (consumed).
extern "C" bool gw_bg_rx(const uint8_t* d,int n){ if(n<3||d[0]<0x60||d[0]>0x63) return false;
  int l=d[1]|d[2]<<8; const uint8_t* p=d+3; if(l>n-3) return true;
  switch(d[0]){
  case 0x60: if(!px) px=(uint8_t*)gw_psram_calloc(BG_N); show(nullptr); rx_on=px!=nullptr; break;
  case 0x61: if(rx_on&&l>4){ uint32_t o; memcpy(&o,p,4); if(o<BG_N&&(uint32_t)(l-4)<=BG_N-o) memcpy(px+o,p+4,l-4); } break;
  case 0x62: if(rx_on&&l>=4){ uint32_t h; memcpy(&h,p,4); rx_on=false; bool ok=fnv(px,BG_N)==h; ESP_LOGI("GW-BG","end ok=%d",ok);
      if(ok){ show(px); xTaskCreate(save_task,"gw_bg",3072,(void*)1,2,NULL); } } break;
  case 0x63: rx_on=false; show(nullptr); xTaskCreate(save_task,"gw_bg",3072,NULL,2,NULL); break; }
  return true; }
