#include "gw_slide.h"
#include "gw_mem.h"
#include <string.h>
#include "src/display/lv_display_private.h"   // inv_p
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
static int s_style=2;   // 0 = legacy compose (two disc "coins"), 1 = cover (56 ms/frame on device), 2 = push (default)
static int64_t pre_us;   // when the old page was snapped at touch-down (0 = none)
bool gw_slide_snap_old(void){ if(ov) finish(); int64_t n=us();
  if(have_old&&pre_us&&n-pre_us<1500000){ pre_us=0; snap_us[0]=0; return true; }   // snapped at touch-down: the swipe pays nothing
  pre_us=0; have_old=false; if(!bufs()) return false;
  have_old=snap(&sb_old,m_old,&snap_us[0]); return have_old; }
void gw_slide_prepare(void){ if(ov||!bufs()) return; have_old=snap(&sb_old,m_old,&snap_us[0]); pre_us=have_old?us():0; }
static const gw_slide_hw_t* hw; static int st_frames, st_ms, st_max_us, st_comp_ms, st_wait_ms;
void gw_slide_set_hw(const gw_slide_hw_t* h){ hw=h; }
void gw_slide_last_split(int* comp_ms,int* wait_ms){ if(comp_ms)*comp_ms=st_comp_ms; if(wait_ms)*wait_ms=st_wait_ms; }
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

// ---- cover transition: the new page is its own round card that slides over the old page (which parallaxes). Only the
// card's chord is composed (memcpy spans, no per-pixel clamp), with a short edge shadow on the old page.
#include <math.h>
void gw_slide_set_style(int s){ s_style=s; }
static uint16_t dim(uint16_t c,int a){ return (uint16_t)((((c>>11)&31)*a>>5)<<11 | (((c>>5)&63)*a>>5)<<5 | ((c&31)*a>>5)); }
#define SHADOW 10   // px of soft shade the new page casts on the old one
static int16_t s_lo[W];
static void compose_style(uint8_t* dst,int y0,int n,float q){
  if(!s_lo[0]) for(int y=0;y<W;y++){ float dy=y+0.5f-W/2.0f, h2=233.f*233.f-dy*dy; s_lo[y]=(int16_t)(W/2-(h2>0?(int)sqrtf(h2):0)); }
  int xn=s_dir*(int)(W*(1.f-q)+0.5f), xo=-s_dir*(int)(GW_SLIDE_PARALLAX*q+0.5f);
  for(int r=0;r<n;r++){ int y=y0+r, lo=s_lo[y], hi=W-1-lo; uint8_t* d=dst+(size_t)r*W*2; size_t o=(size_t)y*W*2;
    int nl=xn+lo<0?0:xn+lo>W?W:xn+lo, nr=xn+hi+1>W?W:xn+hi+1; if(nr<nl) nr=nl;   // the new page is its own round card: only its chord covers the old page
    seg(d,m_old+o,0,nl,xo); seg(d,m_old+o,nr,W,xo);
    if(nr>nl){ memcpy(d+nl*2,m_new+o+(size_t)(nl-xn)*2,(size_t)(nr-nl)*2);
      uint16_t* p=(uint16_t*)d; for(int k=0;k<SHADOW;k++){ int x=s_dir>0?nl-1-k:nr+k; if(x<0||x>=W) break; p[x]=dim(p[x],32-(SHADOW-k)*14/SHADOW); } } } }
// ---- push transition (style 2): one shared background, both pages' discs ride on it 1:1, so there are no black
// crescents or hard seams between two "coins". Each page is clipped to its own disc; the gap between them is the bg.
static uint16_t s_bg;
static uint16_t pick_bg(void){ const uint16_t* n=(const uint16_t*)m_new; uint16_t c[32]; int best=0,bc=0;   // the page's own fill: majority colour on a ring well inside the rim
  for(int i=0;i<32;i++){ float a=i*0.19635f; c[i]=n[(int)(W/2+196*sinf(a))*W+(int)(W/2+196*cosf(a))]; }
  for(int i=0;i<32;i++){ int k=0; for(int j=0;j<32;j++) k+=c[j]==c[i]; if(k>bc){ bc=k; best=i; } } return c[best]; }
