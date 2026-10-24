#pragma GCC diagnostic ignored "-Wmisleading-indentation"
// Grok voice-page character: LVGL-drawn orb with eyes. States: 0 idle 1 listening 2 thinking 3 speaking 4 offline.
// Placeholder geometry until the Grok Bot phone-app character reference arrives; hooks are final.
#include "gw_faces.h"
#include <stdio.h>
#include <string.h>
const lv_font_t* gw_f20(void); const lv_font_t* gw_f21(void);
#define F20 gw_f20()
#define F21 gw_f21()
enum { ORG=0xFF6A13, CARD=0x1C1C1E, SEC=0x8E8E93 };
static lv_obj_t *g_root,*g_ring,*g_body,*g_eye[2],*g_mouth,*g_bar[7],*g_dot[3],*g_stat,*g_sub,*g_user,*g_reply;
static int g_st=0, g_lvl=0, g_t=0; static lv_timer_t* g_tm;
static lv_obj_t* rect(lv_obj_t* p,int w,int h,int r,uint32_t c){ lv_obj_t* o=lv_obj_create(p); lv_obj_remove_style_all(o); lv_obj_set_size(o,w,h);
  lv_obj_set_style_radius(o,r,0); lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0); lv_obj_set_style_bg_color(o,lv_color_hex(c),0);
  lv_obj_remove_flag(o,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE); return o; }
