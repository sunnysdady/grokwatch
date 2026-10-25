// GrokWatch scripted host simulator: real LVGL 9.5 + real UI sources (gw_ultra.c, gw_grok.c, gw_model.c, fonts),
// 466x466 round mask, scripted touch gestures through a real LVGL pointer indev, and real TLV frames into gw_model.
#include "lvgl.h"
#include "gw_faces.h"
#include "cbin_font.h"
#ifndef GW_FONT_BIN
#error "build with sim/build.sh (defines GW_FONT_BIN)"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define W 466
static uint16_t fb[W*W]; static const char* OUT; static int shot_n=0, missing_total=0;
static long flushed=0; static void flush(lv_display_t* d,const lv_area_t* a,uint8_t* px){ int w=a->x2-a->x1+1; flushed+=(long)w*(a->y2-a->y1+1); for(int y=a->y1;y<=a->y2;y++) memcpy(&fb[y*W+a->x1],px+((long)y*W+a->x1)*2,w*2); lv_display_flush_ready(d);}
static void save_ppm(const char* path){ FILE* f=fopen(path,"wb"); fprintf(f,"P6\n%d %d\n255\n",W,W);
  for(int i=0;i<W*W;i++){ uint16_t c=fb[i]; int x=i%W,y=i/W; int dx=x-233,dy=y-233; unsigned char r=((c>>11)&31)*255/31,g=((c>>5)&63)*255/63,b=(c&31)*255/31;
    if(dx*dx+dy*dy>233*233){r=g=b=24;} fputc(r,f);fputc(g,f);fputc(b,f);} fclose(f);}
static uint32_t tick=0; static uint32_t tk(void){return tick;}
static lv_display_t* d; static int64_t now_us(void){ return (int64_t)tick*1000; }
// ---- pointer indev driven by the script
static int px_=233,py_=233; static bool pressed=false;
static void indev_read(lv_indev_t* i,lv_indev_data_t* data){ (void)i; data->point.x=px_; data->point.y=py_; data->state=pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED; }
static double now_ms(void);
static double st_ms_sum=0, st_ms_max=0; static int st_n=0;
static void step(int ms){ for(int t=0;t<ms;t+=10){ tick+=10; double a=now_ms(); lv_timer_handler(); double e=now_ms()-a; st_ms_sum+=e; if(e>st_ms_max) st_ms_max=e; st_n++; } }
static int gif_n=0; static long gf_px=0; static int gf_cnt=0;
static double fr_ms_sum=0, fr_ms_max=0; static int fr_n=0;
static double now_ms(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec*1e3+t.tv_nsec/1e6; }
static void frame(void){ double a=now_ms(); lv_refr_now(d); double e=now_ms()-a; fr_ms_sum+=e; if(e>fr_ms_max) fr_ms_max=e; fr_n++; }
// Host numbers are NOT device numbers. Device estimate = dirty bytes over QSPI (pclk 40 MHz x4 lines = 20 MB/s, RGB565).
static void perf(const char* name){ double dirty=(double)gf_px/gf_cnt; double spi_ms=dirty*2/20e6*1e3;
  printf("PERF %s: dirty %.1f%%/frame, host lv_timer_handler avg %.3f ms max %.3f ms, est. QSPI transfer %.1f ms/frame (<= %.0f fps transfer-bound)\n",
    name,100.0*dirty/(W*W),st_ms_sum/st_n,st_ms_max,spi_ms,1000.0/spi_ms); gf_px=gf_cnt=0; st_ms_sum=st_ms_max=0; st_n=0; }
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
static void gif_frame(const char* tag){ frame(); gf_px+=flushed; flushed=0; gf_cnt++; char p[256]; snprintf(p,sizeof p,"%s/gif_%s_%03d.ppm",OUT,tag,gif_n++); save_ppm(p); }
static void drag(int x0,int y0,int x1,int y1,int ms){ px_=x0;py_=y0;pressed=true; step(30); int n=ms/10; for(int i=1;i<=n;i++){ px_=x0+(x1-x0)*i/n; py_=y0+(y1-y0)*i/n; step(10);} pressed=false; step(40); }
static void tap(int x,int y){ px_=x;py_=y;pressed=true; step(60); pressed=false; step(400); }
#define SWIPE_LEFT()  drag(380,233,90,233,120)
#define SWIPE_RIGHT() drag(90,233,380,233,120)
#define SWIPE_UP()    drag(233,400,233,90,120)
#define SWIPE_DOWN()  drag(233,90,233,400,120)
// ---- page controller (same routing as nav_ui.cc)
static int page=1, back_page=1; static bool night=false, dnd=false; static gw_cc_t cc;
// fake server-side to-do store (same JSON shape as /gw/todo)
static int td_n=0, td_open=0, td_state=1; static const char* td_text[GW_TODO_ROWS]; static bool td_done[GW_TODO_ROWS];
static void todo_tap(int r){ if(r<td_n){ td_done[r]=true; printf("  todo POST /gw/todo/done row=%d\n",r); } }
static int sim_ss=40;
static void render(void){ time_t t0=time(NULL); struct tm t; localtime_r(&t0,&t); (void)t;
  gw_state_t s={.hh=21,.mm=18,.ss=sim_ss,.mon=10,.day=24,.wday=6,.battery=76,.charging=false,.wifi=true};
  s.ble=gw_model_linked(true,now_us()); s.phone_batt=gw_m.phone_batt; s.ride_km=gw_m.ride_km; s.dnd=dnd;
  s.nav_active=gw_m.nav_active; s.nav_dist=gw_m.dist; s.nav_dir=gw_m.road; s.nav_dur=gw_m.dur; s.nav_eta=gw_m.eta; s.nav_icon=gw_m.has_icon?gw_m.icon:NULL; s.light_s=gw_model_light_left(now_us()); s.light_c=gw_m.light_c; s.usage_pct=gw_m.usage_pct; s.usage_reset=gw_m.usage_reset;
  s.speed=gw_m.speed; s.ride_min=gw_model_ride_min(now_us());
  s.n_todo=td_n; s.todo_open=td_open; s.todo_state=td_state; for(int i=0;i<td_n;i++){ s.todo_text[i]=td_text[i]; s.todo_done[i]=td_done[i]; }
  gw_faces_show(page,&s); }
