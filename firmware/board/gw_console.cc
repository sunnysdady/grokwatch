#pragma GCC diagnostic ignored "-Wmisleading-indentation"
// USB-Serial-JTAG test console (debug/private builds only: compiled in when GW_TEST_CONSOLE=1, otherwise a no-op).
// Line commands: swipe l|r|u|d, tap x y, longpress x y, page n, stat, fps on|off, touchlog on|off, rot [key v], help
// Touch injection: a second LVGL pointer indev on the same display -> same LVGL press/gesture/click pipeline the
// CST820 indev feeds (only the I2C read is bypassed).
#include <esp_log.h>
extern "C" void gw_console_start(void);
#if GW_TEST_CONSOLE
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <esp_app_desc.h>
#include <esp_system.h>
#include <driver/usb_serial_jtag.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl.h>
#include <esp_lvgl_port.h>
#include "gw_health.h"
#include <esp_memory_utils.h>
void NavUiGoPage(int p); int NavUiPage(); void NavUiTouchLog(bool on); void NavUiFpsLog(bool on); bool NavUiSlideOn();
uint32_t NavUiFrameStats(int n,int* mn,int* av,int* mx);
int GwRotCmd(const char* key,int val,char* out,int n);
int GwPowerDiag(char* out,int n);
int GwBatCmd(char* out,int n);   // 2.6.37
static const char* TAG="gwcon";
static volatile int inj_x=233, inj_y=233; static volatile bool inj_down=false;
static void inj_read(lv_indev_t*, lv_indev_data_t* d){ d->point.x=inj_x; d->point.y=inj_y; d->state=inj_down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED; }
#include "gw_console_write.h"
static int usb_send(const char* s,size_t n){ return usb_serial_jtag_write_bytes(s,n,pdMS_TO_TICKS(200)); }
static void out(const char* s){ (void)gw_console_write(s,strlen(s),usb_send); }
static void outf(const char* f, ...){ char b[320]; va_list a; va_start(a,f); vsnprintf(b,sizeof b,f,a); va_end(a); out(b); }
static void drag(int x0,int y0,int x1,int y1,int ms){ int steps=ms/10; inj_x=x0; inj_y=y0; inj_down=true; vTaskDelay(pdMS_TO_TICKS(40));
  for(int i=1;i<=steps;i++){ inj_x=x0+(x1-x0)*i/steps; inj_y=y0+(y1-y0)*i/steps; vTaskDelay(pdMS_TO_TICKS(10)); } inj_down=false; vTaskDelay(pdMS_TO_TICKS(60)); }
static const char* rr_str(int r){ static const char* s[]={"unknown","poweron","ext","sw","panic","int_wdt","task_wdt","wdt","deepsleep","brownout","sdio","usb","jtag","efuse","pwr_glitch","cpu_lockup"}; return (r>=0&&r<16)?s[r]:"?"; }
static void stat(){
  int mn,av,mx; uint32_t tot=NavUiFrameStats(60,&mn,&av,&mx);
  const esp_partition_t* p=esp_ota_get_running_partition(); esp_ota_img_states_t st=ESP_OTA_IMG_UNDEFINED; if(p) esp_ota_get_state_partition(p,&st);
  const gw_bootrec_t* r=gw_bootrec_get();
  outf("STAT ver=%s part=%s ota=%s up=%llus page=%d slide=%s frames=%lu ft60_ms=%d.%d/%d.%d/%d.%d fps60=%d "
       "int_free=%u int_min=%u psram_free=%u psram_min=%u rr=%s boots=%lu pv_failed=%lu fail_up=%lus crash_boots=%lu\n",
    esp_app_get_description()->version, p?p->label:"?", st==ESP_OTA_IMG_PENDING_VERIFY?"pending":st==ESP_OTA_IMG_VALID?"valid":"other",
    (unsigned long long)(esp_timer_get_time()/1000000), NavUiPage(), NavUiSlideOn()?"on":"off", (unsigned long)tot,
    mn/10,mn%10, av/10,av%10, mx/10,mx%10, av?10000/av:0,
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),(unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),(unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM),
    rr_str((int)esp_reset_reason()),(unsigned long)r->boots,(unsigned long)r->pv_failed,(unsigned long)r->fail_up_s,(unsigned long)r->crash_rr_count); }
