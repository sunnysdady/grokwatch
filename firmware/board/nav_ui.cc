#include "gw_health.h"
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
#include "display/lvgl_display/lvgl_display.h"
#include "audio/audio_codec.h"
#include "backlight.h"
#include "settings.h"
#include "gw_faces.h"
#include "gw_todo_net.h"
#include "gw_input.h"
#define TAG "NavUI"
#define GW_RX_LOG 1  // log type/len of each incoming frame (no content)
extern "C" bool gw_ble_link(void);
static int page=1, back_page=1; static const int NPAGES=GW_NCORE; // carousel: 0 chat 1 face 2 ride 3 todo 4 nav
static bool built=false, power_pending=false, dnd=false, night=false; static int64_t last_gesture=0;
static void (*haptic)(int ms)=nullptr; static void (*chat_tap)()=nullptr;
static void render();
static int64_t now_us(){ return esp_timer_get_time(); }
static bool linked(){ return gw_model_linked(gw_ble_link(), now_us()); }
static void buzz(int ms){ if(haptic && !dnd) haptic(ms); }
static void buzz2(){ if(!haptic || dnd) return; haptic(70); lv_timer_create([](lv_timer_t* t){ if(haptic) haptic(70); lv_timer_delete(t); },180,nullptr); }
static int64_t page_us=0, wake_us=0; static gw_anim_gov_t gov; static bool gov_init=false; static uint32_t fr_total=0;
static bool touch_log=false; static lv_indev_t* touch_indev=nullptr; static int64_t down_us=0; static lv_point_t down_pt={0,0};
static void slide_measure(){ if(!gov_init){ gw_anim_gov_init(&gov); gov_init=true; } if(!gov.slide_on) return;
  struct M{ uint32_t f0; int64_t t0; }; M* m=new M{fr_total,now_us()};
  lv_timer_t* t=lv_timer_create([](lv_timer_t* t){ M* m=(M*)lv_timer_get_user_data(t); gw_anim_gov_slide_done(&gov,(int)(fr_total-m->f0),now_us()-m->t0);
    if(!gov.slide_on){ gw_faces_set_slide(false); ESP_LOGW(TAG,"slides %d fps < 30 twice: switching pages directly",gov.last_fps);} delete m; lv_timer_delete(t); },GW_SLIDE_MS,m); (void)t; }
static void go(int p,int d){ page=(p%NPAGES+NPAGES)%NPAGES; page_us=now_us(); slide_measure(); gw_faces_dir(d); if(haptic) haptic(18); if(page==GW_PAGE_TODO) GwTodoFetch(); render(); }

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
  if(ev&GW_EV_NAV_START){ page=GW_PAGE_NAV; buzz(30); }
  if(ev&GW_EV_TURN_SOON){ buzz(90); ESP_LOGI(TAG,"turn in 200m"); }
  if(ev&GW_EV_TURN_NOW){ buzz2(); ESP_LOGI(TAG,"turn now"); }
  if(ev&GW_EV_SUMMARY){ gw_faces_sheet("骑行结束",gw_m.summary); buzz(60); }
  if(ev&GW_EV_TIME){ time_t t=time(nullptr); struct tm tm; localtime_r(&t,&tm);
    if(tm.tm_year<125){ struct timeval tv={(time_t)gw_m.time_unix,0}; settimeofday(&tv,nullptr); ESP_LOGI(TAG,"time set from phone"); } }
  if(ev) render();
}
static bool last_ble=false; static int nfr=0; static int64_t fr_t0=0, fr_start=0, fr_sum=0, fr_max=0;
// Frame log (default OFF): NVS gw/fpslog=1 enables a WARN line every 2 s with fps and per-frame render+flush time.
static bool fps_log=false;
static void frame_start_cb(lv_event_t*){ fr_start=now_us(); }
static uint16_t fr_ring[128]; // last 128 frame render+flush times, 0.1 ms units (console "stat")
static void frame_cb(lv_event_t*){ nfr++; fr_total++; if(fr_start){ int64_t d=now_us()-fr_start; fr_sum+=d; if(d>fr_max) fr_max=d; fr_ring[fr_total&127]=(uint16_t)(d>6553500?65535:d/100); } }
static void fps_cb(lv_timer_t*){ int64_t t=now_us();
  if(fps_log && nfr>3) ESP_LOGW(TAG,"fps=%d frame avg %.1f ms max %.1f ms (%d frames / %.0f ms)",(int)(nfr*1e6/(t-fr_t0)),fr_sum/1e3/nfr,fr_max/1e3,nfr,(t-fr_t0)/1e3);
  nfr=0; fr_sum=fr_max=0; fr_t0=t; }
