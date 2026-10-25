#include "gw_slide.h"
#include "gw_mem.h"
#include <string.h>
#ifdef ESP_PLATFORM
#include "esp_timer.h"
static int64_t us(void){ return esp_timer_get_time(); }
#else
#include <time.h>
static int64_t us(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (int64_t)t.tv_sec*1000000+t.tv_nsec/1000; }
#endif
#define W GW_SLIDE_W
#define BUFSZ ((size_t)W*W*2)
static lv_draw_buf_t sb_old, sb_new; static uint8_t *m_old, *m_new; static bool have_old;
static lv_obj_t *ov, *im_old, *im_new; static int s_dir=1; static void (*done_cb)(void); static int snap_us[2];
static bool bufs(void){
  if(!m_old) m_old=(uint8_t*)gw_psram_calloc(BUFSZ);
  if(!m_new) m_new=(uint8_t*)gw_psram_calloc(BUFSZ);
  return m_old&&m_new; }
static bool snap(lv_draw_buf_t* db,uint8_t* mem,int* t){ int64_t t0=us();
  lv_draw_buf_init(db,W,W,LV_COLOR_FORMAT_RGB565,W*2,mem,BUFSZ);
  lv_obj_t* scr=lv_screen_active(); lv_obj_update_layout(scr);
  lv_result_t r=lv_snapshot_take_to_draw_buf(scr,LV_COLOR_FORMAT_RGB565,db); *t=(int)(us()-t0); return r==LV_RESULT_OK; }
bool gw_slide_active(void){ return ov!=NULL; }
void gw_slide_set_done_cb(void (*cb)(void)){ done_cb=cb; }
int gw_slide_last_snap_us(int which){ return snap_us[which?1:0]; }
static void finish(void){ if(!ov) return; lv_anim_delete(ov,NULL); lv_obj_delete(ov); ov=im_old=im_new=NULL; if(done_cb) done_cb(); }
void gw_slide_cancel(void){ finish(); }
static void ax(void* var,int32_t v){ (void)var; if(!ov) return;   // v: 0..1000 progress
  int x_new=s_dir*(W-(W*v)/1000), x_old=-s_dir*(GW_SLIDE_PARALLAX*v)/1000;
  lv_obj_set_x(im_new,x_new); lv_obj_set_x(im_old,x_old); }
static void anim_done(lv_anim_t* a){ (void)a; finish(); }
bool gw_slide_snap_old(void){ if(ov) finish(); have_old=false; if(!bufs()) return false;
  have_old=snap(&sb_old,m_old,&snap_us[0]); return have_old; }
static const gw_slide_hw_t* hw; static int st_frames, st_ms, st_max_us;
void gw_slide_set_hw(const gw_slide_hw_t* h){ hw=h; }
void gw_slide_last_stats(int* f,int* ms,int* mx){ if(f)*f=st_frames; if(ms)*ms=st_ms; if(mx)*mx=st_max_us; }
// screen x in [a,b) shows old[x-x_old]; outside the old bitmap it is black (RGB565 0)
static void seg(uint8_t* d,const uint8_t* oo,int a,int b,int x_old){ if(b<=a) return;
  int s=a-x_old, e=b-x_old, vs=s<0?0:s, ve=e>W?W:e;
  if(ve<=vs){ memset(d+a*2,0,(size_t)(b-a)*2); return; }
  if(vs>s) memset(d+a*2,0,(size_t)(vs-s)*2);
  memcpy(d+(vs+x_old)*2,oo+(size_t)vs*2,(size_t)(ve-vs)*2);
  if(ve<e) memset(d+(ve+x_old)*2,0,(size_t)(e-ve)*2); }
// one strip: the new page covers [x_new,x_new+W) on top, the old page (parallax x_old) shows in the rest
static void compose(uint8_t* dst,int y0,int n,int x_new,int x_old){ int nl=x_new>0?x_new:0, nr=x_new+W<W?x_new+W:W; if(nr<nl) nr=nl;
  for(int r=0;r<n;r++){ uint8_t* d=dst+(size_t)r*W*2; size_t o=(size_t)(y0+r)*W*2;
    if(nr>nl) memcpy(d+nl*2,m_new+o+(size_t)(nl-x_new)*2,(size_t)(nr-nl)*2);
    seg(d,m_old+o,0,nl,x_old); seg(d,m_old+o,nr,W,x_old); } }