// ---- profiler: per-frame breakdown from LVGL display events ----
static int64_t p_t_refr=0,p_t_fw=0; static int64_t p_refr_sum=0,p_wait_sum=0,p_refr_max=0; static uint32_t p_frames=0,p_flushes=0,p_px=0;
static void prof_cb(lv_event_t* e){ int64_t t=esp_timer_get_time(); switch(lv_event_get_code(e)){
  case LV_EVENT_REFR_START: p_t_refr=t; break;
  case LV_EVENT_REFR_READY: if(p_t_refr){ int64_t d=t-p_t_refr; p_refr_sum+=d; if(d>p_refr_max) p_refr_max=d; p_frames++; } break;
  case LV_EVENT_FLUSH_START: { p_flushes++; lv_area_t* a=(lv_area_t*)lv_event_get_param(e); if(a) p_px+=lv_area_get_size(a); } break;
  case LV_EVENT_FLUSH_WAIT_START: p_t_fw=t; break;
  case LV_EVENT_FLUSH_WAIT_FINISH: if(p_t_fw) p_wait_sum+=t-p_t_fw; break;
  default: break; } }
static bool prof_hooked=false; static lv_timer_t* force_t=nullptr;
static void force_cb(lv_timer_t*){ lv_obj_invalidate(lv_screen_active()); }
static void prof(int secs, bool force){
  if(lvgl_port_lock(0)){ lv_display_t* d=lv_display_get_default();
    if(!prof_hooked){ lv_display_add_event_cb(d,prof_cb,LV_EVENT_ALL,nullptr); prof_hooked=true; }
    if(force && !force_t) force_t=lv_timer_create(force_cb,33,nullptr);
    lv_draw_buf_t* b1=lv_display_get_buf_active(d);
    outf("BUF active=%p size=%lu internal=%d dma=%d render_mode=%d\n", b1?b1->data:nullptr, b1?(unsigned long)b1->data_size:0UL,
      b1?(int)esp_ptr_internal(b1->data):-1, b1?(int)esp_ptr_dma_capable(b1->data):-1, (int)lv_display_get_render_mode(d));
    lvgl_port_unlock(); }
  p_refr_sum=p_wait_sum=p_refr_max=0; p_frames=p_flushes=p_px=0; int64_t t0=esp_timer_get_time();
  static TaskStatus_t a[40], b[40]; uint32_t ta,tb; /* static: 2x40 TaskStatus_t (~3KB) overflowed the 4KB gw_console stack */ int na=uxTaskGetSystemState(a,40,&ta);
  vTaskDelay(pdMS_TO_TICKS(secs*1000));
  int nb=uxTaskGetSystemState(b,40,&tb); int64_t dt=esp_timer_get_time()-t0;
  if(force_t){ lvgl_port_lock(0); lv_timer_delete(force_t); force_t=nullptr; lvgl_port_unlock(); }
  outf("PROF %s %ds frames=%lu fps=%.1f flushes=%lu px/frame=%lu refr_avg=%.1fms refr_max=%.1fms flush_wait_avg=%.1fms/frame\n",
    force?"forced-full":"idle", secs,(unsigned long)p_frames, p_frames*1e6/dt,(unsigned long)p_flushes, p_frames?(unsigned long)(p_px/p_frames):0UL,
    p_frames?p_refr_sum/1e3/p_frames:0.0, p_refr_max/1e3, p_frames?p_wait_sum/1e3/p_frames:0.0);
  uint32_t tot=tb-ta; if(!tot) tot=1;
  for(int i=0;i<nb;i++){ uint32_t prev=0; for(int j=0;j<na;j++) if(a[j].xHandle==b[i].xHandle) prev=a[j].ulRunTimeCounter;
    uint32_t d=b[i].ulRunTimeCounter-prev; int pct=(int)(d*100ULL/tot);
    if(pct>=1) outf("TASK %-16s prio=%u core=%d cpu=%d%%\n", b[i].pcTaskName,(unsigned)b[i].uxCurrentPriority,(int)xTaskGetCoreID(b[i].xHandle),pct); }
  out("PROF end\n"); }