static bool standby=false; static uint32_t sb_min=0; static int sb_last=-1;
static void standby_tick(){ if(!standby) return; time_t now=time(nullptr); struct tm t; localtime_r(&now,&t);
  if(t.tm_min!=sb_last){ sb_last=t.tm_min; gw_standby_set(t.tm_hour,t.tm_min,sb_min++); } }
static void tick_cb(lv_timer_t*){ gw_health_ui_beat(); standby_tick(); bool b=linked(); int ev=gw_model_tick(now_us()); if(ev&GW_EV_NAV_END) ESP_LOGW(TAG,"nav stale 20s -> idle");
  if(page<=2 || b!=last_ble || page==GW_PAGE_NAV || ev) render(); last_ble=b; }
static void gesture_cb(lv_event_t*){
  lv_dir_t d=lv_indev_get_gesture_dir(lv_indev_active()); last_gesture=now_us();
  if(touch_log) ESP_LOGW(TAG,"touch gesture dir=%s page=%d -> %s",d==LV_DIR_LEFT?"L":d==LV_DIR_RIGHT?"R":d==LV_DIR_TOP?"U":d==LV_DIR_BOTTOM?"D":"?",page,
    gw_cc_is_open()?"close_cc":gw_faces_sheet_open()?"close_sheet":(d==LV_DIR_LEFT||d==LV_DIR_RIGHT)?"page":d==LV_DIR_TOP?"open_cc":"none");
  if(gw_cc_is_open()){ if(d==LV_DIR_BOTTOM||d==LV_DIR_LEFT||d==LV_DIR_RIGHT) gw_cc_open(nullptr,nullptr); return; }
  if(gw_faces_sheet_open()){ if(d==LV_DIR_LEFT||d==LV_DIR_RIGHT) gw_faces_sheet(nullptr,nullptr); return; }
  // horizontal cycle = core pages only (Grok, face, ride, nav); notifications are an optional pull-down from the face
  if(d==LV_DIR_LEFT||d==LV_DIR_RIGHT){ int s=d==LV_DIR_LEFT?1:-1; go(page+s,s); }
  else if(d==LV_DIR_TOP){ cc_fill(); gw_cc_open(&cc,cc_tap); if(haptic) haptic(18); }                     // swipe up = Control Center
}
// Grok entry: a plain tap used to toggle listening (also the wake tap, taps right after a swipe, edge grazes) ->
// frequent false triggers. Now only a deliberate centre long-press (gw_chat_tap_decide) or the blue button.
static void chat_try(bool long_press){
  int64_t n=now_us(); lv_point_t p=down_pt; if(lv_indev_active()) lv_indev_get_point(lv_indev_active(),&p);
  gw_tap_t t={n, down_us?n-down_us:0, p.x, p.y, n-last_gesture, n-wake_us, n-page_us, long_press};
  gw_tap_verdict_t v=gw_chat_tap_decide(&t);
  ESP_LOGW(TAG,"grok touch %s at %d,%d held %lld ms -> %s",long_press?"long":"tap",(int)p.x,(int)p.y,(long long)(t.press_us/1000),v==GW_TAP_OK?"START GROK":gw_tap_verdict_str(v));
  if(v==GW_TAP_OK && chat_tap) chat_tap(); }
