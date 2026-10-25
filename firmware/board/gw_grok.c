#pragma GCC diagnostic ignored "-Wmisleading-indentation"
// Grok voice-page: aora-bot expression contours adapted to LVGL 9.
// Expression data/animation parameters: Copyright 2026 sam70361.
// LicenseRef-Emotion-Ball-Community; see third_party/aora/LICENSE and NOTICE.md.
// States: 0 idle, 1 listening, 2 thinking, 3 speaking, 4 offline.
#include "gw_faces.h"
#include "gw_aora_data.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif
const lv_font_t* gw_f20(void); const lv_font_t* gw_f21(void); const lv_font_t* gw_f_status(void);
#define F20 gw_f20()
#define F21 gw_f21()
// Original GrokWatch character skin (not the upstream emotion-ball visual): teal page, mint body built
// from two flat tonal discs (shade + face) for depth without RGB565 gradient bands. Night = dimmed warm white.
typedef struct { uint32_t bg, shade, body, off_shade, off_body, ink, accent, sec, reply; } gw_skin_t;
static const gw_skin_t SKIN[2]={
  {0x213F3B,0x86C7B1,0xC6EBDC,0x6F9488,0x93B3A9,0x14302B,0x8FE3C8,0x9DB8B0,0xF4F6F2},   // day
  {0x100D0A,0x4F6E64,0x7E9F93,0x3E4F49,0x56685F,0x0C1311,0x9DB8AC,0x7C8A84,0xC9C2B4}}; // night: dimmed mint body, warm-white text
static bool g_night;
void gw_dial_finish(lv_layer_t* L,const lv_area_t* a,bool night);
static void g_finish(lv_event_t* e){ lv_area_t a; lv_obj_get_coords(lv_event_get_target_obj(e),&a);
  const lv_area_t* c=&lv_event_get_layer(e)->_clip_area; int cx=(a.x1+a.x2)/2, cy=(a.y1+a.y2)/2;
  int dx=LV_MAX(LV_ABS(c->x1-cx),LV_ABS(c->x2-cx)), dy=LV_MAX(LV_ABS(c->y1-cy),LV_ABS(c->y2-cy));
  if(dx*dx+dy*dy>=200*200) gw_dial_finish(lv_event_get_layer(e),&a,g_night); }
#define SK (&SKIN[g_night?1:0])
static lv_obj_t *g_root,*g_avatar,*g_bar[7],*g_dot[3],*g_stat,*g_sub,*g_user,*g_reply;
static int g_st=0, g_lvl=0;
static lv_timer_t* g_tm;
static uint32_t g_now, g_pool_at, g_morph_at;
static uint8_t g_pool_pos;
#include "gw_mem.h"
#define GEYE_SZ (sizeof(float)*2*48*2)
static float (*g_points)[48][2], (*g_from)[48][2], (*g_to)[48][2];   // PSRAM (2.3 KB moved out of internal DRAM)
static void eyes_alloc(void){ if(!g_points){ g_points=gw_psram_calloc(GEYE_SZ); g_from=gw_psram_calloc(GEYE_SZ); g_to=gw_psram_calloc(GEYE_SZ); } }
static void apply_skin(void);
static void g_fade(lv_obj_t* o,uint32_t ms);
static void g_show(lv_obj_t* o,bool on);
#define GW_SUB_W 260   // subtitle width: inside the 466 px round screen's chord at the subtitle rows, 18 px margin
// 2.6.24: no words on this page at all. State is carried by the face (listening lean + "o", thinking gaze + halo, speaking mouth).
#define GFY 4    // face block centre offset: lit disc sits optically centred on the 466 round screen
static lv_obj_t* rect(lv_obj_t* p,int w,int h,int r,uint32_t c){ lv_obj_t* o=lv_obj_create(p); lv_obj_remove_style_all(o); lv_obj_set_size(o,w,h);
  lv_obj_set_style_radius(o,r,0); lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0); lv_obj_set_style_bg_color(o,lv_color_hex(c),0);
  lv_obj_remove_flag(o,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE); return o; }
