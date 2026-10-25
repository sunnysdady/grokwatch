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
#include "gw_sbsm.h"
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

static int sb_ev(sb_event_t ev,const char* why); void NavUiPowerMenu(bool show);
static void apply(int ev){
  if(ev&GW_EV_NAV_START) sb_ev(SB_EV_NAV_START,"nav start"); else if(ev&(GW_EV_TURN_SOON|GW_EV_TURN_NOW)) sb_ev(SB_EV_NAV_TURN,"nav turn");
  if(ev&GW_EV_SUMMARY) sb_ev(SB_EV_SUMMARY,"summary");
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
// ---- 2.6.19 standby: ONE state machine (gw_sbsm.c, spec docs/relay/STANDBY-SPEC.md) owns enter/leave. Everything that
// used to wake/dim on its own (PowerSaveTimer callbacks, WakeScreen, screen_off_, standby tap hook) now sends an event.
static void (*backlight_fn)(int pct)=nullptr;   // board: pct 0-100, -1 = restore the user brightness (smooth ramp)
static gw_sbsm_t sbm; static bool sbm_init=false; static lv_timer_t* sb_anim_t=nullptr;
static bool standby=false; static uint32_t sb_min=0; static int sb_last=-1;
static void standby_tick(){ if(!standby) return; time_t now=time(nullptr); struct tm t; localtime_r(&now,&t);
  if(t.tm_min!=sb_last){ sb_last=t.tm_min; gw_standby_set(t.tm_hour,t.tm_min,sb_min++); } }
static int64_t ms_now(){ return now_us()/1000; }
static sb_ctx_t sb_ctx(){ sb_ctx_t c; c.page=page; c.nav_active=gw_m.nav_active; c.ride_active=gw_m.last_spd!=0;
  int ds=(int)Application::GetInstance().GetDeviceState(); c.voice_busy=ds==kDeviceStateListening||ds==kDeviceStateSpeaking||ds==kDeviceStateConnecting;
  c.overlay_open=gw_cc_is_open()||gw_faces_sheet_open()||power_pending; return c; }
// Fully in standby: hide the page objects under the clock so the 1 s second hand / page timers stop redrawing and
// flushing behind an opaque layer (power). Unhidden at the start of the wake fade so the page shows through it.
static lv_obj_t* sb_hid[16]; static int sb_nhid=0;
static void sb_pages_hidden(bool h){ if(h){ lv_obj_t* scr=lv_screen_active(); sb_nhid=0; for(uint32_t i=0;i<lv_obj_get_child_count(scr)&&sb_nhid<16;i++){ lv_obj_t* o=lv_obj_get_child(scr,i);
      if(!lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN)){ lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN); sb_hid[sb_nhid++]=o; } } }
  else { for(int i=0;i<sb_nhid;i++) lv_obj_remove_flag(sb_hid[i],LV_OBJ_FLAG_HIDDEN); sb_nhid=0; } }
static void sb_anim_done(lv_timer_t* t){ lv_timer_delete(t); sb_anim_t=nullptr; sb_ctx_t c=sb_ctx(); gw_sbsm_event(&sbm,SB_EV_ANIM_DONE,&c,ms_now());
  if(sbm.st==SB_STANDBY) sb_pages_hidden(true);
  ESP_LOGW("GW-SB","state=%s",gw_sbsm_state_str(sbm.st)); }
static void go(int p,int dd);
static void sb_exec(int a,const char* why){
  if(!a) return; ESP_LOGW("GW-SB","%s -> act=0x%x state=%s prev=%d",why,a,gw_sbsm_state_str(sbm.st),sbm.prev_page);
  if(a&SB_A_CLOSE_OVERLAY){ gw_cc_open(nullptr,nullptr); if(power_pending){ power_pending=false; } gw_faces_sheet(nullptr,nullptr); }
  if(a&SB_A_SHOW){ sb_pages_hidden(false); standby=true; sb_last=-1; standby_tick(); gw_standby_show(true); }
  if(a&SB_A_DIM && backlight_fn) backlight_fn(10);
  if(a&SB_A_HIDE){ standby=false; wake_us=now_us(); sb_pages_hidden(false); gw_standby_show(false);
    int to=(a&SB_A_GOTO_NAV)?GW_PAGE_NAV:(a&SB_A_GOTO_CHAT)?GW_PAGE_CHAT:(a&SB_A_GOTO_PREV)?sbm.prev_page:page;
    if(to!=page) go(to,to>page?1:-1); else render(); }
  if(a&SB_A_RESTORE && backlight_fn) backlight_fn(-1);
  if(a&SB_A_HAPTIC && haptic) haptic(15);
  if(a&(SB_A_SHOW|SB_A_HIDE)){ if(sb_anim_t) lv_timer_delete(sb_anim_t); sb_anim_t=lv_timer_create(sb_anim_done,(a&SB_A_SHOW)?SB_ENTER_MS:SB_EXIT_MS,nullptr); } }