static void go(int p,int dd){ page=(p+GW_NCORE)%GW_NCORE; gw_faces_dir(dd); render(); }
static void cc_fill(void){ cc.battery=76; cc.wifi=true; cc.ble=gw_model_linked(true,now_us()); cc.phone_batt=gw_m.phone_batt; cc.night=night; cc.dnd=dnd; }
static void cc_tap(int id){ if(id==GW_CC_BRIGHT) cc.bright=cc.bright>=100?30:(cc.bright>=60?100:60); if(id==GW_CC_VOLUME) cc.volume=cc.volume>=90?0:(cc.volume>=60?90:(cc.volume>=30?60:30));
  if(id==GW_CC_NIGHT){ night=!night; gw_faces_set_night(night); gw_faces_create(lv_screen_active()); render(); } if(id==GW_CC_DND) dnd=!dnd; cc_fill(); gw_cc_open(&cc,cc_tap); }
static void gesture_cb(lv_event_t* e){ (void)e; lv_dir_t dd=lv_indev_get_gesture_dir(lv_indev_active());
  if(gw_cc_is_open()){ if(dd==LV_DIR_BOTTOM||dd==LV_DIR_LEFT||dd==LV_DIR_RIGHT) gw_cc_open(NULL,NULL); return; }
  if(gw_faces_sheet_open()){ if(dd==LV_DIR_LEFT||dd==LV_DIR_RIGHT) gw_faces_sheet(NULL,NULL); return; }
  if(dd==LV_DIR_LEFT) go(page+1,1); else if(dd==LV_DIR_RIGHT) go(page-1,-1);
  else if(dd==LV_DIR_TOP){ cc_fill(); gw_cc_open(&cc,cc_tap); } }