static float wave(uint32_t ms,float period){ return sinf((float)(ms%(uint32_t)period)*6.2831853f/period); }
static float glance(uint32_t ms,uint32_t period){
  float p=(float)(ms%period)/period, v;
  if(p<.1f){ v=p/.1f; return v*v*(3-2*v); }
  if(p<.45f) return 1;
  if(p<.55f){ v=(p-.45f)/.1f; return 1-2*v*v*(3-2*v); }
  if(p<.9f) return -1;
  v=(p-.9f)/.1f; return -1+v*v*(3-2*v);
}
static void target_eye(bool immediate){ eyes_alloc(); if(!g_points) return;
  memcpy(g_from,g_points,GEYE_SZ);
  int ring=gw_aora_cfg[g_st].pool[g_pool_pos];
  for(int e=0;e<2;e++) for(int i=0;i<48;i++) for(int xy=0;xy<2;xy++)
    g_to[e][i][xy]=gw_aora_eyes[ring][e][i][xy]*.01f;
  if(immediate) memcpy(g_points,g_to,GEYE_SZ);
  g_morph_at=g_now;
}
// Both eyes are rasterized as whole polygons into one A8 coverage mask, then blended once.
// One fill per contour means no internal antialias seams (the old triangle strip had 46).
enum { AV=220, SUB=4 };
static uint8_t* g_mask;           // AV*AV A8, allocated once (PSRAM on device), never per frame
static lv_image_dsc_t g_mask_img;
static float g_cov[AV+2];
static void raster_eye(const float (*pt)[2], int n, int y0, int y1){
  for(int py=y0; py<=y1; py++){
    memset(g_cov,0,sizeof g_cov);
    for(int s=0;s<SUB;s++){
      float sy=py+(s+.5f)/SUB, xs[16]; int k=0;
      for(int i=0,j=n-1;i<n;j=i++){
        float ya=pt[i][1], yb=pt[j][1];
        if((ya<=sy)!=(yb<=sy) && k<16) xs[k++]=pt[i][0]+(sy-ya)*(pt[j][0]-pt[i][0])/(yb-ya);
      }
      for(int i=1;i<k;i++){ float v=xs[i]; int j=i-1; while(j>=0&&xs[j]>v){ xs[j+1]=xs[j]; j--; } xs[j+1]=v; }
      for(int i=0;i+1<k;i+=2){
        float xa=xs[i]<0?0:xs[i], xb=xs[i+1]>AV?AV:xs[i+1]; if(xb<=xa) continue;
        int ia=(int)xa, ib=(int)xb;
        if(ia==ib){ g_cov[ia]+=(xb-xa)/SUB; continue; }
        g_cov[ia]+=(ia+1-xa)/SUB; for(int x=ia+1;x<ib;x++) g_cov[x]+=1.f/SUB; if(ib<AV) g_cov[ib]+=(xb-ib)/SUB;
      }
    }
    uint8_t* row=g_mask+py*AV;
    for(int x=0;x<AV;x++){ float c=g_cov[x]; int v=(int)(c*255.f+.5f); v=v>255?255:v; if(v>row[x]) row[x]=(uint8_t)v; }
  }
}
// Face v2 (Sunny 10/25: no long slit eyes). Parametric and cheap: a few rounded rects + arcs per frame, no mask.
// Variant 0: soft vertical oval eyes + highlight; 1: round dot eyes + highlight; 2: oval eyes + blush cheeks.
static int g_var=2;
void gw_grok_variant(int v){ g_var=v<0?0:(v>2?2:v); if(g_avatar) lv_obj_invalidate(g_avatar); }
static void fill_rr(lv_layer_t* L,lv_color_t c,lv_opa_t o,int cx,int cy,int w,int h){ lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
  d.radius=LV_RADIUS_CIRCLE; d.bg_color=c; d.bg_opa=o; lv_area_t a={cx-w/2,cy-h/2,cx-w/2+w-1,cy-h/2+h-1}; lv_draw_rect(L,&d,&a); }