#define PUSH_R 233   // source disc = the whole glass: pages are frameless on black, so nothing is held back as a fixed rim
static int16_t s_in[W];   // source chord start per row (W/2 = row outside the source disc)
// copy source chord [sl,sh] shifted by xs, clipped to the screen chord [lo,hi]
static void span(uint8_t* d,const uint8_t* src,int xs,int sl,int sh,int lo,int hi){ if(sh<sl) return; int a=xs+sl, b=xs+sh+1; if(a<lo) a=lo; if(b>hi+1) b=hi+1; if(b>a) memcpy(d+a*2,src+(size_t)(a-xs)*2,(size_t)(b-a)*2); }
static void compose_push(uint8_t* dst,int y0,int n,float q){ compose_style(dst,y0,0,q);   // fills s_lo
  if(q>=1.f){ compose(dst,y0,n,0,0); return; }   // last frame: exactly the real page, so the landing redraw can be skipped
  if(!s_in[0]) for(int y=0;y<W;y++){ float dy=y+0.5f-W/2.0f, h2=(float)PUSH_R*PUSH_R-dy*dy; s_in[y]=(int16_t)(h2>0?W/2-(int)sqrtf(h2):W/2); }
  int xn=s_dir*(int)(W*(1.f-q)+0.5f), xo=xn-s_dir*W;
  for(int r=0;r<n;r++){ int y=y0+r, lo=s_lo[y], hi=W-1-lo, sl=s_in[y], sh=W-1-sl; uint16_t* d=(uint16_t*)(dst+(size_t)r*W*2); size_t o=(size_t)y*W*2;
    const uint16_t* rim=(const uint16_t*)(m_new+o); int il=sl>lo?sl:lo, ih=sh<hi?sh:hi;   // the rim stays put (new page's), content slides under it
    for(int x=0;x<W;x++) d[x]=(x<lo||x>hi)?0:(x<il||x>ih)?rim[x]:s_bg;
    span((uint8_t*)d,m_old+o,xo,sl,sh,il,ih); span((uint8_t*)d,m_new+o,xn,sl,sh,il,ih); } }
// ---- style 3: push + soft edges. Same layout as push, but content fades into the page fill near the rim and at the
// moving page edges (FEATHER px), and the outgoing page fades out while the incoming one fades in.
#define FEATHER 40
static inline uint16_t mix(uint16_t c,uint16_t b,int a){ if(a>=32) return c; if(a<=0) return b;   // a/32 of c over b
  int r=((c>>11)*a+(b>>11)*(32-a))>>5, g=(((c>>5)&63)*a+((b>>5)&63)*(32-a))>>5, l=((c&31)*a+(b&31)*(32-a))>>5; return (uint16_t)(r<<11|g<<5|l); }
static void compose_soft(uint8_t* dst,int y0,int n,float q){ compose_push(dst,y0,n,q); if(q>=1.f) return;
  int xn=s_dir*(int)(W*(1.f-q)+0.5f), xo=xn-s_dir*W;
  int go=(int)(32*(1.f-q*1.4f)), gn=(int)(32*(0.3f+q*0.9f)); if(go<0) go=0; if(gn>32) gn=32;   // old gone by ~70 %, new full by ~80 %
  for(int r=0;r<n;r++){ int y=y0+r, lo=s_lo[y], hi=W-1-lo, sl=s_in[y], sh=W-1-sl; if(sh<sl) continue;
    int il=sl>lo?sl:lo, ih=sh<hi?sh:hi; uint16_t* d=(uint16_t*)(dst+(size_t)r*W*2);
    for(int x=il;x<=ih;x++){ int so=x-xo, sn=x-xn, g, e;
      if(so>=sl&&so<=sh){ g=go; e=so-sl<sh-so?so-sl:sh-so; } else if(sn>=sl&&sn<=sh){ g=gn; e=sn-sl<sh-sn?sn-sl:sh-sn; } else continue;
      int ed=x-il<ih-x?x-il:ih-x; if(ed<e) e=ed; int a=e>=FEATHER?g:g*e/FEATHER;
      if(a<32) d[x]=mix(d[x],s_bg,a); } } }
static int64_t hnow(void){ return hw->now?hw->now():us(); }
static int64_t s_tc, s_tw; static int s_fr, s_mx, s_i; static int64_t s_tf;
// push one full frame at progress q (0 = exactly the old page, 1 = exactly the new page)
static void push_frame(float q){ int v=(int)(q*1000+0.5f), x_new=s_dir*(W-(W*v)/1000), x_old=-s_dir*(GW_SLIDE_PARALLAX*v)/1000;
  if(q<=0.f){ x_new=s_dir*W; x_old=0; }
  for(int y=0;y<W;y+=hw->rows,s_i++){ int n=W-y<hw->rows?W-y:hw->rows; uint8_t* b=hw->buf(s_i&1);
    int64_t a=hnow(); if(q<=0.f) compose(b,y,n,x_new,x_old); else if(s_style==3) compose_soft(b,y,n,q); else if(s_style==2) compose_push(b,y,n,q); else if(s_style) compose_style(b,y,n,q); else compose(b,y,n,x_new,x_old);
    int64_t c=hnow(); hw->wait(); s_tc+=c-a; s_tw+=hnow()-c; hw->push(y,n,b); }   // compose overlaps the previous strip's transfer
  if(hw->frame_done) hw->frame_done();
  int64_t t=hnow(); int fm=(int)(t-s_tf); if(fm>s_mx) s_mx=fm; s_tf=t; s_fr++; }
