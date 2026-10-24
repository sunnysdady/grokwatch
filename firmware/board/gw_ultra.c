// GrokWatch "Ultra" — Apple Watch Ultra (Wayfinder / Modular Ultra) inspired, round 466x466 AMOLED.
// Pure LVGL 9: shared by firmware and the host preview renderer.
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#include "gw_faces.h"
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(gu_hero) LV_FONT_DECLARE(gu_num) LV_FONT_DECLARE(gu_sm) LV_FONT_DECLARE(gu_lab) LV_FONT_DECLARE(gu_icon) LV_FONT_DECLARE(gz_20) LV_FONT_DECLARE(gz_21b)
// RAM copies of the compact CJK fonts so a runtime fallback (full CJK font) can cover rare characters
static lv_font_t f20_rt, f21_rt; static bool f_init=false;
const lv_font_t* gw_f20(void){ if(!f_init){ f20_rt=gz_20; f21_rt=gz_21b; f_init=true; } return &f20_rt; }
const lv_font_t* gw_f21(void){ gw_f20(); return &f21_rt; }
void gw_faces_set_fallback(const lv_font_t* f){ gw_f20(); f20_rt.fallback=f; f21_rt.fallback=f; }
#define F20 gw_f20()
#define F21 gw_f21()
#define I_WIFI "\xEF\x87\xAB"
#define I_BT   "\xEF\x8A\x94"
#define I_BELL "\xEF\x83\xB3"
#define I_BOLT "\xEF\x83\xA7"
#define I_BIKE "\xEF\x88\x86"
#define I_LOC  "\xEF\x84\xA4"
#define I_MSG  "\xEF\x81\xB5"
#define I_MOON "\xEF\x86\x86"
#define I_MIC  "\xEF\x84\xB0"
#define I_SUN  "\xEF\x86\x85"
#define I_VOL  "\xEF\x80\xA8"
#define I_DND  "\xEF\x87\xB6"
#define I_BAT  "\xEF\x89\x80"
#define I_WX   "\xEF\x87\x97"
#define I_MAIL "\xEF\x83\xA0"
#define I_TEL  "\xEF\x82\x95"
// palette (watchOS dark): pure black, international orange accent
enum { K_TXT=0xFFFFFF, K_SEC=0x8E8E93, K_TER=0x3A3A3C, K_CARD=0x1C1C1E, K_CARD2=0x2C2C2E, K_ORG=0xFF6A13, K_GRN=0x30D158, K_RED=0xFF453A, K_CYAN=0x64D2FF, K_BLUE=0x0A84FF, K_YEL=0xFFD60A };
static bool night=false, fade=true;
static lv_color_t col(uint32_t h){ if(!night) return lv_color_hex(h);
  int r=(h>>16)&255,g=(h>>8)&255,b=h&255; int l=r>g?r:g; if(b>l) l=b; if(h==K_CARD||h==K_CARD2) l=l*7/10; return lv_color_make(l,0,0); }

static lv_obj_t *root,*pg[GW_NPAGES],*dotbar,*dots[GW_NPAGES],*chat,*sheet,*sheet_t,*sheet_b;
static int cur=-1, dir=1;
// face
static lv_obj_t *f_sec,*f_date,*f_time,*f_g[3],*f_gv[3],*f_glink2;
// ride
static lv_obj_t *r_arc,*r_speed,*r_unit,*r_f[2],*r_fl[2],*r_hint;
// notes
#define NROW 5   // rows kept small: each row is ~8 LVGL objects in internal RAM
static lv_obj_t *n_more2,*n_list,*n_head,*n_row[NROW],*n_ico[NROW],*n_icl[NROW],*n_app[NROW],*n_tm[NROW],*n_t[NROW],*n_b[NROW],*n_empty[3];
static const gw_note_t* n_src; static int n_cnt;
static lv_obj_t *sh_scroll,*sh_app,*sh_t,*sh_b;
static lv_obj_t *cc,*cc_tile[GW_CC_N],*cc_ic[GW_CC_N],*cc_lb[GW_CC_N],*cc_val[GW_CC_N],*cc_stat; static void (*cc_cb)(int);
// nav
static lv_obj_t *v_img,*v_dist,*v_dir,*v_meta,*v_idle[4];
static lv_image_dsc_t v_dsc; static uint8_t v_px[48*48]; static uint8_t v_last[288]; static bool v_has=false;
// chat deco
static lv_obj_t *c_time;
// animation targets
static float t_bat=0, t_spd=0, a_spd=0; static int prog=1000;

static lv_obj_t* box(lv_obj_t* p){ lv_obj_t* o=lv_obj_create(p); lv_obj_remove_style_all(o); lv_obj_set_size(o,466,466); lv_obj_center(o);
  lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE); return o; }
static lv_obj_t* label(lv_obj_t* p,const lv_font_t* f,uint32_t c,int x,int y){ lv_obj_t* l=lv_label_create(p);
  if(f) lv_obj_set_style_text_font(l,f,0); lv_obj_set_style_text_color(l,col(c),0); lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);
  lv_obj_align(l,LV_ALIGN_CENTER,x,y); return l; }
