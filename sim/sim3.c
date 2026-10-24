// GrokWatch scripted host simulator: real LVGL 9.5 + real UI sources (gw_ultra.c, gw_grok.c, gw_model.c, fonts),
// 466x466 round mask, scripted touch gestures through a real LVGL pointer indev, and real TLV frames into gw_model.
#include "lvgl.h"
#include "gw_faces.h"
#include "cbin_font.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define W 466
static uint16_t fb[W*W]; static const char* OUT; static int shot_n=0, missing_total=0;
static long flushed=0; static void flush(lv_display_t* d,const lv_area_t* a,uint8_t* px){ int w=a->x2-a->x1+1; flushed+=(long)w*(a->y2-a->y1+1); for(int y=a->y1;y<=a->y2;y++) memcpy(&fb[y*W+a->x1],px+(y-a->y1)*w*2,w*2); lv_display_flush_ready(d);}
static void save_ppm(const char* path){ FILE* f=fopen(path,"wb"); fprintf(f,"P6\n%d %d\n255\n",W,W);
  for(int i=0;i<W*W;i++){ uint16_t c=fb[i]; int x=i%W,y=i/W; int dx=x-233,dy=y-233; unsigned char r=((c>>11)&31)*255/31,g=((c>>5)&63)*255/63,b=(c&31)*255/31;
    if(dx*dx+dy*dy>233*233){r=g=b=24;} fputc(r,f);fputc(g,f);fputc(b,f);} fclose(f);}
static uint32_t tick=0; static uint32_t tk(void){return tick;}
static lv_display_t* d; static int64_t now_us(void){ return (int64_t)tick*1000; }
// ---- pointer indev driven by the script
static int px_=233,py_=233; static bool pressed=false;
static void indev_read(lv_indev_t* i,lv_indev_data_t* data){ (void)i; data->point.x=px_; data->point.y=py_; data->state=pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED; }
static void step(int ms){ for(int t=0;t<ms;t+=10){ tick+=10; lv_timer_handler(); } }
static void frame(void){ lv_refr_now(d); }
// ---- glyph audit: every visible label, every code point, must exist in its font (or fallback)
static uint32_t u8next(const char* t,uint32_t* i){ const unsigned char* s=(const unsigned char*)t+*i; uint32_t c=s[0]; int n=1;
  if(c>=0xF0){ c=((c&7)<<18)|((s[1]&63)<<12)|((s[2]&63)<<6)|(s[3]&63); n=4; } else if(c>=0xE0){ c=((c&15)<<12)|((s[1]&63)<<6)|(s[2]&63); n=3; } else if(c>=0xC0){ c=((c&31)<<6)|(s[1]&63); n=2; } *i+=n; return c; }
static void audit(lv_obj_t* o,const char* shot){
  if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN)) return;
  if(lv_obj_check_type(o,&lv_label_class)){ const char* t=lv_label_get_text(o); const lv_font_t* f=lv_obj_get_style_text_font(o,0); uint32_t i=0;
    while(t&&t[i]){ uint32_t cp=u8next(t,&i); if(cp<0x20||cp==0x2026) continue; lv_font_glyph_dsc_t g;
      if(!lv_font_get_glyph_dsc(f,&g,cp,0)){ missing_total++; printf("  MISSING U+%04X in '%.40s' (%s)\n",cp,t,shot); } } }
  for(uint32_t k=0;k<lv_obj_get_child_count(o);k++) audit(lv_obj_get_child(o,k),shot); }
static void shot(const char* name){ step(300); frame(); char p[256]; snprintf(p,sizeof p,"%s/%02d_%s.ppm",OUT,++shot_n,name); save_ppm(p);
  audit(lv_screen_active(),name); audit(lv_layer_top(),name); printf("shot %s\n",p); }