static int64_t s_t0;
static void stats_begin(void){ s_t0=s_tf=hnow(); s_tc=s_tw=0; s_fr=s_mx=0; hw->wait(); }
static void stats_end(void){ hw->wait(); st_comp_ms=(int)(s_tc/1000); st_wait_ms=(int)(s_tw/1000); st_frames=s_fr; st_ms=(int)((hnow()-s_t0)/1000); st_max_us=s_mx; }
// animate q0 -> q1 over dur ms with curve 0 = smoothstep, 1 = cubic ease-out (starts at the finger's speed)
static void animate(float q0,float q1,int dur,int curve){ int64_t t0=hnow();
  for(;;){ int el=(int)((hnow()-t0)/1000); bool last=el>=dur; float p=last?1.f:(float)el/dur, k;
    if(curve){ float u=1.f-p; k=1.f-u*u*u; } else k=p*p*(3.f-2.f*p);   // smoothstep: at ~34 fps a cubic ease-out from rest jumped 126 px in frame 1
    push_frame(last?q1:q0+(q1-q0)*k); if(last) break; } }
static void run_direct(void){ if(s_style>=2) s_bg=pick_bg(); stats_begin(); animate(0.f,1.f,GW_SLIDE_DUR,0); stats_end(); }
// the screen already shows the final page: repaint only the top layer (page dots), which the snapshot leaves out
static void land(void){ lv_obj_t* top=lv_layer_top();
  for(uint32_t i=0;i<lv_obj_get_child_count(top);i++){ lv_obj_invalidate(lv_obj_get_child(top,i)); } }
bool gw_slide_start(int dir){ if(!have_old) return false; have_old=false; s_dir=dir<0?-1:1;
  if(!snap(&sb_new,m_new,&snap_us[1])) return false;
  if(hw){ run_direct();
    // the last pushed frame already is the new page: drop the full-screen redraw render() queued (90-200 ms on device)
    // and repaint only the top layer (page dots), which the screen snapshot leaves out
    if(s_style!=1){ lv_display_get_default()->inv_p=0; land(); }   // cover leaves old-page pixels outside the card
    if(done_cb) done_cb();
    return true; }
  ov=lv_obj_create(lv_layer_top()); lv_obj_remove_style_all(ov); lv_obj_set_size(ov,W,W); lv_obj_set_pos(ov,0,0);
  lv_obj_set_style_bg_color(ov,lv_color_black(),0); lv_obj_set_style_bg_opa(ov,LV_OPA_COVER,0);   // opaque: nothing under it is redrawn
  lv_obj_remove_flag(ov,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
  // raw RGB565 in RAM: the built-in decoder reads it in place (no decoded copy to invalidate)
  im_old=lv_image_create(ov); lv_image_set_src(im_old,&sb_old); lv_obj_set_pos(im_old,0,0);
  im_new=lv_image_create(ov); lv_image_set_src(im_new,&sb_new); lv_obj_set_pos(im_new,s_dir*W,0);
  lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a,ov); lv_anim_set_exec_cb(&a,ax); lv_anim_set_values(&a,0,1000);
  lv_anim_set_duration(&a,GW_SLIDE_DUR); lv_anim_set_path_cb(&a,lv_anim_path_ease_out); lv_anim_set_completed_cb(&a,anim_done); lv_anim_start(&a);
  return true; }

// ---- 2.6.32 finger-following slide (direct mode only). The caller switches to the target page and renders it, then
// drag_begin() snapshots it; drag_to() pushes one frame per touch sample; drag_end() settles with the finger's speed.
// LVGL's refresh timer is paused meanwhile, so nothing it draws lands on top of the drag frames.
static bool s_drag; static float s_q;
bool gw_slide_can_drag(void){ return hw&&have_old&&!s_drag; }
bool gw_slide_dragging(void){ return s_drag; }
bool gw_slide_drag_begin(int dir){ if(!gw_slide_can_drag()) return false; have_old=false; s_dir=dir<0?-1:1;
  if(!snap(&sb_new,m_new,&snap_us[1])) return false;
  lv_display_t* d=lv_display_get_default(); d->inv_p=0;   // the render that preceded is in the snapshot; later changes still get drawn at landing
  lv_timer_pause(lv_display_get_refr_timer(d)); if(s_style>=2) s_bg=pick_bg();
  s_drag=true; s_q=0; stats_begin(); return true; }
void gw_slide_drag_to(int dx){ if(!s_drag) return; float q=(float)(-s_dir*dx)/W; s_q=q<0?0:q>1?1:q; push_frame(s_q); }
bool gw_slide_drag_end(float vx){ if(!s_drag) return false;
  float vq=-s_dir*vx/W;   // progress per ms
  bool fling=fabsf(vx)>0.25f, commit=fling?vq>0:s_q>0.4f; float q1=commit?1.f:0.f, dq=q1-s_q;
  int dur=(int)(120+300*fabsf(dq)); if(fling&&(vq>0)==(dq>0)&&fabsf(vq)>1e-4f){ dur=(int)(3*fabsf(dq)/fabsf(vq)); }   // cubic ease-out starts at slope 3*dq/dur = finger speed
  if(dur<160) dur=160;
  if(dur>420) dur=420;
  if(dq!=0.f) animate(s_q,q1,dur,1); else push_frame(q1);
  stats_end(); s_drag=false; lv_timer_resume(lv_display_get_refr_timer(lv_display_get_default()));
  if(commit) land();
  if(done_cb) done_cb();
  return commit; }
void gw_slide_drag_back_done(void){ lv_display_get_default()->inv_p=0; land(); }   // after the caller re-rendered the old page
