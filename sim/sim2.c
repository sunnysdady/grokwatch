#include "lvgl.h"
#include "gw_faces.h"
#include "cbin_font.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define W 466
LV_FONT_DECLARE(font_noto_emoji_30_4)
static uint16_t fb[W*W];
static void flush(lv_display_t* d,const lv_area_t* a,uint8_t* px){ int w=a->x2-a->x1+1; for(int y=a->y1;y<=a->y2;y++) memcpy(&fb[y*W+a->x1],px+(y-a->y1)*w*2,w*2); lv_display_flush_ready(d);}
static void save(const char* path){ FILE* f=fopen(path,"wb"); fprintf(f,"P6\n%d %d\n255\n",W,W);
  for(int i=0;i<W*W;i++){ uint16_t c=fb[i]; int x=i%W,y=i/W; int dx=x-233,dy=y-233; unsigned char r=((c>>11)&31)*255/31,g=((c>>5)&63)*255/63,b=(c&31)*255/31;
    if(dx*dx+dy*dy>233*233){r=g=b=24;} fputc(r,f);fputc(g,f);fputc(b,f);} fclose(f);}
static uint32_t tick=0; static uint32_t tk(void){return tick;}
static lv_display_t* d;
static double frame(int ms){ tick+=ms; struct timespec a,b; clock_gettime(CLOCK_MONOTONIC,&a); lv_timer_handler(); lv_obj_invalidate(lv_screen_active()); lv_refr_now(d); clock_gettime(CLOCK_MONOTONIC,&b); return (b.tv_sec-a.tv_sec)*1e3+(b.tv_nsec-a.tv_nsec)/1e6; }
static void settle(void){ for(int i=0;i<80;i++) frame(33); }
int main(int argc,char**argv){
  const char* out=argv[1]; int night=argc>2&&atoi(argv[2]); int gif=argc>3&&atoi(argv[3]); int fade=argc>4?atoi(argv[4]):1;
  lv_init(); lv_tick_set_cb(tk);
  d=lv_display_create(W,W); static uint8_t buf[W*W*2]; lv_display_set_buffers(d,buf,NULL,sizeof buf,LV_DISPLAY_RENDER_MODE_FULL); lv_display_set_flush_cb(d,flush);
  FILE* ff=fopen("$XIAOZHI/managed_components/78__xiaozhi-fonts/cbin/font_noto_sans_common_30_4.bin","rb"); fseek(ff,0,SEEK_END); long n=ftell(ff); rewind(ff); uint8_t* fbn=malloc(n); fread(fbn,1,n,ff); fclose(ff); lv_font_t* noto=cbin_font_create(fbn);
  lv_obj_t* scr=lv_screen_active(); lv_obj_set_style_bg_color(scr,lv_color_black(),0); lv_obj_set_style_text_font(scr,noto,0); lv_obj_set_style_text_color(scr,lv_color_white(),0);
  // mock of xiaozhi native chat widgets, positioned exactly like RoundLcdDisplay::SetupUI (RC5b)
  lv_obj_t* cont=lv_obj_create(scr); lv_obj_remove_style_all(cont); lv_obj_set_size(cont,W,W); lv_obj_set_style_bg_color(cont,lv_color_black(),0); lv_obj_set_style_bg_opa(cont,255,0);
  static uint8_t epx[64*64*4]; FILE* ef=fopen("happy64.argb","rb"); fread(epx,1,sizeof epx,ef); fclose(ef);
  static lv_image_dsc_t ed; ed.header.cf=LV_COLOR_FORMAT_ARGB8888; ed.header.w=64; ed.header.h=64; ed.header.stride=256; ed.data=epx; ed.data_size=sizeof epx;
  lv_obj_t* emo=lv_image_create(scr); lv_image_set_src(emo,&ed); lv_obj_center(emo); lv_image_set_scale(emo,150*256/64);
  lv_obj_t* st=lv_label_create(scr); lv_obj_set_width(st,280); lv_obj_set_style_text_align(st,LV_TEXT_ALIGN_CENTER,0); lv_obj_set_style_text_color(st,lv_color_hex(night?0xFF0000:0xFF6A13),0); lv_label_set_text(st,"聆听中…"); lv_obj_align(st,LV_ALIGN_CENTER,0,-118);
  lv_obj_t* msg=lv_label_create(scr); lv_obj_set_width(msg,300); lv_label_set_long_mode(msg,LV_LABEL_LONG_DOT); lv_obj_set_height(msg,42); lv_obj_set_style_text_align(msg,LV_TEXT_ALIGN_CENTER,0); lv_label_set_text(msg,"前方 350 米左转，进入科技园南路"); lv_obj_align(msg,LV_ALIGN_CENTER,0,118);
  gw_faces_set_night(night); gw_faces_set_fade(fade); gw_faces_create(scr);
  uint8_t icon[288]; memset(icon,0,288);
  for(int y=0;y<48;y++)for(int x=0;x<48;x++){ int on=0; if(x>=20&&x<28&&y>=16&&y<44) on=1; if(y>=4&&y<22&&abs(x-24)<=(y-4)) on=1; if(on) icon[(y*48+x)/8]|=0x80>>((y*48+x)%8);}
  uint8_t icon2[288]; memset(icon2,0,288);
  for(int y=0;y<48;y++)for(int x=0;x<48;x++){ int on=0; if(x>=30&&x<38&&y>=20&&y<44) on=1; if(y>=16&&y<24&&x>=14&&x<38) on=1; if(x>=4&&x<20&&abs(y-20)<=(x-4)) on=1; if(on) icon2[(y*48+x)/8]|=0x80>>((y*48+x)%8);}
  const char* notes[3]={"微信 · 张三：今晚七点老地方吃饭？","顺丰速运：您的快递已放入驿站，取件码 3-2-1088","日历：19:30 骑行俱乐部例会"};
  gw_state_t s={.hh=19,.mm=42,.ss=38,.mon=10,.day=24,.wday=6,.battery=76,.charging=false,.wifi=true,.ble=true,.n_notes=3,.notes={notes[0],notes[1],notes[2]},
    .nav_active=true,.nav_dist="350 m",.nav_dir="左转进入 科技园南路",.nav_dur="12 min",.nav_eta="19:55",.nav_icon=icon,.speed="23.4",.ride_min=27};
  char p[256];
  if(gif){ // page swipe sequence: face -> ride -> notes -> nav, 30 fps
    int seq[]={GW_PAGE_FACE,GW_PAGE_RIDE,GW_PAGE_NOTES,GW_PAGE_NAV,GW_PAGE_FACE}; int fi=0; double tot=0; int cnt=0;
    for(int k=0;k<5;k++){ gw_faces_show(seq[k],&s); for(int f=0;f<27;f++){ double ms=frame(33); if(k){tot+=ms;cnt++;} if(seq[k]==GW_PAGE_NAV&&f==15){ s.nav_icon=icon2; s.nav_dist="80 m"; s.nav_dir="右转 进入 深南大道"; gw_faces_show(GW_PAGE_NAV,&s);} snprintf(p,sizeof p,"%s_f%03d.ppm",out,fi++); save(p);} }
    fprintf(stderr,"host avg full-frame render %.2f ms over %d frames (fade=%d)\n",tot/cnt,cnt,fade); return 0; }
  struct { int page; const char* name; int mod; } shots[]={{1,"face",0},{2,"ride",0},{2,"ride_idle",1},{2,"ride_nophone",6},{3,"notes",0},{3,"notes_empty",2},{4,"nav_active",0},{4,"nav_idle",3},{4,"nav_nophone",4},{0,"chat",0},{3,"notes_open",5},{2,"ride_summary",7}};
  for(unsigned i=0;i<sizeof shots/sizeof shots[0];i++){ gw_state_t t=s; if(shots[i].mod==1){t.speed="";t.nav_active=false;} if(shots[i].mod==2) t.n_notes=0; if(shots[i].mod==3) t.nav_active=false; if(shots[i].mod==4){t.nav_active=false;t.ble=false;} if(shots[i].mod==6){t.speed="";t.nav_active=false;t.ble=false;t.ride_min=0;}
    gw_faces_sheet(NULL,NULL); gw_faces_show(GW_PAGE_CHAT,&t); settle(); gw_faces_show(shots[i].page,&t); if(shots[i].mod==5) gw_faces_sheet("顺丰速运",notes[1]); if(shots[i].mod==7) gw_faces_sheet("骑行结束","用时 0:42  ·  12.6 km\n最高 31  ·  平均 18 km/h"); settle(); snprintf(p,sizeof p,"%s_%s.ppm",out,shots[i].name); save(p); }
  return 0; }