static void arc_(lv_layer_t* L,lv_color_t c,int w,int cx,int cy,int r,int a0,int a1){ lv_draw_arc_dsc_t a; lv_draw_arc_dsc_init(&a);
  a.color=c; a.width=w; a.rounded=1; a.radius=r; a.center.x=cx; a.center.y=cy; a.start_angle=a0; a.end_angle=a1; lv_draw_arc(L,&a); }
static float ease(float x){ x=x<0?0:(x>1?1:x); return x*x*(3-2*x); }
static void draw_avatar(lv_event_t* event){
  lv_layer_t* L=lv_event_get_layer(event);
  lv_area_t b; lv_obj_get_coords(g_avatar,&b);
  bool off=g_st==4;
  float lean=g_st==1?3.f:0;                                  // listening: lean in a touch
  float bob=g_st==3?1.6f*wave(g_now,1400):0;
  int cx=b.x1+110, cy=b.y1+110+(int)(lean+bob);
  int R=98+(g_st==1?2:0);
  lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d); d.radius=LV_RADIUS_CIRCLE; d.bg_opa=LV_OPA_COVER;
  d.bg_color=lv_color_hex(off?SK->off_shade:SK->shade); lv_area_t body={cx-R,cy-R,cx+R,cy+R}; lv_draw_rect(L,&d,&body);
  d.bg_color=lv_color_hex(off?SK->off_body:SK->body); lv_area_t face={cx-R+2,cy-R+1,cx+R-8,cy+R-9}; lv_draw_rect(L,&d,&face);
  lv_color_t ink=lv_color_hex(SK->ink), hi=lv_color_hex(g_night?0xBFD3CB:0xFFFFFF);
  int fx=cx-3, fy=cy-5;                                      // optical centre of the lit face disc
  // gaze
  float gx=0, gy=0;
  if(g_st==0){ float p=(float)(g_now%7000)/7000; gx=p<.5f?5*ease((p-.40f)/.06f):5-5*ease((p-.90f)/.06f); }
  if(g_st==2){ gx=12; gy=-11; }
  // eye size per variant/state
  // 2.6.11: every variant and state uses round eyes (Sunny: no long/slit eyes). Never taller or wider than 1.2:1.
  static float es=30; float et=g_st==1?33.f:30.f; es+=(et-es)*.35f; if(fabsf(et-es)<.2f) es=et;   // ~200 ms ease-out between states at 33 ms ticks
  int ew=(int)(es+.5f), eh=ew;
  int sep=32, ey=fy-6;
  uint32_t bk=g_now%4300; float open=bk<70?1-bk/70.f:(bk<160?(bk-70)/90.f:1); if(open<.12f) open=.12f;
  if(g_st==2) open=open<.85f?open:.85f;
  if(g_var==2&&!off&&!g_night){ lv_color_t blush=lv_color_mix(lv_color_hex(0xF4A9A0),lv_color_hex(SK->body),g_night?70:110);
    fill_rr(L,blush,LV_OPA_COVER,fx-sep-20,ey+30,26,14); fill_rr(L,blush,LV_OPA_COVER,fx+sep+20,ey+30,26,14); }
  for(int k=-1;k<=1;k+=2){ int ex=fx+k*sep+(int)gx, eyy=ey+(int)gy;
    if(off){ arc_(L,ink,6,ex,eyy-6,11,20,160); continue; }   // asleep: soft closed lids
    if(open<.55f){ arc_(L,ink,6,ex,eyy-6,11,20,160); continue; }   // blink: soft closed lid, never a flat bar
    int h=(int)(eh*open); if(h<ew*5/6) h=ew*5/6; fill_rr(L,ink,LV_OPA_COVER,ex,eyy,ew,h);
    if(open>.6f){ int hs=g_var==1?8:7; fill_rr(L,hi,LV_OPA_COVER,ex-ew/2+hs/2+4,eyy-h/2+hs/2+5,hs,hs); } }
  // mouth
  int mx=fx+(int)(gx*.5f), my=ey+36+(int)(gy*.3f);
  if(g_st==0) arc_(L,ink,5,mx,my-6,11,25,155);                       // small smile
  else if(g_st==1) fill_rr(L,ink,LV_OPA_COVER,mx,my+1,13,11);          // little "o"
  else if(g_st==2) fill_rr(L,ink,LV_OPA_COVER,mx+8,my+2,14,5);       // small flat "hmm", off to the side
  else if(g_st==3){ float t=wave(g_now,240), u=wave(g_now+70,390); float ht=5+7*(t>0?t:-t)+4*(u>0?u:-u); static float mh=5; mh+=(ht-mh)*.45f;   // 2.6.24: eased, no frame-to-frame snaps
    fill_rr(L,ink,LV_OPA_COVER,mx,my+2,20,(int)(mh+.5f)); }
  else arc_(L,ink,4,mx,my-4,8,30,150);                                 // asleep: tiny relaxed smile
}
// Redraw the 220x220 face only while something on it actually moves (device: ~70 ms per full face frame).
static bool face_moving(void){ if(g_st==1||g_st==3) return true; if(g_now-g_morph_at<400) return true;
  uint32_t bk=g_now%4300; if(bk<200) return true; if(g_st==0){ uint32_t p=g_now%7000; if((p>2750&&p<3300)||(p>6250&&p<6800)) return true; } return false; }