// ---- cover transition: the new page covers the old one, which parallaxes and dims. Pages are extended past their
// round outline by repeating the rim pixel, so a sliding page is full-bleed (no round card edge, no black between pages).
#include <math.h>
static int s_style=1;   // 0 = legacy compose (disc cards), 1 = cover
void gw_slide_set_style(int s){ s_style=s; }
static uint16_t dim(uint16_t c,int a){ return (uint16_t)((((c>>11)&31)*a>>5)<<11 | (((c>>5)&63)*a>>5)<<5 | ((c&31)*a>>5)); }
static void fill16(uint16_t* d,int n,uint16_t v){ while(n-->0) *d++=v; }
static void compose_style(uint8_t* dst,int y0,int n,float q){
  int xn=s_dir*(int)(W*(1.f-q)+0.5f), xo=-s_dir*(int)(GW_SLIDE_PARALLAX*q+0.5f), da=32-(int)(14*q+0.5f);
  for(int r=0;r<n;r++){ int y=y0+r; float dy=y+0.5f-W/2.0f, h2=233.f*233.f-dy*dy; int hc=h2>0?(int)sqrtf(h2):0, lo=W/2-hc, hi=W/2+hc-1; if(hi<lo) hi=lo;
    uint16_t* d=(uint16_t*)(dst+(size_t)r*W*2); const uint16_t* pn=(const uint16_t*)m_new+(size_t)y*W; const uint16_t* po=(const uint16_t*)m_old+(size_t)y*W;
    int nl=xn>0?xn:0, nr=xn+W<W?xn+W:W;   // new page covers [nl,nr)
    for(int x=0;x<nl;x++){ int sx=x-xo; sx=sx<lo?lo:sx>hi?hi:sx; d[x]=dim(po[sx],da); }
    for(int x=nr;x<W;x++){ int sx=x-xo; sx=sx<lo?lo:sx>hi?hi:sx; d[x]=dim(po[sx],da); }
    for(int x=nl;x<nr;x++){ int sx=x-xn; d[x]=pn[sx<lo?lo:sx>hi?hi:sx]; } } }
static int64_t hnow(void){ return hw->now?hw->now():us(); }
static void run_direct(void){ int64_t t0=hnow(), tf=t0; int fr=0, mx=0, i=0; hw->wait();
  for(;;){ int el=(int)((hnow()-t0)/1000); bool last=el>=GW_SLIDE_DUR; float p=last?1.f:(float)el/GW_SLIDE_DUR, q=p*p*(3.f-2.f*p);   // smoothstep: at ~34 fps a cubic ease-out jumped 126 px in frame 1 and crawled in the last 4 (reads as a pop)
    int v=(int)(q*1000+0.5f), x_new=s_dir*(W-(W*v)/1000), x_old=-s_dir*(GW_SLIDE_PARALLAX*v)/1000;
    for(int y=0;y<W;y+=hw->rows,i++){ int n=W-y<hw->rows?W-y:hw->rows; uint8_t* b=hw->buf(i&1);
      if(s_style) compose_style(b,y,n,q); else compose(b,y,n,x_new,x_old); hw->wait(); hw->push(y,n,b); }   // compose overlaps the previous strip's transfer
    if(hw->frame_done) hw->frame_done();
    int64_t t=hnow(); int fm=(int)(t-tf); if(fm>mx) mx=fm; tf=t; fr++;
    if(last) break; }
  hw->wait(); st_frames=fr; st_ms=(int)((hnow()-t0)/1000); st_max_us=mx; }
bool gw_slide_start(int dir){ if(!have_old) return false; have_old=false; s_dir=dir<0?-1:1;
  if(!snap(&sb_new,m_new,&snap_us[1])) return false;
  if(hw){ run_direct(); if(done_cb) done_cb(); return true; }
  ov=lv_obj_create(lv_layer_top()); lv_obj_remove_style_all(ov); lv_obj_set_size(ov,W,W); lv_obj_set_pos(ov,0,0);
  lv_obj_set_style_bg_color(ov,lv_color_black(),0); lv_obj_set_style_bg_opa(ov,LV_OPA_COVER,0);   // opaque: nothing under it is redrawn
  lv_obj_remove_flag(ov,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
  // raw RGB565 in RAM: the built-in decoder reads it in place (no decoded copy to invalidate)
  im_old=lv_image_create(ov); lv_image_set_src(im_old,&sb_old); lv_obj_set_pos(im_old,0,0);
  im_new=lv_image_create(ov); lv_image_set_src(im_new,&sb_new); lv_obj_set_pos(im_new,s_dir*W,0);
  lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a,ov); lv_anim_set_exec_cb(&a,ax); lv_anim_set_values(&a,0,1000);
  lv_anim_set_duration(&a,GW_SLIDE_DUR); lv_anim_set_path_cb(&a,lv_anim_path_ease_out); lv_anim_set_completed_cb(&a,anim_done); lv_anim_start(&a);
  return true; }
