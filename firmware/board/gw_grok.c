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
const lv_font_t* gw_f20(void); const lv_font_t* gw_f21(void);
#define F20 gw_f20()
#define F21 gw_f21()
enum { ORG=0xFF6A13, SEC=0x8E8E93 };
static lv_obj_t *g_root,*g_avatar,*g_bar[7],*g_dot[3],*g_stat,*g_sub,*g_user,*g_reply;
static int g_st=0, g_lvl=0;
static lv_timer_t* g_tm;
static uint32_t g_now, g_pool_at, g_morph_at;
static uint8_t g_pool_pos;
static float g_points[2][48][2], g_from[2][48][2], g_to[2][48][2];
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
static void draw_avatar(lv_event_t* event){
  lv_layer_t* layer=lv_event_get_layer(event);
  lv_area_t bounds; lv_obj_get_coords(g_avatar,&bounds);
  float breathe=1.0f+.010f*wave(g_now,3600);
  float bob=g_st==3?2.2f*wave(g_now,1400):(g_st==4?4.0f:0);
  float cx=bounds.x1+110, cy=bounds.y1+110+bob;
  int radius=(int)(100*breathe);
  lv_area_t body={(int32_t)cx-radius,(int32_t)cy-radius,(int32_t)cx+radius,(int32_t)cy+radius};
  lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d); d.radius=LV_RADIUS_CIRCLE;
  d.bg_color=lv_color_hex(g_st==3?0xF6EFE4:0xF3F1EB); d.bg_opa=LV_OPA_COVER;
  d.bg_grad.dir=LV_GRAD_DIR_VER; d.bg_grad.stops_count=2;
  d.bg_grad.stops[0].color=d.bg_color; d.bg_grad.stops[0].opa=255; d.bg_grad.stops[0].frac=0;
  d.bg_grad.stops[1].color=lv_color_hex(0xD4D1CB); d.bg_grad.stops[1].opa=255; d.bg_grad.stops[1].frac=255;
  lv_draw_rect(layer,&d,&body);
  float dx=0,dy=0,eye_scale=g_st==4?.88f:1;
  if(g_st==0){ dx=10*glance(g_now,4800); dy=2*wave(g_now+718,4100); }
  if(g_st==1) dy=6*wave(g_now,2200);
  if(g_st==2){ dx=9*wave(g_now,2600); dy=-8; }
  if(g_st==3) dy=-3+6*glance(g_now,3000);
  if(g_st==4) dy=12+1.6f*wave(g_now,3600);
  uint32_t blink=g_now%gw_aora_cfg[g_st].blink_ms;
  float open=blink<90?1-.94f*blink/90:(blink<200?.06f+.94f*(blink-90)/110:1);
  lv_draw_triangle_dsc_t tri; lv_draw_triangle_dsc_init(&tri); tri.color=lv_color_hex(0x1C1C1B); tri.opa=255;
  for(int e=0;e<2;e++){
    lv_point_precise_t points[48]; float mid=0;
    for(int i=0;i<48;i++) mid+=g_points[e][i][1]/48;
    for(int i=0;i<48;i++){
      points[i].x=(int32_t)(cx+(g_points[e][i][0]*eye_scale+dx)*.85f*breathe);
      points[i].y=(int32_t)(cy+(mid+(g_points[e][i][1]-mid)*open*eye_scale+dy)*.85f*breathe);
    }
    // Strip triangulation follows the paired sides of the upstream eye contours.
    int lo=0,hi=47; bool left=true;
    while(hi-lo>1){ tri.p[0]=points[lo]; tri.p[1]=points[hi];
      tri.p[2]=left?points[++lo]:points[--hi]; left=!left; lv_draw_triangle(layer,&tri); }
  }
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
  g_root=lv_obj_create(parent); lv_obj_remove_style_all(g_root); lv_obj_set_size(g_root,466,466); lv_obj_center(g_root); lv_obj_remove_flag(g_root,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
  g_st=0; g_lvl=0; g_now=lv_tick_get(); g_pool_at=g_now; g_pool_pos=0; target_eye(true);
  g_avatar=lv_obj_create(g_root); lv_obj_remove_style_all(g_avatar); lv_obj_set_size(g_avatar,220,220);
  lv_obj_align(g_avatar,LV_ALIGN_CENTER,0,-30); lv_obj_remove_flag(g_avatar,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(g_avatar,draw_avatar,LV_EVENT_DRAW_MAIN,NULL);
  for(int i=0;i<7;i++){ g_bar[i]=rect(g_root,7,5,4,ORG); lv_obj_add_flag(g_bar[i],LV_OBJ_FLAG_HIDDEN); }
  for(int i=0;i<3;i++){ g_dot[i]=rect(g_root,7,7,4,i==0?0x9A72EE:(i==1?0x5B95F0:0x35C3BD)); lv_obj_add_flag(g_dot[i],LV_OBJ_FLAG_HIDDEN); }
  g_stat=lv_label_create(g_root); lv_obj_set_style_text_font(g_stat,F21,0); lv_obj_set_style_text_color(g_stat,lv_color_hex(ORG),0); lv_obj_align(g_stat,LV_ALIGN_CENTER,0,-152);
  g_sub=lv_obj_create(g_root); lv_obj_remove_style_all(g_sub); lv_obj_set_size(g_sub,300,84); lv_obj_align(g_sub,LV_ALIGN_CENTER,0,148);
  lv_obj_set_flex_flow(g_sub,LV_FLEX_FLOW_COLUMN); lv_obj_set_flex_align(g_sub,LV_FLEX_ALIGN_END,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
  lv_obj_set_scroll_dir(g_sub,LV_DIR_VER); lv_obj_set_scrollbar_mode(g_sub,LV_SCROLLBAR_MODE_OFF);
  g_user=lv_label_create(g_sub); lv_obj_set_style_text_font(g_user,F20,0); lv_obj_set_style_text_color(g_user,lv_color_hex(SEC),0); lv_obj_set_width(g_user,300);
  lv_label_set_long_mode(g_user,LV_LABEL_LONG_WRAP); lv_obj_set_style_text_align(g_user,LV_TEXT_ALIGN_CENTER,0);
  g_reply=lv_label_create(g_sub); lv_obj_set_style_text_font(g_reply,F20,0); lv_obj_set_style_text_color(g_reply,lv_color_white(),0); lv_obj_set_width(g_reply,300);
  lv_label_set_long_mode(g_reply,LV_LABEL_LONG_WRAP); lv_obj_set_style_text_align(g_reply,LV_TEXT_ALIGN_CENTER,0);
  lv_label_set_text(g_user,""); lv_label_set_text(g_reply,""); lv_label_set_text(g_stat,"");
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
  static const char* S[]={"轻触或按蓝键说话","正在聆听…","思考中…","Grok","离线"}; if(g_stat&&st>=0&&st<=4) lv_label_set_text(g_stat,S[st]); }
void gw_grok_level(int l){ g_lvl=l<0?0:(l>100?100:l); }
void gw_grok_status(const char* s){ if(g_stat&&s) lv_label_set_text(g_stat,s); }
void gw_grok_text(const char* role,const char* t){ if(!g_root||!t) return;
  if(role&&!strcmp(role,"user")){ lv_label_set_text(g_user,t); lv_label_set_text(g_reply,""); }
  else lv_label_set_text(g_reply,t);
  lv_obj_scroll_to_y(g_sub,LV_COORD_MAX,LV_ANIM_OFF); }
