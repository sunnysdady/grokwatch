#pragma GCC diagnostic ignored "-Wmisleading-indentation"
// GrokWatch page controller: phone-link model (gw_model.c) -> Ultra UI (gw_ultra.c), gestures, haptics, Control Center.
#include <cstring>
#include <ctime>
#include <sys/time.h>
#include <functional>
#include <esp_log.h>
#include <esp_timer.h>
#include <lvgl.h>
#include <wifi_manager.h>
#include "board.h"
#include "application.h"
#include "display/display.h"
#include "audio/audio_codec.h"
#include "backlight.h"
#include "settings.h"
#include "gw_faces.h"
#define TAG "NavUI"
#define GW_RX_LOG 1  // log type/len of each incoming frame (no content)
extern "C" bool gw_ble_link(void);
static int page=1; static const int NPAGES=5; // 0 chat 1 face 2 ride 3 notes 4 nav
static bool built=false, power_pending=false, dnd=false, night=false; static int64_t last_gesture=0;
static void (*haptic)(int ms)=nullptr; static void (*chat_tap)()=nullptr;
static void render();
static int64_t now_us(){ return esp_timer_get_time(); }
static bool linked(){ return gw_model_linked(gw_ble_link(), now_us()); }
static void buzz(int ms){ if(haptic && !dnd) haptic(ms); }
static void buzz2(){ if(!haptic || dnd) return; haptic(70); lv_timer_create([](lv_timer_t* t){ if(haptic) haptic(70); lv_timer_delete(t); },180,nullptr); }
static void go(int p,int d){ page=(p%NPAGES+NPAGES)%NPAGES; gw_faces_dir(d); if(haptic) haptic(18); render(); }

// ---- Control Center ----
static gw_cc_t cc;
static void cc_fill(){ auto& b=Board::GetInstance(); int lv=0; bool ch=false,dis=false; b.GetBatteryLevel(lv,ch,dis);
  cc.bright=b.GetBacklight()?b.GetBacklight()->brightness():0; cc.volume=b.GetAudioCodec()->output_volume(); cc.night=night; cc.dnd=dnd;
  cc.battery=lv; cc.charging=ch; cc.wifi=WifiManager::GetInstance().IsConnected(); cc.ble=linked(); cc.phone_batt=gw_m.phone_batt; }
static void cc_tap(int id){ auto& b=Board::GetInstance();
  if(id==GW_CC_BRIGHT && b.GetBacklight()){ int v=cc.bright>=100?30:(cc.bright>=60?100:60); b.GetBacklight()->SetBrightness(v,true); }
  if(id==GW_CC_VOLUME){ int v=cc.volume>=90?0:(cc.volume>=60?90:(cc.volume>=30?60:30)); b.GetAudioCodec()->SetOutputVolume(v); }
  if(id==GW_CC_NIGHT){ night=!night; Settings s("gw",true); s.SetInt("night",night); gw_faces_set_night(night); gw_faces_create(lv_screen_active()); }
  if(id==GW_CC_DND){ dnd=!dnd; Settings s("gw",true); s.SetInt("dnd",dnd); }
  if(haptic) haptic(15); cc_fill(); gw_cc_open(&cc,cc_tap); }

static void apply(int ev){
  if(ev&GW_EV_NAV_START){ page=4; buzz(30); }
  if(ev&GW_EV_TURN_SOON){ buzz(90); ESP_LOGI(TAG,"turn in 200m"); }
  if(ev&GW_EV_TURN_NOW){ buzz2(); ESP_LOGI(TAG,"turn now"); }
  if(ev&GW_EV_SUMMARY){ gw_faces_sheet("骑行结束",gw_m.summary); buzz(60); }
  if(ev&GW_EV_NOTE){ buzz(40); }
  if(ev&GW_EV_TIME){ time_t t=time(nullptr); struct tm tm; localtime_r(&t,&tm);
    if(tm.tm_year<125){ struct timeval tv={(time_t)gw_m.time_unix,0}; settimeofday(&tv,nullptr); ESP_LOGI(TAG,"time set from phone"); } }
  if(ev) render();
}
static bool last_ble=false; static int nfr=0; static int64_t fr_t0=0;
static void frame_cb(lv_event_t*){ nfr++; }
static void fps_cb(lv_timer_t*){ if(nfr>3){ int64_t t=now_us(); ESP_LOGI(TAG,"fps=%d (%.0f ms window)",(int)(nfr*1e6/(t-fr_t0)),(t-fr_t0)/1e3);} nfr=0; fr_t0=now_us(); }
static void tick_cb(lv_timer_t*){ bool b=linked(); int ev=gw_model_tick(now_us()); if(ev&GW_EV_NAV_END) ESP_LOGW(TAG,"nav stale 20s -> idle");
  if(page<=2 || b!=last_ble || page==4 || ev) render(); last_ble=b; }