static lv_obj_t* arc(lv_obj_t* p,int size,int w,uint32_t bg,uint32_t fg){ lv_obj_t* a=lv_arc_create(p);
  lv_obj_set_size(a,size,size); lv_obj_remove_style(a,NULL,LV_PART_KNOB); lv_obj_remove_flag(a,LV_OBJ_FLAG_CLICKABLE); lv_arc_set_rotation(a,0);
  lv_obj_set_style_arc_width(a,w,LV_PART_MAIN); lv_obj_set_style_arc_color(a,col(bg),LV_PART_MAIN); lv_obj_set_style_arc_rounded(a,true,LV_PART_MAIN);
  lv_obj_set_style_arc_width(a,w,LV_PART_INDICATOR); lv_obj_set_style_arc_color(a,col(fg),LV_PART_INDICATOR); lv_obj_set_style_arc_rounded(a,true,LV_PART_INDICATOR);
  lv_obj_set_style_pad_all(a,0,0); lv_obj_set_style_bg_opa(a,LV_OPA_TRANSP,0); lv_obj_set_style_border_width(a,0,0); return a; }
static void arc_set(lv_obj_t* a,int a0,int span,float f){ if(f<0)f=0; if(f>1)f=1; int e=(int)(span*f+0.5f);
  lv_arc_set_bg_angles(a,a0,a0+span); if(e<=0) lv_obj_set_style_arc_opa(a,LV_OPA_TRANSP,LV_PART_INDICATOR);
  else { lv_obj_set_style_arc_opa(a,LV_OPA_COVER,LV_PART_INDICATOR); lv_arc_set_angles(a,a0,a0+(e>=360?359:e)); } }
static lv_obj_t* disc(lv_obj_t* p,int s,uint32_t c){ lv_obj_t* d=lv_obj_create(p); lv_obj_remove_style_all(d); lv_obj_set_size(d,s,s);
  lv_obj_set_style_radius(d,LV_RADIUS_CIRCLE,0); lv_obj_set_style_bg_opa(d,LV_OPA_COVER,0); lv_obj_set_style_bg_color(d,col(c),0); lv_obj_remove_flag(d,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE); return d; }