static int tri(int t,int per){ t%=per; return t<per/2? t*200/per : 200-t*200/per; } // 0..100..0
static void tick(lv_timer_t* t){ (void)t; if(!g_root||lv_obj_has_flag(g_root,LV_OBJ_FLAG_HIDDEN)) return; g_t++;
  int eh=34, ey=-14, ex=0;
  if(g_st==0){ int ph=g_t%120; if(ph<4) eh=34-ph*8; else if(ph<8) eh=2+(ph-4)*8; }          // blink ~ every 4 s
  if(g_st==2){ ex=10; ey=-22; }                                                            // looks up-right while thinking
  if(g_st==4){ eh=6; }
  for(int i=0;i<2;i++){ lv_obj_set_height(g_eye[i],eh<4?4:eh); lv_obj_align(g_eye[i],LV_ALIGN_CENTER,(i?26:-26)+ex,ey); }
  // ring breathing / pulse
  int pulse = g_st==1 ? 6+g_lvl/8 : (g_st==3 ? 4+tri(g_t,10)/20 : 2+tri(g_t,90)/34);
  lv_obj_set_style_border_width(g_ring,pulse,0);
  lv_obj_set_style_border_color(g_ring,lv_color_hex(g_st==4?0x3A3A3C:ORG),0);
  // listening waveform
  for(int i=0;i<7;i++){ bool on=g_st==1; on?lv_obj_remove_flag(g_bar[i],LV_OBJ_FLAG_HIDDEN):lv_obj_add_flag(g_bar[i],LV_OBJ_FLAG_HIDDEN);
    if(on){ int k=(g_t*7+i*29)%23; int h=8+(g_lvl*(40+k*3)/100)*(i==3?100:(i==2||i==4?80:(i==1||i==5?60:40)))/100; lv_obj_set_height(g_bar[i],h); lv_obj_align(g_bar[i],LV_ALIGN_CENTER,(i-3)*14,108); } }
  // thinking dots orbit
  for(int i=0;i<3;i++){ bool on=g_st==2; on?lv_obj_remove_flag(g_dot[i],LV_OBJ_FLAG_HIDDEN):lv_obj_add_flag(g_dot[i],LV_OBJ_FLAG_HIDDEN);
    if(on){ int a=(g_t*9+i*40)%360; lv_obj_align(g_dot[i],LV_ALIGN_CENTER,(92*lv_trigo_cos(a))>>15,(92*lv_trigo_sin(a))>>15); lv_obj_set_style_bg_opa(g_dot[i],255-i*70,0); } }
  // speaking mouth
  if(g_st==3){ lv_obj_remove_flag(g_mouth,LV_OBJ_FLAG_HIDDEN); int h=6+tri(g_t*3+(g_t%7),12)/5; lv_obj_set_size(g_mouth,30,h); lv_obj_align(g_mouth,LV_ALIGN_CENTER,0,30); }
  else lv_obj_add_flag(g_mouth,LV_OBJ_FLAG_HIDDEN);
}
void gw_grok_create(lv_obj_t* parent){
  g_root=lv_obj_create(parent); lv_obj_remove_style_all(g_root); lv_obj_set_size(g_root,466,466); lv_obj_center(g_root); lv_obj_remove_flag(g_root,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
  g_ring=rect(g_root,150,150,LV_RADIUS_CIRCLE,0x000000); lv_obj_set_style_bg_opa(g_ring,LV_OPA_TRANSP,0); lv_obj_set_style_border_color(g_ring,lv_color_hex(ORG),0); lv_obj_align(g_ring,LV_ALIGN_CENTER,0,-20);
  g_body=rect(g_root,124,124,LV_RADIUS_CIRCLE,CARD); lv_obj_align(g_body,LV_ALIGN_CENTER,0,-20);
  lv_obj_set_style_bg_grad_color(g_body,lv_color_hex(0x2C2C2E),0); lv_obj_set_style_bg_grad_dir(g_body,LV_GRAD_DIR_VER,0);
  for(int i=0;i<2;i++) g_eye[i]=rect(g_body,18,34,9,0xFFFFFF);
  g_mouth=rect(g_body,30,6,3,ORG);
  for(int i=0;i<7;i++) g_bar[i]=rect(g_root,8,10,4,ORG);
  for(int i=0;i<3;i++){ g_dot[i]=rect(g_root,10,10,5,ORG); }
  for(int i=0;i<3;i++) lv_obj_set_parent(g_dot[i],g_root);
  g_stat=lv_label_create(g_root); lv_obj_set_style_text_font(g_stat,F21,0); lv_obj_set_style_text_color(g_stat,lv_color_hex(ORG),0); lv_obj_align(g_stat,LV_ALIGN_CENTER,0,-132);
  g_sub=lv_obj_create(g_root); lv_obj_remove_style_all(g_sub); lv_obj_set_size(g_sub,300,96); lv_obj_align(g_sub,LV_ALIGN_CENTER,0,136);
  lv_obj_set_flex_flow(g_sub,LV_FLEX_FLOW_COLUMN); lv_obj_set_flex_align(g_sub,LV_FLEX_ALIGN_END,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
  lv_obj_set_scroll_dir(g_sub,LV_DIR_VER); lv_obj_set_scrollbar_mode(g_sub,LV_SCROLLBAR_MODE_OFF);
  g_user=lv_label_create(g_sub); lv_obj_set_style_text_font(g_user,F20,0); lv_obj_set_style_text_color(g_user,lv_color_hex(SEC),0); lv_obj_set_width(g_user,300);
  lv_label_set_long_mode(g_user,LV_LABEL_LONG_WRAP); lv_obj_set_style_text_align(g_user,LV_TEXT_ALIGN_CENTER,0);
  g_reply=lv_label_create(g_sub); lv_obj_set_style_text_font(g_reply,F20,0); lv_obj_set_style_text_color(g_reply,lv_color_white(),0); lv_obj_set_width(g_reply,300);
  lv_label_set_long_mode(g_reply,LV_LABEL_LONG_WRAP); lv_obj_set_style_text_align(g_reply,LV_TEXT_ALIGN_CENTER,0);
  lv_label_set_text(g_user,""); lv_label_set_text(g_reply,""); lv_label_set_text(g_stat,"");
  lv_obj_add_flag(g_root,LV_OBJ_FLAG_HIDDEN);
  if(!g_tm){ g_tm=lv_timer_create(tick,33,NULL); lv_timer_pause(g_tm); }
}
void gw_grok_visible(bool on){ if(!g_root) return; on?lv_obj_remove_flag(g_root,LV_OBJ_FLAG_HIDDEN):lv_obj_add_flag(g_root,LV_OBJ_FLAG_HIDDEN);
  if(g_tm){ on?lv_timer_resume(g_tm):lv_timer_pause(g_tm); } }  // zero cost while hidden
void gw_grok_state(int st){ g_st=st; static const char* S[]={"轻触或按蓝键说话","正在聆听…","思考中…","Grok","离线"}; if(g_stat&&st>=0&&st<=4) lv_label_set_text(g_stat,S[st]); }
void gw_grok_level(int l){ g_lvl=l<0?0:(l>100?100:l); }
void gw_grok_status(const char* s){ if(g_stat&&s) lv_label_set_text(g_stat,s); }
void gw_grok_text(const char* role,const char* t){ if(!g_root||!t) return;
  if(role&&!strcmp(role,"user")){ lv_label_set_text(g_user,t); lv_label_set_text(g_reply,""); }
  else lv_label_set_text(g_reply,t);
  lv_obj_scroll_to_y(g_sub,LV_COORD_MAX,LV_ANIM_OFF); }