static void gesture_cb(lv_event_t*){
  lv_dir_t d=lv_indev_get_gesture_dir(lv_indev_active()); last_gesture=now_us();
  if(gw_cc_is_open()){ if(d==LV_DIR_BOTTOM||d==LV_DIR_LEFT||d==LV_DIR_RIGHT) gw_cc_open(nullptr,nullptr); return; }
  if(gw_faces_sheet_open()){ if(d==LV_DIR_LEFT||d==LV_DIR_RIGHT) gw_faces_sheet(nullptr,nullptr); return; }
  // horizontal cycle = core pages only (Grok, face, ride, nav); notifications are an optional pull-down from the face
  if(d==LV_DIR_LEFT||d==LV_DIR_RIGHT){ int s=d==LV_DIR_LEFT?1:-1; int n=((page==3?1:page)+s+NPAGES)%NPAGES; if(n==3) n=(n+s+NPAGES)%NPAGES; go(n,s); }
  else if(d==LV_DIR_TOP && page==3) go(1,-1);
  else if(d==LV_DIR_TOP && page==1){ cc_fill(); gw_cc_open(&cc,cc_tap); if(haptic) haptic(18); }       // Control Center
  else if(d==LV_DIR_BOTTOM && page==1) go(3,1);                                                          // notifications
}
static void click_cb(lv_event_t* e){
  if(now_us()-last_gesture<400000) return;
  if(lv_event_get_target(e)!=lv_screen_active()) return;
  if(page==0 && chat_tap) chat_tap();
}
static void build_ui(){
  lv_obj_t* scr=lv_screen_active();
  { Settings s("gw",false); night=s.GetInt("night",0); dnd=s.GetInt("dnd",0); }
  gw_faces_set_fallback(lv_obj_get_style_text_font(scr,LV_PART_MAIN)); gw_faces_set_night(night); gw_faces_set_fade(false); gw_faces_create(scr);
  lv_obj_add_flag(scr,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(scr,gesture_cb,LV_EVENT_GESTURE,nullptr); lv_obj_add_event_cb(scr,click_cb,LV_EVENT_CLICKED,nullptr);
  lv_timer_create(tick_cb,1000,nullptr); built=true;
  lv_display_add_event_cb(lv_display_get_default(),frame_cb,LV_EVENT_REFR_READY,nullptr); lv_timer_create(fps_cb,500,nullptr); fr_t0=now_us();
}
static void render(){
  if(!built) build_ui();
  time_t now=time(nullptr); struct tm t; localtime_r(&now,&t);
  int lv=0; bool ch=false,dis=false; Board::GetInstance().GetBatteryLevel(lv,ch,dis);
  gw_state_t s={}; s.hh=t.tm_hour; s.mm=t.tm_min; s.ss=t.tm_sec; s.mon=t.tm_mon+1; s.day=t.tm_mday; s.wday=t.tm_wday;
  s.battery=lv; s.charging=ch; s.wifi=WifiManager::GetInstance().IsConnected(); s.ble=linked();
  s.n_notes=gw_m.n_notes; s.notes=gw_m.notes; s.phone_batt=gw_m.phone_batt; s.ride_km=gw_m.ride_km; s.dnd=dnd;
  s.nav_active=gw_m.nav_active; s.nav_dist=gw_m.dist; s.nav_dir=gw_m.road; s.nav_dur=gw_m.dur; s.nav_eta=gw_m.eta;
  s.nav_icon=gw_m.has_icon?gw_m.icon:nullptr; s.speed=gw_m.speed; s.ride_min=gw_model_ride_min(now_us());
  gw_faces_show(page,&s);
}
static void refresh(){ DisplayLockGuard g(Board::GetInstance().GetDisplay()); render(); }
// BLE host task -> copy -> main task (under the display lock) -> model -> UI
extern "C" void gw_on_write(const uint8_t* d,uint16_t n){
  if(!n) return;
  if(GW_RX_LOG){ int i=0; while(i+3<=n){ int l=d[i+1]|d[i+2]<<8; if(d[i]!=0x20) ESP_LOGI(TAG,"rx type=%02x len=%d",d[i],l); i+=3+l; } }
  std::string buf((const char*)d,n);
  Application::GetInstance().Schedule([buf](){ DisplayLockGuard g(Board::GetInstance().GetDisplay());
    apply(gw_model_rx((const uint8_t*)buf.data(),(int)buf.size(),now_us())); });
}
static void sched(std::function<void()> f){ Application::GetInstance().Schedule([f](){ DisplayLockGuard g(Board::GetInstance().GetDisplay()); f(); }); }
void NavUiToggle(){ sched([](){ int n=((page==3?1:page)+1)%NPAGES; if(n==3) n=4; go(n,1); }); }
void NavUiNext(){ NavUiToggle(); }
void NavUiHome(){ sched([](){ gw_cc_open(nullptr,nullptr); gw_faces_sheet(nullptr,nullptr); if(page!=1) go(1,-1); }); }
void NavUiShowChat(){ if(page!=0) sched([](){ go(0,-1); }); }
void NavUiSetHaptic(void (*f)(int)){ haptic=f; }
void NavUiSetChatTap(void (*f)()){ chat_tap=f; }
bool NavUiPowerPending(){ return power_pending; }
void NavUiPowerMenu(bool show){ power_pending=show; sched([show](){ if(show){ gw_faces_sheet("关机？","按 蓝键 确认关机\n按 黄键 取消"); if(haptic) haptic(60);} else gw_faces_sheet(nullptr,nullptr); }); }
void NavUiInit(){ Application::GetInstance().Schedule(refresh); }