// ---- TLV injection (same bytes the Android app sends)
static uint8_t fr[1024];
static void rx(uint8_t t,const void* p,int l){ int n=gw_frame(fr,t,p,l); int ev=gw_model_rx(fr,n,now_us());
  if(ev&GW_EV_NAV_START) page=GW_PAGE_NAV; if(ev&GW_EV_SUMMARY) gw_faces_sheet("骑行结束",gw_m.summary);
  if(ev&(GW_EV_TURN_SOON|GW_EV_TURN_NOW)) printf("  haptic: %s\n",ev&GW_EV_TURN_NOW?"double (turn now)":"long (200 m)"); render(); }
static void rx_strs(uint8_t t,const char* a,const char* b,const char* c,const char* d4){ uint8_t p[900]; int n=0; const char* v[4]={a,b,c,d4};
  for(int i=0;i<4;i++){ if(!v[i]) break; int l=strlen(v[i]); memcpy(p+n,v[i],l); n+=l; p[n++]=0; } rx(t,p,n); }
static void hello(void); static void icon_left(void); static void nav_on(int on);
static void rx5(const char* a,const char* b,const char* c,const char* d4,const char* e){ uint8_t p[300]; int n=0; const char* v[5]={a,b,c,d4,e};
  for(int i=0;i<5;i++){ if(!v[i]) break; int l=strlen(v[i]); memcpy(p+n,v[i],l); n+=l; p[n++]=0; } rx(0x11,p,n); }
static void usage(int pct,const char* r){ uint8_t p[20]; p[0]=pct; int l=r?strlen(r):0; memcpy(p+1,r,l); p[1+l]=0; rx(0x50,p,l+2); }
static void v11_extra(const char* D){ char nm[48]; gw_grok_visible(false);
  hello(); nav_on(1); icon_left(); rx5("350 米","科技园南路","12分钟","21:31","L=23;C=R"); page=GW_PAGE_NAV; render(); snprintf(nm,sizeof nm,"v11_%s_nav_light_red",D); shot(nm);
  rx5("180 米","科技园南路","11分钟","21:31","L=8;C=G"); snprintf(nm,sizeof nm,"v11_%s_nav_light_green",D); shot(nm);
  rx_strs(0x11,"350 米","科技园南路","12分钟","21:31"); snprintf(nm,sizeof nm,"v11_%s_nav_nolight",D); shot(nm);
  rx_strs(0x11,"350 米","科技园南路","19分钟",""); snprintf(nm,sizeof nm,"v11_%s_nav_noeta",D); shot(nm);
  rx_strs(0x11,"","正在导航","",""); snprintf(nm,sizeof nm,"v11_%s_nav_fallback",D); shot(nm); nav_on(0);
  gw_m.usage_pct=-1; gw_m.usage_reset[0]=0; go(GW_PAGE_FACE,1); snprintf(nm,sizeof nm,"v11_%s_face_usage_none",D); shot(nm);
  usage(42,"18:00"); go(GW_PAGE_FACE,1); snprintf(nm,sizeof nm,"v11_%s_face_usage_42",D); shot(nm);
  usage(100,"周一"); go(GW_PAGE_FACE,1); snprintf(nm,sizeof nm,"v11_%s_face_usage_100",D); shot(nm); gw_m.usage_pct=-1; }
static void hello(void){ uint8_t p[2]={82,0}; rx(0x01,p,2); }
static void speed(float k){ int v=(int)(k*10); uint8_t p[2]={v&255,v>>8}; rx(0x20,p,2); }
static void nav_on(int on){ uint8_t p[1]={on}; rx(0x10,p,1); }
static void icon_left(void){ uint8_t ic[288]={0}; for(int y=0;y<48;y++)for(int x=0;x<48;x++){ int on=0; if(x>=28&&x<36&&y>=18&&y<44) on=1; if(y>=14&&y<22&&x>=14&&x<36) on=1; if(x>=4&&x<18&&abs(y-18)<=(x-4)) on=1;
    if(on) ic[(y*48+x)/8]|=0x80>>((y*48+x)%8);} for(int part=0;part<3;part++){ uint8_t p[97]; p[0]=part; memcpy(p+1,ic+96*part,96); rx(0x12,p,97);} }