static void click_cb(lv_event_t* e){
  if(lv_event_get_target(e)!=lv_screen_active()) return;
  if(page==0) chat_try(false);  // logged + rejected as "short"
}
static void longpress_cb(lv_event_t* e){ if(lv_event_get_target(e)!=lv_screen_active()) return; if(page==0) chat_try(true); }
static void indev_cb(lv_event_t* e){ lv_event_code_t c=lv_event_get_code(e); lv_indev_t* id=(lv_indev_t*)lv_event_get_target(e); lv_point_t p; lv_indev_get_point(id,&p);
  if(c==LV_EVENT_PRESSED){ down_us=now_us(); down_pt=p; if(touch_log) ESP_LOGW(TAG,"touch down %d,%d page=%d",(int)p.x,(int)p.y,page); }
  else if(c==LV_EVENT_RELEASED){ if(touch_log) ESP_LOGW(TAG,"touch up %d,%d after %lld ms",(int)p.x,(int)p.y,(long long)((now_us()-down_us)/1000)); } }
// P0 (2.6.4 crash): F20/F21 keep a raw .fallback to the theme text font. LvglBuiltInFont owns a heap COPY of the
// builtin font and SetTextFont() frees it after the assets font loads -> dangling fallback -> InstrFetchProhibited in
// lv_font_get_glyph_width. Re-point the fallback whenever the theme font changes (runs under the display lock,
// before the old font is released).
extern "C" void gw_grok_font_changed(const lv_font_t* f);
static void on_text_font_changed(const lv_font_t* f){ gw_faces_set_fallback(f); gw_grok_font_changed(f); if(built) lv_obj_report_style_change(nullptr); }
static void build_ui(){
  lv_obj_t* scr=lv_screen_active();
  { Settings s("gw",false); night=s.GetInt("night",0); dnd=s.GetInt("dnd",0); }
  LvglDisplay::SetTextFontChangedCallback(on_text_font_changed);
  gw_faces_set_fallback(lv_obj_get_style_text_font(scr,LV_PART_MAIN)); gw_faces_set_night(night); gw_faces_set_fade(false); gw_faces_create(scr);
  lv_obj_add_flag(scr,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(scr,gesture_cb,LV_EVENT_GESTURE,nullptr); lv_obj_add_event_cb(scr,click_cb,LV_EVENT_CLICKED,nullptr); lv_obj_add_event_cb(scr,longpress_cb,LV_EVENT_LONG_PRESSED,nullptr);
  if(touch_indev){ lv_indev_set_long_press_time(touch_indev,GW_CHAT_MIN_PRESS_US/1000); lv_indev_add_event_cb(touch_indev,indev_cb,LV_EVENT_ALL,nullptr); }
  gw_standby_create(lv_layer_top());
  lv_timer_create(tick_cb,1000,nullptr); built=true;
  gw_faces_set_todo_cb([](int row){ GwTodoCheck(row); if(haptic) haptic(15); render(); });
  GwTodoSetDoneCb([](){ Application::GetInstance().Schedule([](){ DisplayLockGuard g(Board::GetInstance().GetDisplay()); if(!g) return; if(page==GW_PAGE_TODO) render(); }); });
  { Settings s("gw",false); fps_log=s.GetInt("fpslog",0)!=0; }
  lv_display_add_event_cb(lv_display_get_default(),frame_start_cb,LV_EVENT_REFR_START,nullptr);
  lv_display_add_event_cb(lv_display_get_default(),frame_cb,LV_EVENT_REFR_READY,nullptr); lv_timer_create(fps_cb,2000,nullptr); fr_t0=now_us();
}
static void render(){
  if(!built) build_ui();
  time_t now=time(nullptr); struct tm t; localtime_r(&now,&t);
  int lv=0; bool ch=false,dis=false; Board::GetInstance().GetBatteryLevel(lv,ch,dis);
  gw_state_t s={}; s.hh=t.tm_hour; s.mm=t.tm_min; s.ss=t.tm_sec; s.mon=t.tm_mon+1; s.day=t.tm_mday; s.wday=t.tm_wday;
  s.battery=lv; s.charging=ch; s.wifi=WifiManager::GetInstance().IsConnected(); s.ble=linked();
  s.phone_batt=gw_m.phone_batt; s.ride_km=gw_m.ride_km; s.dnd=dnd;
  s.nav_active=gw_m.nav_active; s.nav_dist=gw_m.dist; s.nav_dir=gw_m.road; s.nav_dur=gw_m.dur; s.nav_eta=gw_m.eta;
  s.n_todo=gw_todo.n; s.todo_open=gw_todo.open; s.todo_state=gw_todo.state;
  for(int i=0;i<gw_todo.n;i++){ s.todo_text[i]=gw_todo.text[i]; s.todo_done[i]=gw_todo.done[i]; }
  s.nav_icon=gw_m.has_icon?gw_m.icon:nullptr; s.light_s=gw_model_light_left(now_us()); s.light_c=gw_m.light_c; s.nav_turn=gw_m.turn; s.usage_pct=gw_m.usage_pct; s.usage_reset=gw_m.usage_reset; s.speed=gw_m.speed; s.ride_min=gw_model_ride_min(now_us());
  gw_faces_show(page,&s);
}
static void refresh(){ DisplayLockGuard g(Board::GetInstance().GetDisplay()); if(!g) return; render(); }
// BLE host task -> copy -> main task (under the display lock) -> model -> UI
extern "C" void gw_on_write(const uint8_t* d,uint16_t n){
  if(!n) return;
  if(GW_RX_LOG){ int i=0; while(i+3<=n){ int l=d[i+1]|d[i+2]<<8; if(d[i]!=0x20) ESP_LOGI(TAG,"rx type=%02x len=%d",d[i],l); i+=3+l; } }
  std::string buf((const char*)d,n);
  Application::GetInstance().Schedule([buf](){ DisplayLockGuard g(Board::GetInstance().GetDisplay()); if(!g) return;
    apply(gw_model_rx((const uint8_t*)buf.data(),(int)buf.size(),now_us())); });
}
static void sched(std::function<void()> f){ Application::GetInstance().Schedule([f](){ DisplayLockGuard g(Board::GetInstance().GetDisplay()); if(!g) return; f(); }); }
void NavUiToggle(){ sched([](){ go(page+1,1); }); }
void NavUiNext(){ NavUiToggle(); }
void NavUiHome(){ sched([](){ gw_cc_open(nullptr,nullptr); gw_faces_sheet(nullptr,nullptr); if(page!=1) go(1,-1); }); }
void NavUiShowChat(){ if(page!=0) sched([](){ go(0,-1); }); }
void NavUiSetHaptic(void (*f)(int)){ haptic=f; }
void NavUiSetChatTap(void (*f)()){ chat_tap=f; }
bool NavUiPowerPending(){ return power_pending; }
void NavUiPowerMenu(bool show){ power_pending=show; sched([show](){ if(show){ gw_faces_sheet("关机？","按 蓝键 确认关机\n按 黄键 取消"); if(haptic) haptic(60);} else gw_faces_sheet(nullptr,nullptr); }); }
void NavUiInit(){ Application::GetInstance().Schedule(refresh); }
// Screen idle -> minimal standby clock; any input (handled by the power-save timer) -> back to the current page.
void NavUiStandby(bool on){ sched([on](){ if(!built) return; standby=on; sb_last=-1;
  if(on){ if(gw_cc_is_open()) gw_cc_open(nullptr,nullptr); standby_tick(); gw_standby_show(true); }
  else { wake_us=now_us(); gw_standby_show(false); render(); } }); }

// ---- hooks for the board + the USB test console (gw_console.cc) ----
void NavUiSetTouchIndev(lv_indev_t* i){ touch_indev=i; }
void NavUiGoPage(int p){ sched([p](){ go(p, p>=page?1:-1); }); }
int  NavUiPage(){ return page; }
void NavUiTouchLog(bool on){ touch_log=on; }
void NavUiFpsLog(bool on){ fps_log=on; }
bool NavUiSlideOn(){ return !gov_init || gov.slide_on; }
// last n frames (n<=128): min/avg/max frame time in 0.1 ms; returns frames counted so far
uint32_t NavUiFrameStats(int n,int* mn,int* av,int* mx){ if(n>128) n=128; if((uint32_t)n>fr_total) n=(int)fr_total; int a=0,lo=1<<30,hi=0;
  for(int i=0;i<n;i++){ int v=fr_ring[(fr_total-i)&127]; a+=v; if(v<lo) lo=v; if(v>hi) hi=v; } *mn=n?lo:0; *mx=hi; *av=n?a/n:0; return fr_total; }
