#include "lvgl.h"
#include "gw_faces.h"
#include "cbin_font.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define W 466
static uint16_t fb[W*W];
static void flush(lv_display_t* d,const lv_area_t* a,uint8_t* px){ int w=a->x2-a->x1+1; for(int y=a->y1;y<=a->y2;y++) memcpy(&fb[y*W+a->x1],px+(y-a->y1)*w*2,w*2); lv_display_flush_ready(d);}
static void save(const char* path){ FILE* f=fopen(path,"wb"); fprintf(f,"P6\n%d %d\n255\n",W,W);
  for(int i=0;i<W*W;i++){ uint16_t c=fb[i]; int x=i%W,y=i/W; int dx=x-233,dy=y-233; unsigned char r=((c>>11)&31)*255/31,g=((c>>5)&63)*255/63,b=(c&31)*255/31;
    if(dx*dx+dy*dy>233*233){r=g=b=24;} fputc(r,f);fputc(g,f);fputc(b,f);} fclose(f);}
static uint32_t tick=0; static uint32_t tk(void){return tick;}
int main(int argc,char**argv){
  int style=atoi(argv[1]); const char* out=argv[2];
  lv_init(); lv_tick_set_cb(tk);
  lv_display_t* d=lv_display_create(W,W); static uint8_t buf[W*W*2]; lv_display_set_buffers(d,buf,NULL,sizeof buf,LV_DISPLAY_RENDER_MODE_FULL); lv_display_set_flush_cb(d,flush);
  FILE* ff=fopen("$XIAOZHI/managed_components/78__xiaozhi-fonts/cbin/font_noto_sans_common_30_4.bin","rb"); fseek(ff,0,SEEK_END); long n=ftell(ff); rewind(ff); uint8_t* fbn=malloc(n); fread(fbn,1,n,ff); fclose(ff); lv_font_t* noto=cbin_font_create(fbn);
  lv_obj_t* scr=lv_screen_active(); lv_obj_set_style_bg_color(scr,lv_color_black(),0); if(noto) lv_obj_set_style_text_font(scr,noto,0); else fprintf(stderr,"no noto\n");
  gw_faces_set_style(style); gw_faces_create(scr);
  static const uint8_t arrow[288]={0}; uint8_t icon[288]; memset(icon,0,288);
  for(int y=0;y<48;y++)for(int x=0;x<48;x++){ int on=0; if(x>=20&&x<28&&y>=14&&y<44) on=1; if(y>=6&&y<22&&abs(x-24)<= (y-6)) on=1; if(on) icon[(y*48+x)/8]|=0x80>>((y*48+x)%8);} (void)arrow;
  const char* notes[3]={"微信 · 张三：今晚七点老地方吃饭？","顺丰：您的快递已放入驿站，取件码 3-2-1088",NULL};
  gw_state_t s={.hh=19,.mm=42,.ss=38,.mon=10,.day=24,.wday=6,.battery=76,.charging=false,.wifi=true,.ble=true,.n_notes=2,.notes={notes[0],notes[1]},
    .nav_active=false,.nav_dist="350 m",.nav_dir="左转进入 科技园南路",.nav_dur="12 min",.nav_eta="19:55",.nav_icon=icon,.speed="",.ride_min=27};
  struct { int page; const char* name; int mod; } shots[]={{1,"face",0},{2,"ride",1},{3,"notes",0},{3,"notes_empty",2},{4,"nav_idle",0},{4,"nav_active",3},{2,"ride_idle",0}};
  for(unsigned i=0;i<sizeof shots/sizeof shots[0];i++){ gw_state_t t=s; if(shots[i].mod==1){t.speed="23.4";t.nav_active=true;} if(shots[i].mod==2) t.n_notes=0; if(shots[i].mod==3) t.nav_active=true;
    gw_faces_show(shots[i].page,&t); tick+=100; lv_refr_now(d); char p[256]; snprintf(p,sizeof p,"%s_%s.ppm",out,shots[i].name); save(p); }
  return 0; }