int main(int argc,char**argv){
  OUT=argv[1]; lv_init(); lv_tick_set_cb(tk);
  d=lv_display_create(W,W); static uint8_t buf[W*W*2]; lv_display_set_buffers(d,buf,NULL,sizeof buf,LV_DISPLAY_RENDER_MODE_DIRECT); lv_display_set_flush_cb(d,flush);
  lv_indev_t* in=lv_indev_create(); lv_indev_set_type(in,LV_INDEV_TYPE_POINTER); lv_indev_set_read_cb(in,indev_read);
  const char* fp=GW_FONT_BIN; /* absolute path injected by build.sh (-DGW_FONT_BIN=...) */ FILE* ff=fopen(fp,"rb"); if(!ff){ fprintf(stderr,"font not found: %s (run build.sh)\n",fp); return 1; } fseek(ff,0,SEEK_END); long n=ftell(ff); rewind(ff); uint8_t* fbn=malloc(n); fread(fbn,1,n,ff); fclose(ff);
#ifdef GW_SIM_NO_CBIN   /* 64-bit hosts (arm64 Mac): cbin fonts need 32-bit pointers; use the built-in 20 px CJK font */
  LV_FONT_DECLARE(gz_20); (void)fbn; lv_font_t* noto=(lv_font_t*)&gz_20;
#else
  lv_font_t* noto=cbin_font_create(fbn);
#endif
  lv_obj_t* scr=lv_screen_active(); lv_obj_set_style_bg_color(scr,lv_color_black(),0); lv_obj_set_style_text_font(scr,noto,0); lv_obj_set_style_text_color(scr,lv_color_white(),0);
  lv_obj_t* cont=lv_obj_create(scr); lv_obj_remove_style_all(cont); lv_obj_set_size(cont,W,W); lv_obj_set_style_bg_opa(cont,255,0); lv_obj_set_style_bg_color(cont,lv_color_black(),0);
  gw_faces_set_fallback(noto); gw_faces_set_fade(false); gw_faces_create(scr); lv_obj_add_flag(scr,LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(scr,gesture_cb,LV_EVENT_GESTURE,NULL);
  cc.bright=60; cc.volume=60; tick=1000000;
  // 1) cold: phone app not connected
  go(2,1); shot("ride_phone_not_connected");
  go(GW_PAGE_NAV,1); shot("nav_phone_not_connected");
  // 2) app connects (heartbeat), no GPS yet
  hello(); go(2,-1); shot("ride_waiting_gps");
  // 3) riding: speed every second for 3 minutes
  for(int i=0;i<180;i++){ speed(18+(i%40)/3.0f); step(1000); if(i%5==0) hello(); }
  shot("ride_moving");
  // 4) legacy NOTIFY frames from an old app must be ignored (notifications removed in 2.6.2)
  rx_strs(0x30,"微信","张三","今晚七点老地方吃饭？记得带上头盔，我们吃完顺便骑一圈深圳湾，大概二十公里，十点前回来。",NULL);
  rx_strs(0x30,"短信","95338","【顺丰速运】您的快递已放入科技园南区驿站，取件码 3-2-1088，请于今日 21:00 前领取。",NULL);
  rx_strs(0x30,"微信","骑行俱乐部（32）","李四：周六早上六点半在人才公园集合，路线：深圳湾—大沙河—塘朗山，全程约 65 公里，爬升 600 米。请大家检查刹车和胎压，带够补给。没有尾灯的同学这次一定要装上，夜骑回来天会黑。",NULL);
  rx_strs(0x30,"Gmail","GitHub","[xiaozhi-esp32] New release v2.6.0 is available with improved OTA and display fixes.",NULL);
  rx_strs(0x30,"日历","骑行俱乐部例会","19:30 · 南山区科技园 3 号会议室",NULL);
  rx_strs(0x30,"微信","张三","[2条] 张三：到了吗？",NULL);           // same chat: replaces older entry
  for(int k=0;k<4;k++){ speed(20); hello(); step(1000); }
  page=2; render(); SWIPE_DOWN(); step(300); if(page!=2) printf("AUDIT FAIL swipe-down changed page\n"); shot("ride_after_legacy_notify_and_swipe_down");
  // 5) navigation from AMap: 350 米 -> 180 米 (long buzz) -> 30 米 (double buzz)
  hello(); nav_on(1); icon_left(); rx_strs(0x11,"350 米","科技园南路","12分钟","21:31"); shot("nav_350m");
  rx_strs(0x11,"180 米","科技园南路","11分钟","21:31"); rx_strs(0x11,"30 米","科技园南路","11分钟","21:31"); shot("nav_30m");
  go(2,-1); shot("ride_navigating_eta");
  // 6) arrival: nav ends -> ride summary sheet
  nav_on(0); shot("ride_summary");
  gw_faces_sheet(NULL,NULL);
  // 7) face + Control Center
  page=1; render(); shot("face");
  // second hand: 6 one-second spring ticks, 20 ms frames; dirty area per frame vs. the frozen face
  { gif_n=0; gf_px=0; gf_cnt=0; flushed=0; for(int k=0;k<50;k++){ step(20); gif_frame("still"); } perf("face idle (no tick)");
    gif_n=0; gf_px=0; gf_cnt=0; st_ms_sum=st_ms_max=0; st_n=0;
    for(int k=0;k<6;k++){ sim_ss=(sim_ss+1)%60; render(); for(int f=0;f<50;f++){ step(20); gif_frame("sec"); } } perf("second hand tick (1 s)");
    sim_ss=40; }
  SWIPE_UP(); shot("control_center");
  tap(233-60,233-70); tap(233+60,233+62); shot("control_center_bright_dnd");
  tap(233-60,233+62); shot("control_center_night");
  SWIPE_DOWN(); go(1,1); shot("face_night");
  SWIPE_UP(); tap(233-60,233+62); SWIPE_DOWN(); render();
  // 7b) voice to-do page: syncing, offline, list, check-off, empty
  gw_faces_set_todo_cb(todo_tap);
  go(GW_PAGE_TODO,1); shot("todo_syncing");
  td_state=2; render(); shot("todo_offline");
  { static const char* T[]={"买内胎 700x25C","周六六点半人才公园集合","给尾灯充电","还张三 200 块","预约码表支架维修，顺便问一下碳纤维座管有没有货"}; td_state=0; td_n=5; td_open=7; for(int i=0;i<5;i++){ td_text[i]=T[i]; td_done[i]=false; } }
  render(); shot("todo_list");
  tap(233,233-112+54*2); render(); shot("todo_checked");
  td_n=0; td_open=0; render(); shot("todo_empty");
  // 8) Grok voice page: character states + subtitles
  go(0,-1); gw_grok_visible(true);
  gw_grok_state(0); shot("grok_idle");
  gw_grok_state(1); gw_grok_level(70); gw_grok_text("user","明天早上深圳会下雨吗？"); shot("grok_listening");
  gw_grok_state(2); shot("grok_thinking");
  gw_grok_state(3); gw_grok_text("assistant","明早深圳多云转阵雨，降雨概率 60%，6 点到 9 点风力 3 级。建议带雨衣，路面湿滑注意刹车距离。"); shot("grok_speaking");
  gw_grok_state(4); gw_grok_status("Grok 离线 · 检查 Wi-Fi"); shot("grok_offline");
  // 2.6.11 subtitle review: short Q&A, ack status, long reply (server sends it sentence by sentence), day+night
  for(int n=0;n<2;n++){ char nm[48]; const char* D=n?"night":"day"; gw_grok_set_night(n);
    gw_grok_state(3); gw_grok_text("user","明天周几？"); gw_grok_text("assistant","明天是周一，10月26号。"); snprintf(nm,sizeof nm,"v11_%s_short",D); shot(nm);
    gw_grok_state(2); gw_grok_text("user","你现在能连接上我的Grokbot吗？"); snprintf(nm,sizeof nm,"v11_%s_question",D); shot(nm);
    gw_grok_state(3); gw_grok_text("assistant","已收到 正在处理。"); snprintf(nm,sizeof nm,"v11_%s_ack",D); shot(nm);
    gw_grok_text("assistant","能连上，我是你的私人助理，"); snprintf(nm,sizeof nm,"v11_%s_long1",D); shot(nm);
    gw_grok_text("assistant","手表的问题会经过 Slack 转到我这里，"); snprintf(nm,sizeof nm,"v11_%s_long3",D); shot(nm);
    gw_grok_text("assistant","能连上，我是你的私人助理，现在正在回你的话。手表的问题会经过 Slack 转到我这里，你直接问就行。"); snprintf(nm,sizeof nm,"v11_%s_longraw",D); shot(nm);
    gw_grok_text("user",""); for(int st=0;st<5;st++){ gw_grok_state(st); step(st==1?400:300); snprintf(nm,sizeof nm,"v11_%s_expr%d",D,st); shot(nm); }
    gw_grok_state(0); }
  gw_grok_set_night(false); v11_extra("day"); go(0,-1); gw_grok_visible(true);
  // face v2 review: 3 variants x 5 states, day + night (subtitles cleared so the face reads alone)
  gw_grok_text("user",""); for(int n=0;n<2;n++){ gw_grok_set_night(n); for(int v=0;v<3;v++){ gw_grok_variant(v); for(int st=0;st<5;st++){ char nm[40];
    gw_grok_state(st); step(st==1?400:300); snprintf(nm,sizeof nm,"fv%d_%s_s%d",v,n?"night":"day",st); shot(nm); } } }
  gw_grok_set_night(false); gw_grok_variant(2);
  // GIFs: listening + thinking loops (30 fps), and a swipe sequence
  flushed=0; st_ms_sum=st_ms_max=0; st_n=0; gw_grok_state(1); gif_n=0; for(int i=0;i<45;i++){ gw_grok_level(30+((i*37)%70)); step(33); gif_frame("listening"); }
  perf("listening anim");
  gw_grok_state(2); gif_n=0; for(int i=0;i<45;i++){ step(33); gif_frame("thinking"); }
  perf("thinking anim");
  { const char* nm[2]={"idle (blink+glance)","speaking"}; int stt[2]={0,3}; for(int m=0;m<2;m++){ gw_grok_state(stt[m]); step(800); frame(); flushed=0;
      int fr=0; long px=0, mx=0; for(int k=0;k<300;k++){ step(33); frame(); if(flushed){ fr++; px+=flushed; if(flushed>mx) mx=flushed; } flushed=0; }
      printf("PERF grok %s 10 s @33 ms: redraw frames %d/300, avg dirty %.1f%% of screen per redrawn frame, max %.1f%% (avatar box 220x220 = 22.3%%), est. QSPI %.1f ms per redrawn frame\n",
        nm[m],fr,fr?100.0*px/fr/(W*W):0,100.0*mx/(W*W),fr?px/fr*2/20e6*1e3:0); } gw_grok_state(0); }
  gw_grok_visible(false); page=1; render(); gif_n=0;
  for(int k=0;k<4;k++){ for(int i=0;i<12;i++){ step(33); gif_frame("pages"); } SWIPE_LEFT(); }
  perf("page swipe");
  // 8b) swipe direction audit: left swipe -> next page enters from the RIGHT (x>0 -> 0), right swipe -> from the LEFT (x<0 -> 0)
  { int fails=0; page=1; render(); step(400);
    struct { const char* n; int exp_page; int sign; } cs[]={{"left",2,1},{"left",3,1},{"right",2,-1},{"right",1,-1}};
    for(int c=0;c<4;c++){ int x0=0,xmid=0;
      px_=cs[c].sign>0?380:90; py_=233; pressed=true; step(30); for(int i=1;i<=12;i++){ px_+=(cs[c].sign>0?-24:24); step(10);
        if(!x0 && gw_faces_page_x()) x0=gw_faces_page_x(); } pressed=false;
      for(int f=0;f<6;f++){ step(40); int x=gw_faces_page_x(); if(f==1) xmid=x; gif_frame(c<2?"slide_left":"slide_right"); printf("  slide %s f%d x=%d\n",cs[c].n,f,x); }
      step(300); int xe=gw_faces_page_x();
      bool ok=page==cs[c].exp_page && x0*cs[c].sign>0 && xmid*cs[c].sign>=0 && abs(xmid)<abs(x0) && xe==0;
      printf("%s swipe %s -> page %d: x start %d mid %d end %d\n",ok?"AUDIT OK":"AUDIT FAIL",cs[c].n,page,x0,xmid,xe); fails+=!ok; }
    perf("slide anim"); printf("AUDIT swipe direction fails: %d\n",fails); }
  // 8c) night pass over every core page (same model state as the day shots)
  gw_grok_visible(false); night=true; gw_faces_set_night(true); gw_faces_create(lv_screen_active());
  go(1,1); shot("n_face"); speed(23.4f); go(2,1); shot("n_ride");
  nav_on(1); icon_left(); rx_strs(0x11,"350 米","科技园南路","12分钟","21:31"); page=GW_PAGE_NAV; render(); shot("n_nav"); nav_on(0); v11_extra("night");
  td_n=3; td_open=2; td_state=1; td_text[0]="买水"; td_text[1]="给车胎打气"; td_text[2]="周日骑行路线"; td_done[0]=false; td_done[1]=false; td_done[2]=true;
  go(GW_PAGE_TODO,1); shot("n_todo"); td_n=0; td_open=0; render(); shot("n_todo_empty");
  go(0,-1); gw_grok_state(0); gw_grok_text("user",""); shot("n_grok_idle");
  gw_grok_state(3); gw_grok_text("user","明天早上深圳会下雨吗？"); gw_grok_text("assistant","明早深圳多云转阵雨，降雨概率 60%，6 点到 9 点风力 3 级。建议带雨衣，路面湿滑注意刹车距离。"); shot("n_grok_speaking");
  go(1,1);
  night=false; gw_faces_set_night(false); gw_faces_create(lv_screen_active()); page=1; render();
  // 9) Standby clock (day, night, minute drift)
  gw_grok_visible(false); go(1,1); gw_standby_create(lv_layer_top());
  gw_standby_set(10,9,0); gw_standby_show(true); shot("standby");
  gw_faces_set_night(true); gw_standby_set(10,9,0); shot("standby_night"); gw_faces_set_night(false);
  gw_standby_set(9,41,3); shot("standby_drift_min3");
  gw_standby_set(1,23,0); shot("standby_glyphs_0123"); gw_standby_set(45,67,0); shot("standby_glyphs_4567"); gw_standby_set(89,0,0); shot("standby_glyphs_8900"); gw_standby_show(false);
  { int cnt=0; lv_obj_t* st[4096]; int sp=0; st[sp++]=lv_screen_active(); st[sp++]=lv_layer_top(); while(sp){ lv_obj_t* o=st[--sp]; cnt++; for(uint32_t k=0;k<lv_obj_get_child_count(o);k++) st[sp++]=lv_obj_get_child(o,k);} printf("PERF lvgl objects total: %d\n",cnt); }
  lv_mem_monitor_t m; lv_mem_monitor(&m); printf("PERF lvgl heap used: %u B of %u, peak %u B, frag %u%%\n",(unsigned)(m.total_size-m.free_size),(unsigned)m.total_size,(unsigned)m.max_used,(unsigned)m.frag_pct);
  printf("AUDIT missing glyphs: %d\n",missing_total); return 0; }