// runs in the LVGL/UI context (under the display lock); returns the action mask
static int sb_ev(sb_event_t ev,const char* why){ if(!sbm_init){ gw_sbsm_init(&sbm,ms_now()); sbm_init=true; } sb_ctx_t c=sb_ctx();
  int a=gw_sbsm_event(&sbm,ev,&c,ms_now()); sb_exec(a&~SB_A_PASS,why); return a; }
static void tick_cb(lv_timer_t*){ gw_health_ui_beat(); standby_tick(); sb_ev(SB_EV_TICK,"idle"); bool b=linked(); int ev=gw_model_tick(now_us()); if(ev&GW_EV_NAV_END) ESP_LOGW(TAG,"nav stale 20s -> idle");
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
  if(c==LV_EVENT_PRESSED){ down_us=now_us(); down_pt=p; if(!standby) sb_ev(SB_EV_ACTIVITY,"touch"); if(touch_log) ESP_LOGW(TAG,"touch down %d,%d page=%d",(int)p.x,(int)p.y,page); }
  else if(c==LV_EVENT_RELEASED){ if(touch_log) ESP_LOGW(TAG,"touch up %d,%d after %lld ms",(int)p.x,(int)p.y,(long long)((now_us()-down_us)/1000)); } }
// P0 (2.6.4 crash): F20/F21 keep a raw .fallback to the theme text font. LvglBuiltInFont owns a heap COPY of the
// builtin font and SetTextFont() frees it after the assets font loads -> dangling fallback -> InstrFetchProhibited in
// lv_font_get_glyph_width. Re-point the fallback whenever the theme font changes (runs under the display lock,
// before the old font is released).
extern "C" void gw_grok_font_changed(const lv_font_t* f);
static void on_text_font_changed(const lv_font_t* f){ gw_faces_set_fallback(f); gw_grok_font_changed(f); if(built) lv_obj_report_style_change(nullptr); }

// ---- 2.6.12 passive GW-PERF line (serial log only, no console command): every 10 s, every 60 s after 3 min ----
#include <esp_heap_caps.h>
static uint32_t pf_frames; static int64_t pf_t0, pf_max_us, pf_last_print; static bool pf_hooked;
static void pf_cb(lv_event_t* e){ int64_t t=esp_timer_get_time(); auto c=lv_event_get_code(e);
  if(c==LV_EVENT_REFR_START) pf_t0=t; else if(c==LV_EVENT_REFR_READY && pf_t0){ int64_t d=t-pf_t0; if(d>pf_max_us) pf_max_us=d; pf_frames++; } }
static void pf_timer(lv_timer_t* tm){ int64_t now=esp_timer_get_time(); uint32_t up=(uint32_t)(now/1000000);
  if(!pf_hooked){ lv_display_add_event_cb(lv_display_get_default(),pf_cb,LV_EVENT_ALL,nullptr); pf_hooked=true; pf_last_print=now; pf_frames=0; pf_max_us=0; return; }
  float secs=(now-pf_last_print)/1e6f; lv_mem_monitor_t m; lv_mem_monitor(&m);
  ESP_LOGW("GW-PERF","up=%lus int=%u/min%u psram=%u/min%u lvgl=%u/%u free(frag%u%%) fps=%.1f slow=%.1fms",(unsigned long)up,
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),(unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),(unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM),
    (unsigned)m.free_size,(unsigned)m.total_size,(unsigned)m.frag_pct,secs>0?pf_frames/secs:0.f,pf_max_us/1000.f);
  pf_frames=0; pf_max_us=0; pf_last_print=now; if(up>=180) lv_timer_set_period(tm,60000); }
