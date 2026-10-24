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
const lv_font_t* gw_f20(void); const lv_font_t* gw_f21(void);
#define F20 gw_f20()
#define F21 gw_f21()
// Original GrokWatch character skin (not the upstream emotion-ball visual): teal page, mint body built
// from two flat tonal discs (shade + face) for depth without RGB565 gradient bands. Night = dimmed warm white.
typedef struct { uint32_t bg, shade, body, off_shade, off_body, ink, accent, sec, reply; } gw_skin_t;
static const gw_skin_t SKIN[2]={
  {0x213F3B,0x86C7B1,0xC6EBDC,0x6F9488,0x93B3A9,0x14302B,0x8FE3C8,0x9DB8B0,0xF4F6F2},   // day
  {0x100D0A,0x5E5446,0x9C8C76,0x4A4238,0x6E6253,0x100D0A,0xB8A68C,0x7A6E60,0xC9BBA5}}; // night
static bool g_night;
#define SK (&SKIN[g_night?1:0])
static lv_obj_t *g_root,*g_avatar,*g_bar[7],*g_dot[3],*g_stat,*g_sub,*g_user,*g_reply;
static int g_st=0, g_lvl=0;
static lv_timer_t* g_tm;
static uint32_t g_now, g_pool_at, g_morph_at;
static uint8_t g_pool_pos;
static float g_points[2][48][2], g_from[2][48][2], g_to[2][48][2];
static void apply_skin(void);
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
static void target_eye(bool immediate){
  memcpy(g_from,g_points,sizeof g_from);
  int ring=gw_aora_cfg[g_st].pool[g_pool_pos];
  for(int e=0;e<2;e++) for(int i=0;i<48;i++) for(int xy=0;xy<2;xy++)
    g_to[e][i][xy]=gw_aora_eyes[ring][e][i][xy]*.01f;
  if(immediate) memcpy(g_points,g_to,sizeof g_points);
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
static void draw_avatar(lv_event_t* event){
  lv_layer_t* layer=lv_event_get_layer(event);
  lv_area_t bounds; lv_obj_get_coords(g_avatar,&bounds);
  float breathe=1.0f+.010f*wave(g_now,3600);
  float bob=g_st==3?2.2f*wave(g_now,1400):(g_st==4?4.0f:0);
  float cx=110, cy=110+bob;
  int radius=(int)(100*breathe+.5f);
  lv_area_t body={bounds.x1+(int32_t)cx-radius,bounds.y1+(int32_t)cy-radius,bounds.x1+(int32_t)cx+radius,bounds.y1+(int32_t)cy+radius};
  // Two flat tonal discs: a shade disc and the face disc nudged up-left -> a soft crescent of depth
  // on the lower right. Solid fills only (no gradient), so RGB565 shows no banding.
  bool off=g_st==4;
  lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d); d.radius=LV_RADIUS_CIRCLE; d.bg_opa=LV_OPA_COVER;
  d.bg_color=lv_color_hex(off?SK->off_shade:SK->shade); lv_draw_rect(layer,&d,&body);
  lv_area_t face={body.x1+2,body.y1+1,body.x2-9,body.y2-10};
  d.bg_color=lv_color_hex(off?SK->off_body:SK->body); lv_draw_rect(layer,&d,&face);
  if(!g_mask) return;
  float dx=0,dy=0,eye_scale=g_st==4?.88f:1;
  if(g_st==0){ dx=10*glance(g_now,4800); dy=2*wave(g_now+718,4100); }
  if(g_st==1) dy=6*wave(g_now,2200);
  if(g_st==2){ dx=9*wave(g_now,2600); dy=-8; }
  if(g_st==3) dy=-3+6*glance(g_now,3000);
  if(g_st==4) dy=12+1.6f*wave(g_now,3600);
  uint32_t blink=g_now%gw_aora_cfg[g_st].blink_ms;
  float open=blink<90?1-.94f*blink/90:(blink<200?.06f+.94f*(blink-90)/110:1);
  static float pts[2][48][2]; float minx=AV,maxx=0,miny=AV,maxy=0;
  for(int e=0;e<2;e++){
    float mid=0; for(int i=0;i<48;i++) mid+=g_points[e][i][1]/48;
    for(int i=0;i<48;i++){
      float x=cx+(g_points[e][i][0]*eye_scale+dx)*.85f*breathe;
      float y=cy+(mid+(g_points[e][i][1]-mid)*open*eye_scale+dy)*.85f*breathe;
      x=x<0?0:(x>AV-1?AV-1:x); y=y<0?0:(y>AV-1?AV-1:y);
      pts[e][i][0]=x; pts[e][i][1]=y;
      if(x<minx)minx=x; if(x>maxx)maxx=x; if(y<miny)miny=y; if(y>maxy)maxy=y;
    }
  }
  int x0=(int)minx, x1=(int)maxx+1, y0=(int)miny, y1=(int)maxy+1; if(x1>AV-1)x1=AV-1; if(y1>AV-1)y1=AV-1;
  for(int y=y0;y<=y1;y++) memset(g_mask+y*AV,0,AV);
  for(int e=0;e<2;e++) raster_eye((const float(*)[2])pts[e],48,y0,y1);
  lv_draw_image_dsc_t img; lv_draw_image_dsc_init(&img);
  g_mask_img.data=g_mask+y0*AV; g_mask_img.header.h=(uint32_t)(y1-y0+1); g_mask_img.data_size=AV*g_mask_img.header.h;
  img.src=&g_mask_img; img.recolor=lv_color_hex(SK->ink); img.recolor_opa=LV_OPA_COVER;
  lv_area_t ia={bounds.x1,bounds.y1+y0,bounds.x1+AV-1,bounds.y1+y1};
  (void)x0; lv_draw_image(layer,&img,&ia);
  // Mouth: GrokWatch's own five shapes, one small primitive each (arc / ring / line / pill).
  // Anchored under the eyes so no pose can make them collide; kept inside the lower face.
  float myf=maxy+(g_st==3?14:20); float lim=cy+radius-30; if(myf>lim) myf=lim;
  int mx=bounds.x1+(int)((minx+maxx)*.5f), my=bounds.y1+(int)myf;
  lv_color_t ink=lv_color_hex(SK->ink);
  if(g_st==0||g_st==4){ lv_draw_arc_dsc_t a; lv_draw_arc_dsc_init(&a); a.color=ink; a.width=7; a.rounded=1; a.radius=19;
    a.center.x=mx; a.center.y=g_st==0?my-15:my+15; a.start_angle=g_st==0?40:220; a.end_angle=g_st==0?140:320; lv_draw_arc(layer,&a); }
  else if(g_st==1){ lv_draw_arc_dsc_t a; lv_draw_arc_dsc_init(&a); a.color=ink; a.width=6; a.radius=9;
    a.center.x=mx; a.center.y=my; a.start_angle=0; a.end_angle=360; lv_draw_arc(layer,&a); }
  else if(g_st==2){ lv_draw_line_dsc_t l; lv_draw_line_dsc_init(&l); l.color=ink; l.width=6; l.round_start=1; l.round_end=1;
    l.p1.x=mx-6; l.p1.y=my+1; l.p2.x=mx+14; l.p2.y=my-2; lv_draw_line(layer,&l); }
  else { float t=wave(g_now,260), u=wave(g_now+90,410); int h=6+(int)(5*(t>0?t:-t)+3*(u>0?u:-u));
    lv_draw_rect_dsc_t m; lv_draw_rect_dsc_init(&m); m.radius=LV_RADIUS_CIRCLE; m.bg_color=ink; m.bg_opa=LV_OPA_COVER;
    lv_area_t ma={mx-14,my-h/2,mx+14,my+h/2}; lv_draw_rect(layer,&m,&ma); }
}
static void tick(lv_timer_t* t){ (void)t; if(!g_root||lv_obj_has_flag(g_root,LV_OBJ_FLAG_HIDDEN)) return;
  g_now=lv_tick_get(); const gw_aora_cfg_t* cfg=&gw_aora_cfg[g_st];
  if(g_now-g_pool_at>=cfg->pool_ms){ g_pool_at=g_now; g_pool_pos=(g_pool_pos+1)%cfg->count; target_eye(false); }
  float f=(float)(g_now-g_morph_at)/cfg->transition_ms; if(f>1) f=1; f=f*f*(3-2*f);
  for(int e=0;e<2;e++) for(int i=0;i<48;i++) for(int xy=0;xy<2;xy++)
    g_points[e][i][xy]=g_from[e][i][xy]+(g_to[e][i][xy]-g_from[e][i][xy])*f;
  lv_obj_invalidate(g_avatar); // Only the avatar's 220x220 area, never the screen.
  if(g_st==1) for(int i=0;i<7;i++){
    int h=5+(int)((1+wave(g_now+i*67,400))*g_lvl*(4-abs(i-3))/40);
    if(h>24) h=24; lv_obj_set_height(g_bar[i],h); lv_obj_align(g_bar[i],LV_ALIGN_CENTER,(i-3)*14,90);
  }
  if(g_st==2) for(int i=0;i<3;i++){
    int a=(g_now/12+i*35)%360;
    lv_obj_align(g_dot[i],LV_ALIGN_CENTER,(96*lv_trigo_cos(a))>>15,-94+((14*lv_trigo_sin(a))>>15));
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
  lv_obj_align(g_avatar,LV_ALIGN_CENTER,0,-30); lv_obj_remove_flag(g_avatar,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(g_avatar,draw_avatar,LV_EVENT_DRAW_MAIN,NULL);
  for(int i=0;i<7;i++){ g_bar[i]=rect(g_root,7,5,4,SKIN[0].accent); lv_obj_add_flag(g_bar[i],LV_OBJ_FLAG_HIDDEN); }
  for(int i=0;i<3;i++){ g_dot[i]=rect(g_root,7,7,4,i==0?0x9A72EE:(i==1?0x5B95F0:0x35C3BD)); lv_obj_add_flag(g_dot[i],LV_OBJ_FLAG_HIDDEN); }
  g_stat=lv_label_create(g_root); lv_obj_set_style_text_font(g_stat,F21,0); lv_obj_set_style_text_color(g_stat,lv_color_hex(SKIN[0].accent),0); lv_obj_align(g_stat,LV_ALIGN_CENTER,0,-152);
  g_sub=lv_obj_create(g_root); lv_obj_remove_style_all(g_sub); lv_obj_set_size(g_sub,300,84); lv_obj_align(g_sub,LV_ALIGN_CENTER,0,148);
  lv_obj_set_flex_flow(g_sub,LV_FLEX_FLOW_COLUMN); lv_obj_set_flex_align(g_sub,LV_FLEX_ALIGN_END,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
  lv_obj_set_scroll_dir(g_sub,LV_DIR_VER); lv_obj_set_scrollbar_mode(g_sub,LV_SCROLLBAR_MODE_OFF);
  g_user=lv_label_create(g_sub); lv_obj_set_style_text_font(g_user,F20,0); lv_obj_set_style_text_color(g_user,lv_color_hex(SKIN[0].sec),0); lv_obj_set_width(g_user,300);
  lv_label_set_long_mode(g_user,LV_LABEL_LONG_WRAP); lv_obj_set_style_text_align(g_user,LV_TEXT_ALIGN_CENTER,0);
  g_reply=lv_label_create(g_sub); lv_obj_set_style_text_font(g_reply,F20,0); lv_obj_set_style_text_color(g_reply,lv_color_white(),0); lv_obj_set_width(g_reply,300);
  lv_label_set_long_mode(g_reply,LV_LABEL_LONG_WRAP); lv_obj_set_style_text_align(g_reply,LV_TEXT_ALIGN_CENTER,0);
  lv_label_set_text(g_user,""); lv_label_set_text(g_reply,""); lv_label_set_text(g_stat,"");
  apply_skin();
  lv_obj_add_flag(g_root,LV_OBJ_FLAG_HIDDEN);
  if(!g_tm) g_tm=lv_timer_create(tick,33,NULL); lv_timer_pause(g_tm);
}
void gw_grok_visible(bool on){ if(!g_root) return; on?lv_obj_remove_flag(g_root,LV_OBJ_FLAG_HIDDEN):lv_obj_add_flag(g_root,LV_OBJ_FLAG_HIDDEN);
  if(g_tm){ on?lv_timer_resume(g_tm):lv_timer_pause(g_tm); } }  // zero cost while hidden
void gw_grok_state(int st){ if(st<0||st>4) return;
  if(st!=g_st){ g_st=st; g_now=lv_tick_get(); g_pool_at=g_now; g_pool_pos=0; target_eye(false); }
  if(g_root){
    for(int i=0;i<7;i++) st==1?lv_obj_remove_flag(g_bar[i],LV_OBJ_FLAG_HIDDEN):lv_obj_add_flag(g_bar[i],LV_OBJ_FLAG_HIDDEN);
    for(int i=0;i<3;i++) st==2?lv_obj_remove_flag(g_dot[i],LV_OBJ_FLAG_HIDDEN):lv_obj_add_flag(g_dot[i],LV_OBJ_FLAG_HIDDEN);
  }
  static const char* S[]={"轻触或按蓝键说话","正在聆听…","思考中…","","离线"}; if(g_stat&&st>=0&&st<=4) lv_label_set_text(g_stat,S[st]); }
void gw_grok_level(int l){ g_lvl=l<0?0:(l>100?100:l); }
void gw_grok_status(const char* s){ if(g_stat&&s) lv_label_set_text(g_stat,s); }
void gw_grok_text(const char* role,const char* t){ if(!g_root||!t) return;
  if(role&&!strcmp(role,"user")){ lv_label_set_text(g_user,t); lv_label_set_text(g_reply,""); }
  else lv_label_set_text(g_reply,t);
  lv_obj_scroll_to_y(g_sub,LV_COORD_MAX,LV_ANIM_OFF); }
static void apply_skin(void){ if(!g_root) return;
  lv_obj_set_style_bg_color(g_root,lv_color_hex(SK->bg),0);
  for(int i=0;i<7;i++) lv_obj_set_style_bg_color(g_bar[i],lv_color_hex(SK->accent),0);
  static const uint8_t mixv[3]={255,215,175};   // thinking dots: one hue, three tones (no rainbow)
  for(int i=0;i<3;i++) lv_obj_set_style_bg_color(g_dot[i],lv_color_mix(lv_color_hex(SK->accent),lv_color_hex(SK->bg),mixv[i]),0);
  lv_obj_set_style_text_color(g_stat,lv_color_hex(SK->accent),0);
  lv_obj_set_style_text_color(g_user,lv_color_hex(SK->sec),0); lv_obj_set_style_text_color(g_reply,lv_color_hex(SK->reply),0);
  lv_obj_invalidate(g_root); }
void gw_grok_set_night(bool on){ g_night=on; apply_skin(); }