static void handle(char* l){
  char c[16]={0}, a[16]={0}; int x=0,y=0; int n=sscanf(l,"%15s %15s %d",c,a,&y); (void)n;
  if(!strcmp(c,"swipe")){ char d=a[0];
    if(d=='l') drag(340,233,110,233,160); else if(d=='r') drag(110,233,340,233,160);
    else if(d=='u') drag(233,340,233,110,160); else if(d=='d') drag(233,110,233,340,160); else { out("ERR swipe l|r|u|d\n"); return; }
    outf("OK swipe %c page=%d\n",d,NavUiPage()); }
  else if(!strcmp(c,"tap")||!strcmp(c,"longpress")){ x=atoi(a); bool lp=c[0]=='l'; inj_x=x; inj_y=y; inj_down=true; vTaskDelay(pdMS_TO_TICKS(lp?800:80)); inj_down=false; vTaskDelay(pdMS_TO_TICKS(60)); outf("OK %s %d %d\n",c,x,y); }
  else if(!strcmp(c,"page")){ NavUiGoPage(atoi(a)); vTaskDelay(pdMS_TO_TICKS(300)); outf("OK page=%d\n",NavUiPage()); }
  else if(!strcmp(c,"stat")) stat();
  else if(!strcmp(c,"prof")) prof(a[0]?atoi(a):3,false);
  else if(!strcmp(c,"proffull")) prof(a[0]?atoi(a):3,true);
  else if(!strcmp(c,"fps")){ NavUiFpsLog(!strcmp(a,"on")); out("OK\n"); }
  else if(!strcmp(c,"touchlog")){ NavUiTouchLog(!strcmp(a,"on")); out("OK\n"); }
  else if(!strcmp(c,"rot")){ char b[200]; GwRotCmd(a,y,b,sizeof b); out(b); }   // rot | rot <rot|rot_ax|rot_sg|rot_mad|rot_gx|rot_gy> v
  else if(!strcmp(c,"power")){ char b[200]; GwPowerDiag(b,sizeof b); out(b); }
  else if(!strcmp(c,"bat")){ static char b[4096]; GwBatCmd(b,sizeof b); out(b); }   // 2.6.37 battery log
  else out("CMDS: swipe l|r|u|d | tap x y | longpress x y | page n | stat | fps on|off | touchlog on|off | rot [key v] | bat\n"); }
static void task(void*){ char line[96]; int n=0;
  for(;;){ uint8_t ch; if(usb_serial_jtag_read_bytes(&ch,1,portMAX_DELAY)!=1) continue;
    if(ch=='\r'||ch=='\n'){ if(n){ line[n]=0; handle(line); n=0; } } else if(n<(int)sizeof line-1) line[n++]=(char)ch; } }
extern "C" void gw_console_start(void){
  usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
  if(usb_serial_jtag_driver_install(&cfg)!=ESP_OK){ ESP_LOGE(TAG,"usj driver install failed"); return; }
  if(lvgl_port_lock(0)){ lv_indev_t* i=lv_indev_create(); lv_indev_set_type(i,LV_INDEV_TYPE_POINTER); lv_indev_set_read_cb(i,inj_read);
    lv_indev_set_display(i,lv_display_get_default()); lvgl_port_unlock(); }
  xTaskCreate(task,"gw_console",6144,nullptr,3,nullptr); ESP_LOGW(TAG,"test console on USB-Serial-JTAG (type help)"); }
#else
extern "C" void gw_console_start(void){}
#endif