static void on_rim(lv_obj_t* d,int r,int deg){ lv_obj_align(d,LV_ALIGN_CENTER,(r*lv_trigo_cos(deg-90))>>15,(r*lv_trigo_sin(deg-90))>>15); }
// Wayfinder-style bezel: 0 minutes, 1 speed 0-60, 2 compass
static const char* L_MIN[]={"60","05","10","15","20","25","30","35","40","45","50","55",NULL};
static const char* L_CMP[]={"N","E","S","W",NULL};
static void bezel(lv_obj_t* p,int mode,bool dim){
  lv_obj_t* s=lv_scale_create(p); lv_obj_set_size(s,462,462); lv_obj_center(s); lv_scale_set_mode(s,LV_SCALE_MODE_ROUND_INNER);
  if(mode==1){ lv_scale_set_total_tick_count(s,61); lv_scale_set_major_tick_every(s,10); lv_scale_set_angle_range(s,270); lv_scale_set_rotation(s,135); lv_scale_set_range(s,0,60); }
  else { lv_scale_set_total_tick_count(s,mode==2?72:60); lv_scale_set_major_tick_every(s,mode==2?18:5); lv_scale_set_angle_range(s,mode==2?355:354); lv_scale_set_rotation(s,270);
    lv_scale_set_text_src(s,mode==2?L_CMP:L_MIN); }
  lv_scale_set_label_show(s,!dim);
  lv_obj_set_style_length(s,8,LV_PART_ITEMS); lv_obj_set_style_line_width(s,2,LV_PART_ITEMS); lv_obj_set_style_line_color(s,col(dim?K_CARD2:K_TER),LV_PART_ITEMS);
  lv_obj_set_style_length(s,16,LV_PART_INDICATOR); lv_obj_set_style_line_width(s,3,LV_PART_INDICATOR); lv_obj_set_style_line_color(s,col(dim?K_TER:K_SEC),LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(s,0,LV_PART_MAIN); lv_obj_set_style_bg_opa(s,LV_OPA_TRANSP,0); lv_obj_set_style_border_width(s,0,0);
  lv_obj_set_style_text_font(s,&gu_lab,LV_PART_INDICATOR); lv_obj_set_style_text_color(s,col(K_SEC),LV_PART_INDICATOR); lv_obj_set_style_pad_radial(s,8,LV_PART_INDICATOR);
  lv_obj_remove_flag(s,LV_OBJ_FLAG_CLICKABLE);
  if(mode!=1){ lv_obj_t* m=lv_obj_create(p); lv_obj_remove_style_all(m); lv_obj_set_size(m,6,20); lv_obj_set_style_radius(m,3,0);
    lv_obj_set_style_bg_opa(m,LV_OPA_COVER,0); lv_obj_set_style_bg_color(m,col(K_ORG),0); lv_obj_align(m,LV_ALIGN_TOP_MID,0,4); }
}
// circular complication (Modular Ultra)
static lv_obj_t* gauge(lv_obj_t* p,int x,int y,uint32_t c,lv_obj_t** val){ lv_obj_t* g=arc(p,84,7,K_CARD2,c); lv_obj_align(g,LV_ALIGN_CENTER,x,y);
  *val=label(p,&gu_sm,K_TXT,x,y); return g; }
static void sheet_click(lv_event_t* e){ (void)e; gw_faces_sheet(NULL,NULL); }
static void card_click(lv_event_t* e){ int i=(int)(intptr_t)lv_event_get_user_data(e); if(n_src&&i<n_cnt) gw_faces_note_open(i); }
// app -> icon/colour (Chinese app labels as sent by the phone)
static void app_style(const char* a,const char** ic,uint32_t* c){ *ic=I_MSG; *c=K_ORG;
  if(strstr(a,"微信")||strstr(a,"WeChat")){ *ic=I_WX; *c=K_GRN; }
  else if(strstr(a,"信息")||strstr(a,"短信")||strstr(a,"Messages")){ *ic=I_MSG; *c=K_GRN; }
  else if(strstr(a,"邮件")||strstr(a,"Mail")||strstr(a,"Gmail")||strstr(a,"Outlook")){ *ic=I_MAIL; *c=K_BLUE; }
  else if(strstr(a,"电话")||strstr(a,"Phone")){ *ic=I_TEL; *c=K_GRN; } }

static void cc_click(lv_event_t* e){ int i=(int)(intptr_t)lv_event_get_user_data(e); if(cc_cb) cc_cb(i); }
static void cc_bg_click(lv_event_t* e){ (void)e; }
void gw_faces_set_night(bool on){ night=on; }
void gw_faces_set_fade(bool on){ fade=on; }
void gw_faces_dir(int d){ dir=d<0?-1:1; }

void gw_faces_create(lv_obj_t* parent){
  if(root){ lv_obj_delete(root); lv_obj_delete(chat); lv_obj_delete(dotbar); lv_obj_delete(sheet); lv_obj_delete(cc); root=NULL; cur=-1; }
  // chat-page decoration sits right above the native container (index 1), under xiaozhi's emoji/status labels
  chat=box(parent); lv_obj_move_to_index(chat,1); bezel(chat,0,true);
  c_time=label(chat,&gu_sm,K_ORG,0,-168); lv_obj_set_style_text_letter_space(c_time,1,0);
  root=box(parent); lv_obj_set_style_bg_color(root,lv_color_black(),0); lv_obj_set_style_bg_opa(root,LV_OPA_COVER,0);
  for(int i=1;i<GW_NPAGES;i++){ pg[i]=box(root); lv_obj_add_flag(pg[i],LV_OBJ_FLAG_HIDDEN); }
  // ---- FACE: Wayfinder bezel + bold numerals + 3 circular complications
  lv_obj_t* p=pg[GW_PAGE_FACE]; bezel(p,0,false);
  f_sec=disc(p,10,K_ORG);
  f_date=label(p,&gu_lab,K_ORG,0,-104); lv_obj_set_style_text_letter_space(f_date,2,0);
  f_time=label(p,&gu_hero,K_TXT,0,-22); lv_obj_set_style_text_letter_space(f_time,-4,0);
  f_g[0]=gauge(p,-100,100,K_GRN,&f_gv[0]); f_g[1]=gauge(p,0,112,K_ORG,&f_gv[1]); f_g[2]=gauge(p,100,100,K_CYAN,&f_gv[2]);
  f_glink2=arc(p,84,7,K_CARD2,K_BLUE); lv_obj_align(f_glink2,LV_ALIGN_CENTER,100,100); lv_obj_set_style_arc_opa(f_glink2,LV_OPA_TRANSP,LV_PART_MAIN);
  // ---- RIDE: speed dial bezel 0-60 + orange sweep
  p=pg[GW_PAGE_RIDE]; bezel(p,1,false);
  r_arc=arc(p,360,12,K_CARD,K_ORG); lv_obj_center(r_arc);
  lv_obj_t* h=label(p,&gu_lab,K_ORG,0,-112); lv_label_set_text(h,"RIDE"); lv_obj_set_style_text_letter_space(h,3,0);
  lv_obj_t* hb=label(p,&gu_sm,K_ORG,0,-144); lv_label_set_text(hb,I_BIKE);
  r_speed=label(p,&gu_hero,K_TXT,0,-22); lv_obj_set_style_text_letter_space(r_speed,-4,0);
  r_unit=label(p,&gu_lab,K_SEC,0,42); lv_label_set_text(r_unit,"KM/H"); lv_obj_set_style_text_letter_space(r_unit,3,0);
  for(int i=0;i<2;i++){ int x=i?66:-66; r_f[i]=label(p,&gu_sm,K_TXT,x,94); r_fl[i]=label(p,&gu_lab,K_SEC,x,124); lv_label_set_text(r_fl[i],i?"KM":"TIME"); lv_obj_set_style_text_letter_space(r_fl[i],2,0); }
  r_hint=label(p,F20,K_SEC,0,158); lv_label_set_text(r_hint,"打开 GrokWatch App");
  // ---- NOTES: compact scrolling list (3-4 rows visible inside the round safe area)
  p=pg[GW_PAGE_NOTES];
  n_list=lv_obj_create(p); lv_obj_remove_style_all(n_list); lv_obj_set_size(n_list,350,466); lv_obj_center(n_list);
  lv_obj_set_flex_flow(n_list,LV_FLEX_FLOW_COLUMN); lv_obj_set_flex_align(n_list,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_top(n_list,58,0); lv_obj_set_style_pad_bottom(n_list,96,0); lv_obj_set_style_pad_row(n_list,8,0);
  lv_obj_set_scroll_dir(n_list,LV_DIR_VER); lv_obj_set_scrollbar_mode(n_list,LV_SCROLLBAR_MODE_OFF); lv_obj_add_flag(n_list,LV_OBJ_FLAG_SCROLL_MOMENTUM);
  n_head=lv_label_create(n_list); lv_obj_set_style_text_font(n_head,F21,0); lv_obj_set_style_text_color(n_head,col(K_ORG),0);
  for(int i=0;i<NROW;i++){ lv_obj_t* c=lv_obj_create(n_list); lv_obj_remove_style_all(c); n_row[i]=c; lv_obj_set_size(c,320,LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(c,col(K_CARD),0); lv_obj_set_style_bg_opa(c,LV_OPA_COVER,0); lv_obj_set_style_radius(c,22,0);
    lv_obj_set_style_pad_all(c,12,0); lv_obj_set_style_pad_left(c,58,0); lv_obj_remove_flag(c,LV_OBJ_FLAG_SCROLLABLE); lv_obj_add_flag(c,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_flex_flow(c,LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_row(c,1,0);
    lv_obj_add_event_cb(c,card_click,LV_EVENT_CLICKED,(void*)(intptr_t)i);
    n_ico[i]=disc(c,36,K_ORG); lv_obj_add_flag(n_ico[i],LV_OBJ_FLAG_IGNORE_LAYOUT); lv_obj_align(n_ico[i],LV_ALIGN_TOP_LEFT,-46,0);
    n_icl[i]=lv_label_create(n_ico[i]); lv_obj_set_style_text_font(n_icl[i],&gu_sm,0); lv_obj_set_style_text_color(n_icl[i],lv_color_black(),0); lv_obj_center(n_icl[i]);
    lv_obj_t* top=lv_obj_create(c); lv_obj_remove_style_all(top); lv_obj_set_size(top,LV_PCT(100),LV_SIZE_CONTENT); lv_obj_remove_flag(top,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    n_app[i]=lv_label_create(top); lv_obj_set_style_text_font(n_app[i],F20,0); lv_obj_set_style_text_color(n_app[i],col(K_SEC),0);
    lv_obj_set_width(n_app[i],180); lv_label_set_long_mode(n_app[i],LV_LABEL_LONG_CLIP);
    n_tm[i]=lv_label_create(top); lv_obj_set_style_text_font(n_tm[i],F20,0); lv_obj_set_style_text_color(n_tm[i],col(K_SEC),0); lv_obj_align(n_tm[i],LV_ALIGN_TOP_RIGHT,0,0);
    n_t[i]=lv_label_create(c); lv_obj_set_style_text_font(n_t[i],F21,0); lv_obj_set_style_text_color(n_t[i],col(K_TXT),0);
    lv_obj_set_width(n_t[i],LV_PCT(100)); lv_label_set_long_mode(n_t[i],LV_LABEL_LONG_DOT); lv_obj_set_height(n_t[i],28);
    n_b[i]=lv_label_create(c); lv_obj_set_style_text_font(n_b[i],F20,0); lv_obj_set_style_text_color(n_b[i],col(0xC7C7CC),0);
    lv_obj_set_width(n_b[i],LV_PCT(100)); lv_label_set_long_mode(n_b[i],LV_LABEL_LONG_DOT); }
  n_more2=lv_label_create(n_list); lv_obj_set_style_text_font(n_more2,F20,0); lv_obj_set_style_text_color(n_more2,col(K_SEC),0);
  n_empty[0]=disc(p,128,K_CARD); lv_obj_align(n_empty[0],LV_ALIGN_CENTER,0,-46);
  n_empty[1]=lv_label_create(n_empty[0]); lv_obj_set_style_text_font(n_empty[1],&gu_icon,0); lv_obj_set_style_text_color(n_empty[1],col(K_SEC),0); lv_label_set_text(n_empty[1],I_BELL); lv_obj_center(n_empty[1]);
  n_empty[2]=label(p,F21,K_SEC,0,64); lv_label_set_text(n_empty[2],"没有新通知");
  // ---- NAV: compass bezel + big turn arrow
  p=pg[GW_PAGE_NAV]; bezel(p,2,false);
  v_img=lv_image_create(p); lv_obj_align(v_img,LV_ALIGN_CENTER,0,-84); lv_image_set_scale(v_img,640);
  lv_obj_set_style_image_recolor(v_img,col(K_TXT),0); lv_obj_set_style_image_recolor_opa(v_img,LV_OPA_COVER,0);
  v_dist=label(p,&gu_num,K_TXT,0,24); lv_obj_set_style_text_letter_space(v_dist,-1,0);
  v_dir=label(p,NULL,K_TXT,0,80); lv_obj_set_width(v_dir,300); lv_label_set_long_mode(v_dir,LV_LABEL_LONG_DOT); lv_obj_set_height(v_dir,42);
  v_meta=label(p,F21,K_ORG,0,128);
  v_idle[0]=disc(p,140,K_CARD); lv_obj_align(v_idle[0],LV_ALIGN_CENTER,0,-50); lv_obj_set_style_border_width(v_idle[0],3,0);
  v_idle[1]=lv_label_create(v_idle[0]); lv_obj_set_style_text_font(v_idle[1],&gu_icon,0); lv_label_set_text(v_idle[1],I_BT); lv_obj_center(v_idle[1]);
  v_idle[2]=label(p,F21,K_TXT,0,64); v_idle[3]=label(p,&gu_lab,K_SEC,0,104); lv_obj_set_style_text_letter_space(v_idle[3],2,0);
  // ---- page dots (top layer: also visible over the native chat page). Transient, like watchOS.
  dotbar=lv_obj_create(lv_layer_top()); lv_obj_remove_style_all(dotbar); lv_obj_set_size(dotbar,140,12); lv_obj_align(dotbar,LV_ALIGN_CENTER,0,184);
  lv_obj_set_flex_flow(dotbar,LV_FLEX_FLOW_ROW); lv_obj_set_flex_align(dotbar,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(dotbar,7,0); lv_obj_remove_flag(dotbar,LV_OBJ_FLAG_CLICKABLE);
  for(int i=0;i<GW_NPAGES;i++){ dots[i]=disc(dotbar,7,K_TER); lv_obj_set_style_radius(dots[i],4,0); }
  // ---- modal sheet: full-screen, scrollable (expanded notification / summaries / confirms)
  sheet=box(parent); lv_obj_set_style_bg_color(sheet,lv_color_black(),0); lv_obj_set_style_bg_opa(sheet,LV_OPA_COVER,0);
  lv_obj_add_flag(sheet,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_HIDDEN); lv_obj_add_event_cb(sheet,sheet_click,LV_EVENT_CLICKED,NULL);
  sh_scroll=lv_obj_create(sheet); lv_obj_remove_style_all(sh_scroll); lv_obj_set_size(sh_scroll,330,466); lv_obj_center(sh_scroll);
  lv_obj_set_flex_flow(sh_scroll,LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_top(sh_scroll,78,0); lv_obj_set_style_pad_bottom(sh_scroll,110,0); lv_obj_set_style_pad_row(sh_scroll,8,0);
  lv_obj_set_scroll_dir(sh_scroll,LV_DIR_VER); lv_obj_set_scrollbar_mode(sh_scroll,LV_SCROLLBAR_MODE_OFF); lv_obj_add_flag(sh_scroll,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(sh_scroll,sheet_click,LV_EVENT_CLICKED,NULL);
  sh_app=lv_label_create(sh_scroll); lv_obj_set_style_text_font(sh_app,F20,0); lv_obj_set_style_text_color(sh_app,col(K_ORG),0);
  sh_t=lv_label_create(sh_scroll); lv_obj_set_style_text_font(sh_t,F21,0); lv_obj_set_style_text_color(sh_t,col(K_TXT),0); lv_obj_set_width(sh_t,LV_PCT(100)); lv_label_set_long_mode(sh_t,LV_LABEL_LONG_WRAP);
  sh_b=lv_label_create(sh_scroll); lv_obj_set_style_text_font(sh_b,F20,0); lv_obj_set_style_text_color(sh_b,col(0xE5E5EA),0); lv_obj_set_width(sh_b,LV_PCT(100));
  lv_label_set_long_mode(sh_b,LV_LABEL_LONG_WRAP); lv_obj_set_style_text_line_space(sh_b,6,0);
  { lv_obj_t* hh=label(sheet,&gu_lab,K_SEC,0,182); lv_label_set_text(hh,"TAP TO CLOSE"); lv_obj_set_style_text_letter_space(hh,2,0); }
  sheet_t=sh_t; sheet_b=sh_b;
  // ---- Control Center (swipe up on the face)
  cc=box(parent); lv_obj_set_style_bg_color(cc,lv_color_black(),0); lv_obj_set_style_bg_opa(cc,LV_OPA_COVER,0); lv_obj_add_flag(cc,LV_OBJ_FLAG_HIDDEN|LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(cc,cc_bg_click,LV_EVENT_CLICKED,NULL);
  cc_stat=label(cc,F20,K_SEC,0,-158);
  static const char* IC[GW_CC_N]={I_SUN,I_VOL,I_MOON,I_DND}; static const char* LB[GW_CC_N]={"亮度","音量","夜间","勿扰"};
  for(int i=0;i<GW_CC_N;i++){ int x=(i%2)?60:-60, y=(i/2)?62:-70;
    cc_tile[i]=disc(cc,92,K_CARD2); lv_obj_align(cc_tile[i],LV_ALIGN_CENTER,x,y); lv_obj_add_flag(cc_tile[i],LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cc_tile[i],cc_click,LV_EVENT_CLICKED,(void*)(intptr_t)i);
    cc_ic[i]=lv_label_create(cc_tile[i]); lv_obj_set_style_text_font(cc_ic[i],&gu_sm,0); lv_label_set_text(cc_ic[i],IC[i]); lv_obj_align(cc_ic[i],LV_ALIGN_CENTER,0,-10);
    cc_val[i]=lv_label_create(cc_tile[i]); lv_obj_set_style_text_font(cc_val[i],F20,0); lv_obj_align(cc_val[i],LV_ALIGN_CENTER,0,20);
    cc_lb[i]=label(cc,F20,K_SEC,x,y+60); lv_label_set_text(cc_lb[i],LB[i]); }
  
  gw_grok_create(chat);
}
void gw_faces_sheet(const char* t,const char* b){ if(!sheet) return; if(!t&&!b){ lv_obj_add_flag(sheet,LV_OBJ_FLAG_HIDDEN); return; }
  lv_label_set_text(sh_app,""); lv_obj_add_flag(sh_app,LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(sh_t,t?t:""); lv_obj_set_style_text_color(sh_t,col(K_ORG),0); lv_label_set_text(sh_b,b?b:"");
  lv_obj_scroll_to_y(sh_scroll,0,LV_ANIM_OFF); lv_obj_remove_flag(sheet,LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(sheet); }
void gw_faces_note_open(int i){ if(!n_src||i<0||i>=n_cnt) return; const gw_note_t* n=&n_src[i];
  char b[64]; snprintf(b,sizeof b,"%s  \xC2\xB7  %s",n->app,n->time); lv_label_set_text(sh_app,b); lv_obj_remove_flag(sh_app,LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(sh_t,n->title[0]?n->title:n->app); lv_obj_set_style_text_color(sh_t,col(K_TXT),0); lv_label_set_text(sh_b,n->body);
  lv_obj_scroll_to_y(sh_scroll,0,LV_ANIM_OFF); lv_obj_remove_flag(sheet,LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(sheet); }
static const gw_cc_t* cc_st;
void gw_cc_open(const gw_cc_t* st,void (*on_tap)(int)){ if(!cc) return; if(!st){ lv_obj_add_flag(cc,LV_OBJ_FLAG_HIDDEN); return; }
  cc_cb=on_tap; cc_st=st; char b[96];
  snprintf(b,sizeof b,"%s%d%%  ·  %s  ·  手机 %s",st->charging?"充电 ":"",st->battery,st->ble&&st->wifi?"蓝牙 Wi-Fi":(st->ble?"蓝牙":(st->wifi?"Wi-Fi":"未连接")),st->phone_batt>=0?"":"--");
  if(st->phone_batt>=0){ size_t l=strlen(b); snprintf(b+l,sizeof b-l,"%d%%",st->phone_batt); }
  lv_label_set_text(cc_stat,b);
  bool on[GW_CC_N]={st->bright>0,st->volume>0,st->night,st->dnd};
  for(int i=0;i<GW_CC_N;i++){ lv_obj_set_style_bg_color(cc_tile[i],col(on[i]&&i>=2?K_ORG:K_CARD2),0);
    lv_obj_set_style_text_color(cc_ic[i],(on[i]&&i>=2)?lv_color_black():col(i<2?K_ORG:K_TXT),0); lv_obj_set_style_text_color(cc_val[i],(on[i]&&i>=2)?lv_color_black():col(K_TXT),0); }
  snprintf(b,sizeof b,"%d%%",st->bright); lv_label_set_text(cc_val[0],b); snprintf(b,sizeof b,"%d%%",st->volume); lv_label_set_text(cc_val[1],b);
  lv_label_set_text(cc_val[2],st->night?"开":"关"); lv_label_set_text(cc_val[3],st->dnd?"开":"关");
  lv_obj_remove_flag(cc,LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(cc); }
bool gw_cc_is_open(void){ return cc&&!lv_obj_has_flag(cc,LV_OBJ_FLAG_HIDDEN); }
bool gw_faces_sheet_open(void){ return sheet&&!lv_obj_has_flag(sheet,LV_OBJ_FLAG_HIDDEN); }

// ---------- animations ----------
static void ax_x(void* o,int32_t v){ lv_obj_set_x((lv_obj_t*)o,v); }
static void ax_opa(void* o,int32_t v){ lv_obj_set_style_opa((lv_obj_t*)o,(lv_opa_t)v,0); }
static void ax_w(void* o,int32_t v){ lv_obj_set_width((lv_obj_t*)o,v); }
static void anim(void* o,lv_anim_exec_xcb_t cb,int32_t a,int32_t b,uint32_t ms,uint32_t delay,lv_anim_path_cb_t path){
  lv_anim_delete(o,cb); lv_anim_t x; lv_anim_init(&x); lv_anim_set_var(&x,o); lv_anim_set_exec_cb(&x,cb); lv_anim_set_values(&x,a,b);
  lv_anim_set_duration(&x,ms); lv_anim_set_delay(&x,delay); lv_anim_set_path_cb(&x,path?path:lv_anim_path_ease_out); lv_anim_start(&x); }
static void apply_prog(void){ float k=prog/1000.f;
  if(cur==GW_PAGE_FACE){ arc_set(f_g[0],135,270,t_bat*k); char b[8]; snprintf(b,sizeof b,"%d",(int)(t_bat*100*k+0.5f)); lv_label_set_text(f_gv[0],b); }
  if(cur==GW_PAGE_RIDE){ arc_set(r_arc,135,270,a_spd/60.f*k); }
}
static void ax_prog(void* o,int32_t v){ (void)o; prog=v; apply_prog(); }
static void ax_spd(void* o,int32_t v){ (void)o; a_spd=v/10.f; if(prog>=1000) apply_prog(); }
static void ax_img(void* o,int32_t v){ lv_obj_set_style_image_opa((lv_obj_t*)o,(lv_opa_t)v,0); }

void gw_faces_show(int page, const gw_state_t* s){
  if(!root) return;
  bool enter=page!=cur; int prev=cur; cur=page;
  if(enter){ // dots: capsule morph + transient visibility
    for(int i=0;i<GW_NPAGES;i++){ bool on=i==page; int w=lv_obj_get_width(dots[i]);
      lv_obj_set_style_bg_color(dots[i],col(on?K_ORG:K_TER),0); anim(dots[i],ax_w,w,on?24:7,160,0,NULL); }
    lv_obj_move_foreground(dotbar); anim(dotbar,ax_opa,LV_OPA_COVER,LV_OPA_COVER,1,0,NULL); anim(dotbar,ax_opa,LV_OPA_COVER,0,400,1600,lv_anim_path_ease_in_out);
  }
  if(page==GW_PAGE_CHAT){ lv_obj_add_flag(root,LV_OBJ_FLAG_HIDDEN); char b[8]; snprintf(b,sizeof b,"%02d:%02d",s->hh,s->mm); lv_label_set_text(c_time,b); return; }
  lv_obj_remove_flag(root,LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(root);
  if(enter){ for(int i=1;i<GW_NPAGES;i++) (i==page)?lv_obj_remove_flag(pg[i],LV_OBJ_FLAG_HIDDEN):lv_obj_add_flag(pg[i],LV_OBJ_FLAG_HIDDEN);
    lv_obj_t* o=pg[page]; int off=(prev<0||!fade)?0:40*dir;
    lv_obj_set_x(o,0); lv_obj_set_style_opa(o,LV_OPA_COVER,0); if(off){ anim(o,ax_x,off,0,200,0,NULL); }
    prog=1000; /* no sweep on enter: one crisp full redraw (sweeps cost ~120ms/frame over QSPI) */ dir=1; }
  char b[96]; static const char* WE[]={"SUN","MON","TUE","WED","THU","FRI","SAT"};
  if(page==GW_PAGE_FACE){
    on_rim(f_sec,222,s->ss*6);
    snprintf(b,sizeof b,"%s %d",WE[s->wday%7],s->day); lv_label_set_text(f_date,b);
    snprintf(b,sizeof b,"%02d:%02d",s->hh,s->mm); lv_label_set_text(f_time,b);
    t_bat=s->battery/100.f; uint32_t bc=s->charging?K_GRN:(s->battery<=20?K_RED:(s->battery<=40?K_YEL:K_GRN));
    lv_obj_set_style_arc_color(f_g[0],col(bc),LV_PART_INDICATOR);
    if(prog>=1000){ arc_set(f_g[0],135,270,t_bat); snprintf(b,sizeof b,"%d",s->battery); lv_label_set_text(f_gv[0],b); }
    lv_obj_set_style_text_color(f_gv[0],col(s->battery<=20?K_RED:K_TXT),0);
    arc_set(f_g[1],135,270,s->n_notes>0?(s->n_notes>=3?1.f:s->n_notes/3.f):0);
    snprintf(b,sizeof b,I_BELL "%d",s->n_notes); lv_label_set_text(f_gv[1],b); lv_obj_set_style_text_color(f_gv[1],col(s->n_notes?K_ORG:K_SEC),0);
    // link: left half = Wi-Fi, right half = phone BLE
    arc_set(f_g[2],135,132,s->wifi?1:0); lv_arc_set_bg_angles(f_g[2],135,405);
    lv_arc_set_bg_angles(f_glink2,273,405); if(s->ble){ lv_obj_set_style_arc_opa(f_glink2,LV_OPA_COVER,LV_PART_INDICATOR); lv_arc_set_angles(f_glink2,273,405);} else lv_obj_set_style_arc_opa(f_glink2,LV_OPA_TRANSP,LV_PART_INDICATOR);
    lv_label_set_text(f_gv[2],s->ble?I_BT:I_WIFI); lv_obj_set_style_text_color(f_gv[2],col(s->ble?K_BLUE:(s->wifi?K_CYAN:K_TER)),0);
  } else if(page==GW_PAGE_RIDE){
    bool has=s->speed&&s->speed[0]; float v=0; if(has) sscanf(s->speed,"%f",&v);
    if(enter){ a_spd=v; t_spd=v; } else if(v!=t_spd){ anim(&t_spd,ax_spd,(int)(a_spd*10),(int)(v*10),400,0,NULL); t_spd=v; }
    if(prog>=1000) apply_prog();
    if(has){ snprintf(b,sizeof b,"%d",(int)(v+0.5f)); lv_label_set_text(r_speed,b); lv_obj_set_style_text_color(r_speed,col(K_TXT),0);}
    else { lv_label_set_text(r_speed,"--"); lv_obj_set_style_text_color(r_speed,col(K_TER),0); }
    snprintf(b,sizeof b,"%d:%02d",s->ride_min/60,s->ride_min%60); lv_label_set_text(r_f[0],b);
    if(s->nav_active&&s->nav_eta&&s->nav_eta[0]){ lv_label_set_text(r_f[1],s->nav_eta); lv_label_set_text(r_fl[1],"ETA"); }
    else { snprintf(b,sizeof b,"%.1f",s->ride_km); lv_label_set_text(r_f[1],b); lv_label_set_text(r_fl[1],"KM"); }
    lv_label_set_text(r_hint,s->ble?"等待手机 GPS":"打开 GrokWatch App"); lv_obj_set_style_text_color(r_hint,col(s->ble?K_SEC:K_ORG),0);
    has?lv_obj_add_flag(r_hint,LV_OBJ_FLAG_HIDDEN):lv_obj_remove_flag(r_hint,LV_OBJ_FLAG_HIDDEN);
  } else if(page==GW_PAGE_NOTES){
    bool any=s->n_notes>0; n_src=s->notes; n_cnt=s->n_notes;
    snprintf(b,sizeof b,"通知  %d",s->n_notes); lv_label_set_text(n_head,b);
    for(int i=0;i<NROW;i++){ if(i<s->n_notes){ const gw_note_t* n=&s->notes[i]; const char* ic; uint32_t c; app_style(n->app,&ic,&c);
        lv_label_set_text(n_icl[i],ic); lv_obj_set_style_bg_color(n_ico[i],col(c),0);
        lv_label_set_text(n_app[i],n->app); lv_label_set_text(n_tm[i],n->time);
        if(n->title[0]){ lv_label_set_text(n_t[i],n->title); lv_obj_remove_flag(n_t[i],LV_OBJ_FLAG_HIDDEN); lv_obj_set_height(n_b[i],26); }
        else { lv_obj_add_flag(n_t[i],LV_OBJ_FLAG_HIDDEN); lv_obj_set_height(n_b[i],52); }
        lv_label_set_text(n_b[i],n->body[0]?n->body:" "); lv_obj_remove_flag(n_row[i],LV_OBJ_FLAG_HIDDEN); }
      else lv_obj_add_flag(n_row[i],LV_OBJ_FLAG_HIDDEN); }
    if(s->n_notes>NROW){ snprintf(b,sizeof b,"还有 %d 条较早的通知",s->n_notes-NROW); lv_label_set_text(n_more2,b); lv_obj_remove_flag(n_more2,LV_OBJ_FLAG_HIDDEN); }
    else lv_obj_add_flag(n_more2,LV_OBJ_FLAG_HIDDEN);
    if(enter) lv_obj_scroll_to_y(n_list,0,LV_ANIM_OFF);
    for(int i=0;i<3;i+=2) any?lv_obj_add_flag(n_empty[i],LV_OBJ_FLAG_HIDDEN):lv_obj_remove_flag(n_empty[i],LV_OBJ_FLAG_HIDDEN);
    any?lv_obj_remove_flag(n_list,LV_OBJ_FLAG_HIDDEN):lv_obj_add_flag(n_list,LV_OBJ_FLAG_HIDDEN);
  } else if(page==GW_PAGE_NAV){
    lv_obj_t* act[]={v_img,v_dist,v_dir,v_meta};
    for(int i=0;i<4;i++) s->nav_active?lv_obj_remove_flag(act[i],LV_OBJ_FLAG_HIDDEN):lv_obj_add_flag(act[i],LV_OBJ_FLAG_HIDDEN);
    for(int i=0;i<4;i+=(i==0?2:1)) s->nav_active?lv_obj_add_flag(v_idle[i],LV_OBJ_FLAG_HIDDEN):lv_obj_remove_flag(v_idle[i],LV_OBJ_FLAG_HIDDEN);
    if(s->nav_active){
      if(s->nav_icon&&(!v_has||memcmp(v_last,s->nav_icon,288))){ memcpy(v_last,s->nav_icon,288); v_has=true;
        for(int i=0;i<48*48;i++) v_px[i]=(s->nav_icon[i/8]>>(7-(i%8))&1)?0xFF:0;
        memset(&v_dsc,0,sizeof v_dsc); v_dsc.header.cf=LV_COLOR_FORMAT_A8; v_dsc.header.w=48; v_dsc.header.h=48; v_dsc.header.stride=48;
        v_dsc.data=v_px; v_dsc.data_size=sizeof v_px; lv_image_set_src(v_img,&v_dsc); lv_obj_invalidate(v_img);
        anim(v_img,ax_img,0,LV_OPA_COVER,220,0,NULL); } // arrow crossfade-in on turn change
      { char d[32]; const char* src=s->nav_dist?s->nav_dist:""; int j=0;
        for(const char* q=src; *q && j<26; ){ if(!strncmp(q,"公里",6)||!strncmp(q,"千米",6)){ j+=snprintf(d+j,sizeof d-j,(j&&d[j-1]==' ')?"km":" km"); q+=6; } else if(!strncmp(q,"米",3)){ j+=snprintf(d+j,sizeof d-j,(j&&d[j-1]==' ')?"m":" m"); q+=3; }
          else if((unsigned char)*q<0x80){ if(!(*q==' '&&j&&d[j-1]==' ')) d[j++]=*q; q++; } else q++; }
        d[j]=0; lv_label_set_text(v_dist,d); } lv_label_set_text(v_dir,s->nav_dir?s->nav_dir:"");
      snprintf(b,sizeof b,"%s  " "\xC2\xB7" "  ETA %s",s->nav_dur?s->nav_dur:"",s->nav_eta?s->nav_eta:""); lv_label_set_text(v_meta,b);
    } else {
      lv_obj_set_style_border_color(v_idle[0],col(s->ble?K_ORG:K_TER),0); lv_obj_set_style_border_opa(v_idle[0],s->ble?LV_OPA_80:LV_OPA_COVER,0);
      lv_obj_set_style_text_color(v_idle[1],col(s->ble?K_ORG:K_SEC),0);
      lv_label_set_text(v_idle[2],s->ble?"等待导航":"等待手机连接");
      lv_label_set_text(v_idle[3],s->ble?"START IN AMAP / MAPS":"OPEN GROKWATCH APP"); }
  }
}