// 2.6.14: second hand poll (50 ms, wall clock) + passive GW-SEC interval stats (serial log only, every 60 moves)
// 2.6.15: passive stall diagnostics. On any move gap > 1.1 s print one GW-STALL line: page, longest LVGL loop gap
// (gap between runs of this 50 ms lv_timer), wall-clock step vs esp_timer, device state, last BLE rx, wifi, and the top
// CPU users during the gap (uxTaskGetSystemState delta vs the snapshot taken at the previous move). Snapshots: PSRAM.
#include <sys/time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "gw_mem.h"
static int64_t sc_last, sc_sum, sc_max, sc_min=INT64_MAX; static uint32_t sc_n, sc_late, sc_skip; static int sc_prev=-1;
static int64_t sc_cb_last, sc_cb_gap_max, sc_skew0; static uint32_t sc_stalls;
enum { TS_MAX=48 };
static TaskStatus_t *ts_prev, *ts_now; static UBaseType_t ts_np; static uint32_t ts_tot_prev;
static void ts_snap(bool report, int64_t gap_us){
  if(!ts_prev){ ts_prev=(TaskStatus_t*)gw_psram_calloc(sizeof(TaskStatus_t)*TS_MAX); ts_now=(TaskStatus_t*)gw_psram_calloc(sizeof(TaskStatus_t)*TS_MAX); if(!ts_prev||!ts_now) return; }
  uint32_t tot=0; UBaseType_t n=uxTaskGetSystemState(ts_now,TS_MAX,&tot); if(!n) return;
  if(report && ts_np && tot>ts_tot_prev){ uint32_t dt=tot-ts_tot_prev; char b[200]; int o=0;
    for(int k=0;k<5;k++){ int bi=-1; uint32_t bd=0;
      for(UBaseType_t i=0;i<n;i++){ uint32_t pv=0; bool f=false; for(UBaseType_t j=0;j<ts_np;j++) if(ts_prev[j].xHandle==ts_now[i].xHandle){ pv=ts_prev[j].ulRunTimeCounter; f=true; break; }
        uint32_t d=ts_now[i].ulRunTimeCounter-(f?pv:0); if(d>bd){ bd=d; bi=i; } }
      if(bi<0) break; o+=snprintf(b+o,sizeof b-o,"%s:%u%%/p%u ",ts_now[bi].pcTaskName,(unsigned)(bd*100ULL/dt/ (portNUM_PROCESSORS)),(unsigned)ts_now[bi].uxCurrentPriority);
      ts_now[bi].ulRunTimeCounter-= bd;  // exclude from next pick (restored by the copy below being taken before)
      if(o>=(int)sizeof b-24) break; }
    ESP_LOGW("GW-STALL","cpu over %lldms: %s",gap_us/1000,b); }
  // re-take so counters are exact for the next delta (the loop above modified ts_now)
  n=uxTaskGetSystemState(ts_prev,TS_MAX,&ts_tot_prev); ts_np=n; }
// 2.6.17: one move record for both paths (50 ms poll and the 1 s render()) -- 2.6.16 counted only the poll path, so a
// move made by render() showed up as a fake gap + skip.
static int64_t sc_skew_now;
static void on_sec_move(int ss){ if(page!=GW_PAGE_FACE) return; int64_t n=now_us();
  bool stall=false; int64_t d=0;
  if(sc_last){ d=n-sc_last; sc_sum+=d; sc_n++; if(d>sc_max) sc_max=d; if(d<sc_min) sc_min=d; if(d>1100000){ sc_late++; stall=true; }
    bool sk=sc_prev>=0 && ss!=(sc_prev+1)%60; if(sk) sc_skip++;
    if(stall||sk){ sc_stalls++;
      ESP_LOGW("GW-STALL","#%u gap=%lldms ss %d->%d page=%d lvloop_max=%lldms clockstep=%lldms state=%d ble_rx=%llds_ago wifi=%d int=%u",
        (unsigned)sc_stalls,d/1000,sc_prev,ss,page,sc_cb_gap_max/1000,sc_skew0?(sc_skew_now-sc_skew0)/1000:0,(int)Application::GetInstance().GetDeviceState(),
        gw_m.last_rx?(long long)((n-gw_m.last_rx)/1000000):-1LL,(int)WifiManager::GetInstance().IsConnected(),(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL)); }
    if(sc_n>=60){ ESP_LOGW("GW-SEC","n=%u avg=%lldms min=%lldms max=%lldms late>1.1s=%u skipped=%u",(unsigned)sc_n,sc_sum/sc_n/1000,sc_min/1000,sc_max/1000,(unsigned)sc_late,(unsigned)sc_skip);
      sc_n=0; sc_sum=0; sc_max=0; sc_min=INT64_MAX; sc_late=0; sc_skip=0; } }
  ts_snap(stall,d);
  sc_last=n; sc_prev=ss; sc_skew0=sc_skew_now; sc_cb_gap_max=0; }