static int gif_n=0; static long gf_px=0; static int gf_cnt=0; static void gif_frame(const char* tag){ frame(); gf_px+=flushed; flushed=0; gf_cnt++; char p[256]; snprintf(p,sizeof p,"%s/gif_%s_%03d.ppm",OUT,tag,gif_n++); save_ppm(p); }
static void drag(int x0,int y0,int x1,int y1,int ms){ px_=x0;py_=y0;pressed=true; step(30); int n=ms/10; for(int i=1;i<=n;i++){ px_=x0+(x1-x0)*i/n; py_=y0+(y1-y0)*i/n; step(10);} pressed=false; step(40); }
static void tap(int x,int y){ px_=x;py_=y;pressed=true; step(60); pressed=false; step(400); }
#define SWIPE_LEFT()  drag(380,233,90,233,120)
#define SWIPE_RIGHT() drag(90,233,380,233,120)
#define SWIPE_UP()    drag(233,400,233,90,120)
#define SWIPE_DOWN()  drag(233,90,233,400,120)
// ---- page controller (same routing as nav_ui.cc)
static int page=1; static bool night=false, dnd=false; static gw_cc_t cc;
static void render(void){ time_t t0=time(NULL); struct tm t; localtime_r(&t0,&t); (void)t;
  gw_state_t s={.hh=21,.mm=18,.ss=40,.mon=10,.day=24,.wday=6,.battery=76,.charging=false,.wifi=true};
  s.ble=gw_model_linked(true,now_us()); s.n_notes=gw_m.n_notes; s.notes=gw_m.notes; s.phone_batt=gw_m.phone_batt; s.ride_km=gw_m.ride_km; s.dnd=dnd;
  s.nav_active=gw_m.nav_active; s.nav_dist=gw_m.dist; s.nav_dir=gw_m.road; s.nav_dur=gw_m.dur; s.nav_eta=gw_m.eta; s.nav_icon=gw_m.has_icon?gw_m.icon:NULL;
  s.speed=gw_m.speed; s.ride_min=gw_model_ride_min(now_us()); gw_faces_show(page,&s); }
static void go(int p,int dd){ page=(p+5)%5; gw_faces_dir(dd); render(); }
static void cc_fill(void){ cc.battery=76; cc.wifi=true; cc.ble=gw_model_linked(true,now_us()); cc.phone_batt=gw_m.phone_batt; cc.night=night; cc.dnd=dnd; }
static void cc_tap(int id){ if(id==GW_CC_BRIGHT) cc.bright=cc.bright>=100?30:(cc.bright>=60?100:60); if(id==GW_CC_VOLUME) cc.volume=cc.volume>=90?0:(cc.volume>=60?90:(cc.volume>=30?60:30));
  if(id==GW_CC_NIGHT){ night=!night; gw_faces_set_night(night); gw_faces_create(lv_screen_active()); render(); } if(id==GW_CC_DND) dnd=!dnd; cc_fill(); gw_cc_open(&cc,cc_tap); }
static void gesture_cb(lv_event_t* e){ (void)e; lv_dir_t dd=lv_indev_get_gesture_dir(lv_indev_active());
  if(gw_cc_is_open()){ if(dd==LV_DIR_BOTTOM||dd==LV_DIR_LEFT||dd==LV_DIR_RIGHT) gw_cc_open(NULL,NULL); return; }
  if(gw_faces_sheet_open()){ if(dd==LV_DIR_LEFT||dd==LV_DIR_RIGHT) gw_faces_sheet(NULL,NULL); return; }
  if(dd==LV_DIR_LEFT) go(page+1,1); else if(dd==LV_DIR_RIGHT) go(page-1,-1);
  else if(dd==LV_DIR_TOP&&page==1){ cc_fill(); gw_cc_open(&cc,cc_tap); } else if(dd==LV_DIR_BOTTOM&&page==1) go(3,1); }
// ---- TLV injection (same bytes the Android app sends)
static uint8_t fr[1024];
static void rx(uint8_t t,const void* p,int l){ int n=gw_frame(fr,t,p,l); int ev=gw_model_rx(fr,n,now_us());
  if(ev&GW_EV_NAV_START) page=4; if(ev&GW_EV_SUMMARY) gw_faces_sheet("骑行结束",gw_m.summary);
  if(ev&(GW_EV_TURN_SOON|GW_EV_TURN_NOW)) printf("  haptic: %s\n",ev&GW_EV_TURN_NOW?"double (turn now)":"long (200 m)"); render(); }
static void rx_strs(uint8_t t,const char* a,const char* b,const char* c,const char* d4){ uint8_t p[900]; int n=0; const char* v[4]={a,b,c,d4};
  for(int i=0;i<4;i++){ if(!v[i]) break; int l=strlen(v[i]); memcpy(p+n,v[i],l); n+=l; p[n++]=0; } rx(t,p,n); }