static bool g_was_moving=true;
bool gw_slide_active(void);
static void tick(lv_timer_t* t){ (void)t; if(!g_root||lv_obj_has_flag(g_root,LV_OBJ_FLAG_HIDDEN)) return;
  if(gw_slide_active()) return;   // 2.6.24: page bitmaps are sliding over us; do not redraw a face nobody sees
  g_now=lv_tick_get(); const gw_aora_cfg_t* cfg=&gw_aora_cfg[g_st];
  if(g_now-g_pool_at>=cfg->pool_ms){ g_pool_at=g_now; g_pool_pos=(g_pool_pos+1)%cfg->count; target_eye(false); }
  float f=(float)(g_now-g_morph_at)/cfg->transition_ms; if(f>1) f=1; f=f*f*(3-2*f);
  for(int e=0;e<2;e++) for(int i=0;i<48;i++) for(int xy=0;xy<2;xy++)
    g_points[e][i][xy]=g_from[e][i][xy]+(g_to[e][i][xy]-g_from[e][i][xy])*f;
  { bool mv=face_moving(); if(mv||g_was_moving) lv_obj_invalidate(g_avatar); g_was_moving=mv; } // avatar area only, and only while it moves
  if(g_st==1) for(int i=0;i<7;i++){
    int h=5+(int)((1+wave(g_now+i*67,400))*g_lvl*(4-abs(i-3))/40);
    if(h>24) h=24; lv_obj_set_height(g_bar[i],h); lv_obj_align(g_bar[i],LV_ALIGN_CENTER,(i-3)*14,GFY+136);
  }
  if(g_st==2) for(int i=0;i<3;i++){
    int a=(g_now/12+i*35)%360;
    lv_obj_align(g_dot[i],LV_ALIGN_CENTER,(96*lv_trigo_cos(a))>>15,GFY-128+((14*lv_trigo_sin(a))>>15));
  }
}
void gw_grok_create(lv_obj_t* parent){
  g_root=lv_obj_create(parent); lv_obj_remove_style_all(g_root); lv_obj_set_size(g_root,466,466); lv_obj_set_style_bg_opa(g_root,LV_OPA_COVER,0); lv_obj_set_style_radius(g_root,LV_RADIUS_CIRCLE,0); lv_obj_center(g_root); lv_obj_remove_flag(g_root,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
  g_st=0; g_lvl=0; g_now=lv_tick_get(); g_pool_at=g_now; g_pool_pos=0; target_eye(true);
  if(!g_mask){
#ifdef ESP_PLATFORM
    g_mask=heap_caps_malloc(AV*AV,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
    g_mask=malloc(AV*AV);
#endif
    if(g_mask){ memset(g_mask,0,AV*AV); g_mask_img.header.magic=LV_IMAGE_HEADER_MAGIC; g_mask_img.header.cf=LV_COLOR_FORMAT_A8;
      g_mask_img.header.w=AV; g_mask_img.header.stride=AV; }
  }
  g_avatar=lv_obj_create(g_root); lv_obj_remove_style_all(g_avatar); lv_obj_set_size(g_avatar,220,220);
  lv_obj_align(g_avatar,LV_ALIGN_CENTER,3,GFY); lv_obj_remove_flag(g_avatar,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(g_avatar,draw_avatar,LV_EVENT_DRAW_MAIN,NULL);
  lv_obj_add_event_cb(g_root,g_finish,LV_EVENT_DRAW_MAIN_END,NULL);
  for(int i=0;i<7;i++){ g_bar[i]=rect(g_root,7,5,4,SKIN[0].accent); lv_obj_add_flag(g_bar[i],LV_OBJ_FLAG_HIDDEN); }
  for(int i=0;i<3;i++){ g_dot[i]=rect(g_root,7,7,4,i==0?0x9A72EE:(i==1?0x5B95F0:0x35C3BD)); lv_obj_add_flag(g_dot[i],LV_OBJ_FLAG_HIDDEN); }
  g_stat=lv_label_create(g_root); lv_obj_set_style_text_font(g_stat,gw_f_status(),0); lv_obj_set_style_text_color(g_stat,lv_color_hex(SKIN[0].accent),0); lv_obj_align(g_stat,LV_ALIGN_CENTER,0,-152);
  // Subtitles: the question is one fixed line (ellipsis); the reply lives in its own 2-line window below it and
  // scrolls so the newest text shows. A long reply can never climb over the question or the character.
  int lh=lv_font_get_line_height(F20);
  g_user=lv_label_create(g_root); lv_obj_set_style_text_font(g_user,F20,0); lv_obj_set_width(g_user,GW_SUB_W); lv_obj_set_style_text_opa(g_user,200,0); lv_obj_set_height(g_user,lh);
  lv_label_set_long_mode(g_user,LV_LABEL_LONG_DOT); lv_obj_set_style_text_align(g_user,LV_TEXT_ALIGN_CENTER,0); lv_obj_align(g_user,LV_ALIGN_CENTER,0,92);
  g_sub=lv_obj_create(g_root); lv_obj_remove_style_all(g_sub); lv_obj_set_size(g_sub,GW_SUB_W,lh*2+8); lv_obj_align_to(g_sub,g_user,LV_ALIGN_OUT_BOTTOM_MID,0,8);
  lv_obj_set_scroll_dir(g_sub,LV_DIR_VER); lv_obj_set_scrollbar_mode(g_sub,LV_SCROLLBAR_MODE_OFF); lv_obj_remove_flag(g_sub,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLL_ELASTIC);
  g_reply=lv_label_create(g_sub); lv_obj_set_style_text_font(g_reply,F20,0); lv_obj_set_width(g_reply,GW_SUB_W); lv_obj_set_style_text_line_space(g_reply,4,0);
  lv_label_set_long_mode(g_reply,LV_LABEL_LONG_WRAP); lv_obj_set_style_text_align(g_reply,LV_TEXT_ALIGN_CENTER,0);
  lv_label_set_text(g_user,""); lv_label_set_text(g_reply,""); lv_label_set_text(g_stat,"");
  lv_obj_add_flag(g_user,LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(g_sub,LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(g_stat,LV_OBJ_FLAG_HIDDEN);   // 2.6.24: audio-only page
  apply_skin();
  lv_obj_add_flag(g_root,LV_OBJ_FLAG_HIDDEN);
  if(!g_tm) g_tm=lv_timer_create(tick,33,NULL); lv_timer_pause(g_tm);
}
void gw_grok_visible(bool on){ if(!g_root) return; on?lv_obj_remove_flag(g_root,LV_OBJ_FLAG_HIDDEN):lv_obj_add_flag(g_root,LV_OBJ_FLAG_HIDDEN);
  if(g_tm){ on?lv_timer_resume(g_tm):lv_timer_pause(g_tm); } }  // zero cost while hidden
void gw_grok_state(int st){ if(st<0||st>4) return;
  if(st!=g_st){ g_st=st; g_now=lv_tick_get(); g_pool_at=g_now; g_pool_pos=0; target_eye(false); }
  if(g_root){
    for(int i=0;i<7;i++) g_show(g_bar[i],st==1);    // 2.6.24: 180 ms ease-out in, 140 ms out (no pop)
    for(int i=0;i<3;i++) g_show(g_dot[i],st==2);
  } }
void gw_grok_level(int l){ g_lvl=l<0?0:(l>100?100:l); }
// 2.6.12: g_stat holds the *theme* font directly (not the f20_rt copy), so it must be re-pointed when the theme
// font is swapped, before the old one is freed (same UAF as 2.6.4; suspected cause of the 2.6.11 first-boot abort).
void gw_grok_font_changed(const lv_font_t* f){ if(g_stat) lv_obj_set_style_text_font(g_stat,f?f:gw_f21(),0); }
void gw_grok_status(const char* s){ (void)s; }   // 2.6.24: no status text
static void gx_opa(void* o,int32_t v){ lv_obj_set_style_opa((lv_obj_t*)o,(lv_opa_t)v,0); }
static void g_fade(lv_obj_t* o,uint32_t ms){ lv_anim_delete(o,gx_opa); lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a,o); lv_anim_set_exec_cb(&a,gx_opa);
  lv_anim_set_values(&a,60,LV_OPA_COVER); lv_anim_set_duration(&a,ms); lv_anim_set_path_cb(&a,lv_anim_path_ease_out); lv_anim_start(&a); }
static void g_hide_done(lv_anim_t* a){ lv_obj_add_flag((lv_obj_t*)a->var,LV_OBJ_FLAG_HIDDEN); }
static void g_show(lv_obj_t* o,bool on){ bool vis=!lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN);
  if(on){ if(vis&&!lv_anim_get(o,gx_opa)) return; lv_obj_remove_flag(o,LV_OBJ_FLAG_HIDDEN); lv_anim_delete(o,gx_opa); lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a,o);
    lv_anim_set_exec_cb(&a,gx_opa); lv_anim_set_values(&a,lv_obj_get_style_opa(o,0)<LV_OPA_COVER?lv_obj_get_style_opa(o,0):0,LV_OPA_COVER); lv_anim_set_duration(&a,180); lv_anim_set_path_cb(&a,lv_anim_path_ease_out); lv_anim_start(&a); }
  else { if(!vis) return; lv_anim_delete(o,gx_opa); lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a,o); lv_anim_set_exec_cb(&a,gx_opa);
    lv_anim_set_values(&a,lv_obj_get_style_opa(o,0),0); lv_anim_set_duration(&a,140); lv_anim_set_path_cb(&a,lv_anim_path_ease_in); lv_anim_set_completed_cb(&a,g_hide_done); lv_anim_start(&a); } }
// 2.6.24: replies are spoken only; the page shows no subtitles. Kept as a no-op so the board API is unchanged.
void gw_grok_text(const char* role,const char* t){ (void)role; (void)t; }
static void apply_skin(void){ if(!g_root) return;
  lv_obj_set_style_bg_color(g_root,lv_color_hex(SK->bg),0);
  for(int i=0;i<7;i++) lv_obj_set_style_bg_color(g_bar[i],lv_color_hex(SK->accent),0);
  static const uint8_t mixv[3]={255,215,175};   // thinking dots: one hue, three tones (no rainbow)
  for(int i=0;i<3;i++) lv_obj_set_style_bg_color(g_dot[i],lv_color_mix(lv_color_hex(SK->accent),lv_color_hex(SK->bg),mixv[i]),0);
  lv_obj_set_style_text_color(g_stat,lv_color_hex(SK->accent),0);
  lv_obj_set_style_text_color(g_user,lv_color_hex(SK->sec),0); lv_obj_set_style_text_color(g_reply,lv_color_hex(SK->reply),0);
  lv_obj_invalidate(g_root); }
void gw_grok_set_night(bool on){ g_night=on; apply_skin(); }