static void sec_cb(lv_timer_t*){ int64_t n=now_us();
  if(sc_cb_last){ int64_t g=n-sc_cb_last; if(g>sc_cb_gap_max) sc_cb_gap_max=g; } sc_cb_last=n;
  if(page!=GW_PAGE_FACE){ sc_last=0; sc_prev=-1; return; }
  struct timeval tv; gettimeofday(&tv,nullptr); struct tm t; time_t s=tv.tv_sec; localtime_r(&s,&t);
  sc_skew_now=(int64_t)tv.tv_sec*1000000+tv.tv_usec-n;
  gw_faces_sec(t.tm_sec); }
static void build_ui(){
  lv_obj_t* scr=lv_screen_active();
  { Settings s("gw",false); night=s.GetInt("night",0); dnd=s.GetInt("dnd",0); }
  LvglDisplay::SetTextFontChangedCallback(on_text_font_changed);
  lv_timer_create(pf_timer,10000,nullptr);
  gw_faces_set_fallback(lv_obj_get_style_text_font(scr,LV_PART_MAIN)); gw_faces_set_night(night); gw_faces_set_fade(false); gw_faces_create(scr);
  lv_obj_add_flag(scr,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(scr,gesture_cb,LV_EVENT_GESTURE,nullptr); lv_obj_add_event_cb(scr,click_cb,LV_EVENT_CLICKED,nullptr); lv_obj_add_event_cb(scr,longpress_cb,LV_EVENT_LONG_PRESSED,nullptr);
  if(touch_indev){ lv_indev_set_long_press_time(touch_indev,GW_CHAT_MIN_PRESS_US/1000); lv_indev_add_event_cb(touch_indev,indev_cb,LV_EVENT_ALL,nullptr); }
  gw_standby_create(lv_layer_top()); gw_standby_set_tap([](){ sb_ev(SB_EV_TOUCH,"touch"); });   // standby layer eats the waking touch
  gw_faces_set_sec_hook(on_sec_move);
  lv_timer_create(tick_cb,1000,nullptr); lv_timer_create(sec_cb,50,nullptr); built=true;
  // 2.6.15: esp_lvgl_port runs taskLVGL at priority 1 (core 1) while audio_afe (wake word, prio 3, unpinned), opus (2),
  // activation (2) and wifi compete for the same core -> LVGL starves for seconds. Run it at 3: time-sliced with the AFE,
  // still below audio_output (4) and audio_input (8), so audio keeps priority.
  { TaskHandle_t h=xTaskGetHandle("taskLVGL"); if(h){ vTaskPrioritySet(h,3); ESP_LOGW(TAG,"taskLVGL priority 1 -> 3"); } else ESP_LOGW(TAG,"taskLVGL not found"); }
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
void NavUiSetBacklight(void (*f)(int)){ backlight_fn=f; }
bool NavUiIsStandby(){ return standby; }
// Keys: the board passes the key event and its normal action; the SM decides (in the UI context) whether the key only
// wakes the watch or also does its job. ev: SB_EV_RED_CLICK / KEY_YELLOW / KEY_BLUE / RED_DOUBLE / VOICE.
void NavUiKey(int ev,std::function<void()> normal){ sched([ev,normal](){ if(!built) return;
  if(ev==SB_EV_RED_CLICK && power_pending){ NavUiPowerMenu(false); return; }
  int a=sb_ev((sb_event_t)ev,ev==SB_EV_RED_CLICK?"red":ev==SB_EV_KEY_YELLOW?"yellow":ev==SB_EV_KEY_BLUE?"blue":ev==SB_EV_RED_DOUBLE?"red2":"voice");
  if((a&SB_A_PASS) && normal) { std::function<void()> f=normal; Application::GetInstance().Schedule(std::move(f)); } }); }   // normal job runs outside the display lock   // 2.6.18: red key toggles the standby clock   // 2.6.17: board wake (backlight + PowerSaveTimer::WakeUp)
void NavUiSetChatTap(void (*f)()){ chat_tap=f; }
bool NavUiPowerPending(){ return power_pending; }
void NavUiPowerMenu(bool show){ power_pending=show; sched([show](){ if(show){ gw_faces_sheet("关机？","按 蓝键 确认关机\n按 黄键 取消"); if(haptic) haptic(60);} else gw_faces_sheet(nullptr,nullptr); }); }
void NavUiInit(){ Application::GetInstance().Schedule(refresh); }
// Test-console / legacy entry: same path as the red key.
void NavUiStandby(bool on){ sched([on](){ if(!built) return; if(on!=standby) sb_ev(SB_EV_RED_CLICK,on?"console on":"console off"); }); }

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