static void hello(void){ uint8_t p[2]={82,0}; rx(0x01,p,2); }
static void speed(float k){ int v=(int)(k*10); uint8_t p[2]={v&255,v>>8}; rx(0x20,p,2); }
static void nav_on(int on){ uint8_t p[1]={on}; rx(0x10,p,1); }
static void icon_left(void){ uint8_t ic[288]={0}; for(int y=0;y<48;y++)for(int x=0;x<48;x++){ int on=0; if(x>=28&&x<36&&y>=18&&y<44) on=1; if(y>=14&&y<22&&x>=14&&x<36) on=1; if(x>=4&&x<18&&abs(y-18)<=(x-4)) on=1;
    if(on) ic[(y*48+x)/8]|=0x80>>((y*48+x)%8);} for(int part=0;part<3;part++){ uint8_t p[97]; p[0]=part; memcpy(p+1,ic+96*part,96); rx(0x12,p,97);} }
int main(int argc,char**argv){
  OUT=argv[1]; lv_init(); lv_tick_set_cb(tk);
  d=lv_display_create(W,W); static uint8_t buf[W*W*2]; lv_display_set_buffers(d,buf,NULL,sizeof buf,LV_DISPLAY_RENDER_MODE_DIRECT); lv_display_set_flush_cb(d,flush);
  lv_indev_t* in=lv_indev_create(); lv_indev_set_type(in,LV_INDEV_TYPE_POINTER); lv_indev_set_read_cb(in,indev_read);
  FILE* ff=fopen("$XIAOZHI/managed_components/78__xiaozhi-fonts/cbin/font_noto_sans_common_30_4.bin","rb"); fseek(ff,0,SEEK_END); long n=ftell(ff); rewind(ff); uint8_t* fbn=malloc(n); fread(fbn,1,n,ff); fclose(ff);
  lv_font_t* noto=cbin_font_create(fbn); lv_obj_t* scr=lv_screen_active(); lv_obj_set_style_bg_color(scr,lv_color_black(),0); lv_obj_set_style_text_font(scr,noto,0); lv_obj_set_style_text_color(scr,lv_color_white(),0);
  lv_obj_t* cont=lv_obj_create(scr); lv_obj_remove_style_all(cont); lv_obj_set_size(cont,W,W); lv_obj_set_style_bg_opa(cont,255,0); lv_obj_set_style_bg_color(cont,lv_color_black(),0);
  gw_faces_set_fallback(noto); gw_faces_set_fade(false); gw_faces_create(scr); lv_obj_add_flag(scr,LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(scr,gesture_cb,LV_EVENT_GESTURE,NULL);
  cc.bright=60; cc.volume=60; tick=1000000;
  // 1) cold: phone app not connected
  go(2,1); shot("ride_phone_not_connected");
  go(4,1); shot("nav_phone_not_connected");
  // 2) app connects (heartbeat), no GPS yet
  hello(); go(2,-1); shot("ride_waiting_gps");
  // 3) riding: speed every second for 3 minutes
  for(int i=0;i<180;i++){ speed(18+(i%40)/3.0f); step(1000); if(i%5==0) hello(); }
  shot("ride_moving");
  // 4) notifications: 6 incl. long Chinese WeChat, group chat, SMS, mail, English
  rx_strs(0x30,"微信","张三","今晚七点老地方吃饭？记得带上头盔，我们吃完顺便骑一圈深圳湾，大概二十公里，十点前回来。",NULL);
  rx_strs(0x30,"短信","95338","【顺丰速运】您的快递已放入科技园南区驿站，取件码 3-2-1088，请于今日 21:00 前领取。",NULL);
  rx_strs(0x30,"微信","骑行俱乐部（32）","李四：周六早上六点半在人才公园集合，路线：深圳湾—大沙河—塘朗山，全程约 65 公里，爬升 600 米。请大家检查刹车和胎压，带够补给。没有尾灯的同学这次一定要装上，夜骑回来天会黑。",NULL);
  rx_strs(0x30,"Gmail","GitHub","[xiaozhi-esp32] New release v2.6.0 is available with improved OTA and display fixes.",NULL);
  rx_strs(0x30,"日历","骑行俱乐部例会","19:30 · 南山区科技园 3 号会议室",NULL);
  rx_strs(0x30,"微信","张三","[2条] 张三：到了吗？",NULL);           // same chat: replaces older entry
  for(int k=0;k<4;k++){ speed(20); hello(); step(1000); }
  page=1; render(); SWIPE_DOWN(); shot("notes_list_top");
  drag(233,380,233,140,300); step(800); shot("notes_list_scrolled");
  step(200); tap(233,300); shot("note_expanded_long");
  drag(233,380,233,120,300); step(800); shot("note_expanded_scrolled");
  SWIPE_LEFT(); step(200);
  // 5) navigation from AMap: 350 米 -> 180 米 (long buzz) -> 30 米 (double buzz)
  hello(); nav_on(1); icon_left(); rx_strs(0x11,"350 米","科技园南路","12分钟","21:31"); shot("nav_350m");
  rx_strs(0x11,"180 米","科技园南路","11分钟","21:31"); rx_strs(0x11,"30 米","科技园南路","11分钟","21:31"); shot("nav_30m");
  go(2,-1); shot("ride_navigating_eta");
  // 6) arrival: nav ends -> ride summary sheet
  nav_on(0); shot("ride_summary");
  gw_faces_sheet(NULL,NULL);
  // 7) face + Control Center
  page=1; render(); shot("face");
  SWIPE_UP(); shot("control_center");
  tap(233-60,233-70); tap(233+60,233+62); shot("control_center_bright_dnd");
  tap(233-60,233+62); shot("control_center_night");
  SWIPE_DOWN(); go(1,1); shot("face_night");
  SWIPE_UP(); tap(233-60,233+62); SWIPE_DOWN(); render();
  // 8) Grok voice page: character states + subtitles
  go(0,-1); gw_grok_visible(true);
  gw_grok_state(0); shot("grok_idle");
  gw_grok_state(1); gw_grok_level(70); gw_grok_text("user","明天早上深圳会下雨吗？"); shot("grok_listening");
  gw_grok_state(2); shot("grok_thinking");
  gw_grok_state(3); gw_grok_text("assistant","明早深圳多云转阵雨，降雨概率 60%，6 点到 9 点风力 3 级。建议带雨衣，路面湿滑注意刹车距离。"); shot("grok_speaking");
  gw_grok_state(4); gw_grok_status("Grok 离线 · 检查 Wi-Fi"); shot("grok_offline");
  // GIFs: listening + thinking loops (30 fps), and a swipe sequence
  flushed=0; gw_grok_state(1); gif_n=0; for(int i=0;i<45;i++){ gw_grok_level(30+((i*37)%70)); step(33); gif_frame("listening"); }
  printf("PERF listening anim: avg dirty %.1f%% of screen/frame\n",100.0*gf_px/gf_cnt/(W*W)); gf_px=gf_cnt=0;
  gw_grok_state(2); gif_n=0; for(int i=0;i<45;i++){ step(33); gif_frame("thinking"); }
  printf("PERF thinking anim: avg dirty %.1f%% of screen/frame\n",100.0*gf_px/gf_cnt/(W*W)); gf_px=gf_cnt=0;
  gw_grok_visible(false); page=1; render(); gif_n=0;
  for(int k=0;k<4;k++){ for(int i=0;i<12;i++){ step(33); gif_frame("pages"); } SWIPE_LEFT(); }
  printf("PERF page swipe: avg dirty %.1f%% of screen/frame\n",100.0*gf_px/gf_cnt/(W*W));
  { int cnt=0; lv_obj_t* st[4096]; int sp=0; st[sp++]=lv_screen_active(); st[sp++]=lv_layer_top(); while(sp){ lv_obj_t* o=st[--sp]; cnt++; for(uint32_t k=0;k<lv_obj_get_child_count(o);k++) st[sp++]=lv_obj_get_child(o,k);} printf("PERF lvgl objects total: %d\n",cnt); }
  lv_mem_monitor_t m; lv_mem_monitor(&m); printf("PERF lvgl heap used: %u B\n",(unsigned)(m.total_size-m.free_size));
  printf("AUDIT missing glyphs: %d\n",missing_total); return 0; }
