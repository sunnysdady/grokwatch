// GrokWatch "Ultra" — Apple Watch Ultra (Wayfinder / Modular Ultra) inspired, round 466x466 AMOLED.
// Pure LVGL 9: shared by firmware and the host preview renderer.
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#include "gw_faces.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
LV_FONT_DECLARE(gu_hero) LV_FONT_DECLARE(gu_num) LV_FONT_DECLARE(gu_sm) LV_FONT_DECLARE(gu_lab) LV_FONT_DECLARE(gu_icon) LV_FONT_DECLARE(gz_20)
LV_FONT_DECLARE(gn_hero) LV_FONT_DECLARE(gn_dist) LV_FONT_DECLARE(gn_unit) LV_FONT_DECLARE(gn_mid) LV_FONT_DECLARE(gn_lab) LV_FONT_DECLARE(gn_hour)  // Nunito, HomeView mockup faces
// RAM copies of the compact CJK fonts so a runtime fallback (full CJK font) can cover rare characters
static lv_font_t f20_rt, f21_rt; static bool f_init=false;
const lv_font_t* gw_f20(void){ if(!f_init){ f20_rt=gz_20; f21_rt=gz_20; /* size: gz_21b (408 KB) dropped, see docs/BUILD.md */ f_init=true; } return &f20_rt; }
const lv_font_t* gw_f21(void){ gw_f20(); return &f21_rt; }
void gw_faces_set_fallback(const lv_font_t* f){ gw_f20(); f20_rt.fallback=f; f21_rt.fallback=f; }
const lv_font_t* gw_f_status(void){ gw_f20(); return f20_rt.fallback?f20_rt.fallback:&f21_rt; }  // full UI text font (30 px on device)
#define F20 gw_f20()
#define F21 gw_f21()
#define I_WIFI "\xEF\x87\xAB"
#define I_BT   "\xEF\x8A\x94"
#define I_NAV  "\xEF\x84\xA4"
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
// palette (watchOS dark): pure black, no frame -- content meets the round glass on its own. Neutral greys, mint accent (K_ORG keeps its name).
enum { K_BG=0x000000, K_TXT=0xF4F6F2, K_SEC=0xAEAEB2, K_TER=0x636366, K_CARD=0x1C1C1E, K_CARD2=0x2C2C2E, K_ORG=0xE8EFC4, K_GRN=0x30D158, K_RED=0xFF453A, K_CYAN=0x64D2FF, K_BLUE=0x0A84FF, K_YEL=0xFFD60A };
// Apple system colours (dark appearance), used only where watchOS / Apple Home colour a function: second hand, CC icons and on-states.
enum { A_ORANGE=0xFF9F0A, A_YELLOW=0xFFD60A, A_PINK=0xFF375F, A_INDIGO=0x5E5CE6 };
static bool is_accent(uint32_t h){ return h==A_ORANGE||h==A_YELLOW||h==A_PINK||h==A_INDIGO; }
static bool night=false, fade=true;
// Night: like iPhone StandBy at night -- not red. Low-saturation warm white at ~55 % level; accents keep a hint of hue.
static lv_color_t col(uint32_t h){ if(!night) return lv_color_hex(h);
  if(is_accent(h)) return lv_color_make(((h>>16)&255)*52/100,((h>>8)&255)*52/100,(h&255)*52/100); // night: dim to 52 %, keep hue
  if(h==K_BG) return lv_color_hex(0x000000); if(h==K_CARD) return lv_color_hex(0x1C1814); if(h==K_CARD2) return lv_color_hex(0x28221C);
  int r=(h>>16)&255,g=(h>>8)&255,b=h&255; int l=(r*77+g*150+b*29)>>8;
  int R=(r*3+l*7)/10, G=(g*3+l*7)/10, B=(b*3+l*7)/10;            // 70 % desaturate
  return lv_color_make(R*64/100, G*57/100, B*49/100); }           // dim + slightly warm white

static lv_obj_t *root,*pg[GW_NPAGES],*dotbar,*dots[GW_NCORE],*chat,*sheet,*sheet_t,*sheet_b;
static int cur=-1, dir=1;
// face
static lv_obj_t *r_row,*r_dot,*r_dec; static int r_iw;
static int f_ss=-1; static float f_sa=-1, s_from, s_span=6.f;  // second hand: last second, drawn angle (deg)
static lv_obj_t *f_sec,*f_date,*f_time,*f_g[3],*f_gv[3],*f_glink2,*f_hands,*f_pill,*f_km; static int f_hh=-1,f_mm=-1;
static lv_obj_t *r_card[2],*v_unit,*v_card,*v_row;
// ride
static lv_obj_t *r_arc,*r_speed,*r_unit,*r_f[2],*r_fl[2],*r_hint;
// modal sheet (power menu, ride summary)
static lv_obj_t *sh_scroll,*sh_app,*sh_t,*sh_b;
static lv_obj_t *cc,*cc_tile[GW_CC_N],*cc_ic[GW_CC_N],*cc_lb[GW_CC_N],*cc_val[GW_CC_N],*cc_stat; static void (*cc_cb)(int);
// nav
static lv_obj_t *v_img,*v_dist,*v_dir,*v_meta,*v_idle[4],*v_lt,*v_ltd,*v_ltn,*v_vec;
static char v_turn[4]; static bool v_lt_on; static float v_k=1.f;
static lv_image_dsc_t v_dsc; static uint8_t* v_px;   /* 96x96 A8, PSRAM */ static uint8_t v_last[288]; static bool v_has=false;
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
  lv_obj_t* s=lv_scale_create(p); lv_obj_set_size(s,446,446); lv_obj_center(s); lv_scale_set_mode(s,LV_SCALE_MODE_ROUND_INNER);
  if(mode==1){ lv_scale_set_total_tick_count(s,61); lv_scale_set_major_tick_every(s,10); lv_scale_set_angle_range(s,270); lv_scale_set_rotation(s,135); lv_scale_set_range(s,0,60); }
  else { lv_scale_set_total_tick_count(s,mode==2?72:60); lv_scale_set_major_tick_every(s,mode==2?18:5); lv_scale_set_angle_range(s,mode==2?355:354); lv_scale_set_rotation(s,270);
    lv_scale_set_text_src(s,mode==2?L_CMP:L_MIN); }
  lv_scale_set_label_show(s,false);
  lv_obj_set_style_length(s,8,LV_PART_ITEMS); lv_obj_set_style_line_width(s,2,LV_PART_ITEMS); lv_obj_set_style_line_color(s,col(dim?K_CARD2:0x5A5A5E),LV_PART_ITEMS);
  lv_obj_set_style_length(s,16,LV_PART_INDICATOR); lv_obj_set_style_line_width(s,3,LV_PART_INDICATOR); lv_obj_set_style_line_color(s,col(dim?K_TER:0xC7C7CC),LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(s,0,LV_PART_MAIN); lv_obj_set_style_bg_opa(s,LV_OPA_TRANSP,0); lv_obj_set_style_border_width(s,0,0);
  lv_obj_set_style_text_font(s,&gu_lab,LV_PART_INDICATOR); lv_obj_set_style_text_color(s,col(K_SEC),LV_PART_INDICATOR); lv_obj_set_style_pad_radial(s,8,LV_PART_INDICATOR);
  lv_obj_remove_flag(s,LV_OBJ_FLAG_CLICKABLE);
  if(0){ lv_obj_t* m=lv_obj_create(p); lv_obj_remove_style_all(m); lv_obj_set_size(m,6,20); lv_obj_set_style_radius(m,3,0);
    lv_obj_set_style_bg_opa(m,LV_OPA_COVER,0); lv_obj_set_style_bg_color(m,col(K_ORG),0); lv_obj_align(m,LV_ALIGN_TOP_MID,0,4); }
}
// circular complication (Modular Ultra)
static lv_obj_t* gauge(lv_obj_t* p,int x,int y,uint32_t c,lv_obj_t** val){ lv_obj_t* g=arc(p,84,7,K_CARD2,c); lv_obj_align(g,LV_ALIGN_CENTER,x,y);
  *val=label(p,&gu_sm,K_TXT,x,y); return g; }
// Thin analog hands (HomeView A): two round-capped lines + hub, redrawn only when the minute changes.
enum { SEC_LEN=200, SEC_TAIL=30 };
static void sec_pts(float deg,int cx,int cy,lv_point_precise_t* tip,lv_point_precise_t* tail){ float r=deg*0.01745329f, sn=sinf(r), cs=cosf(r);
  tip->x=cx+SEC_LEN*sn+.5f; tip->y=cy-SEC_LEN*cs+.5f; tail->x=cx-SEC_TAIL*sn+.5f; tail->y=cy+SEC_TAIL*cs+.5f; }
// Invalidate only the second hand's own bounding box (old + new pose), never the whole face.
static void sec_inval(float deg){ if(deg<0||!f_hands) return; lv_area_t c; lv_obj_get_coords(f_hands,&c); lv_point_precise_t a,b;
  sec_pts(deg,(c.x1+c.x2)/2,(c.y1+c.y2)/2,&a,&b);
  enum { SEG=8 };  // 8 small boxes along the hand: a diagonal costs ~1/5 of its bounding box
  for(int k=0;k<SEG;k++){ float t0=(float)k/SEG, t1=(float)(k+1)/SEG;
    int x0=b.x+(a.x-b.x)*t0, y0=b.y+(a.y-b.y)*t0, x1=b.x+(a.x-b.x)*t1, y1=b.y+(a.y-b.y)*t1;
    lv_area_t r={LV_MIN(x0,x1)-3,LV_MIN(y0,y1)-3,LV_MAX(x0,x1)+3,LV_MAX(y0,y1)+3}; lv_obj_invalidate_area(f_hands,&r); }
  lv_area_t hub={(c.x1+c.x2)/2-6,(c.y1+c.y2)/2-6,(c.x1+c.x2)/2+6,(c.y1+c.y2)/2+6}; lv_obj_invalidate_area(f_hands,&hub); }
// 2.6.15: minute change used to invalidate the whole 466x466 hands box = a full-screen redraw (~0.9 s on device) every
// minute, which froze the second hand. Invalidate only small boxes along the hour/minute hands (old + new pose).
static void seg_inval(int cx,int cy,int deg,int len,int pad){ for(int k=0;k<8;k++){
    int x0=cx+((len*k/8)*lv_trigo_sin(deg)>>15), y0=cy-((len*k/8)*lv_trigo_cos(deg)>>15), x1=cx+((len*(k+1)/8)*lv_trigo_sin(deg)>>15), y1=cy-((len*(k+1)/8)*lv_trigo_cos(deg)>>15);
    lv_area_t r={LV_MIN(x0,x1)-pad,LV_MIN(y0,y1)-pad,LV_MAX(x0,x1)+pad,LV_MAX(y0,y1)+pad}; lv_obj_invalidate_area(f_hands,&r); } }
static void hands_inval(void){ if(!f_hands||f_hh<0) return; lv_area_t c; lv_obj_get_coords(f_hands,&c); int cx=(c.x1+c.x2)/2, cy=(c.y1+c.y2)/2;
  seg_inval(cx,cy,((f_hh%12)*60+f_mm)/2,118,6); seg_inval(cx,cy,f_mm*6,196,5);
  lv_area_t hub={cx-8,cy-8,cx+8,cy+8}; lv_obj_invalidate_area(f_hands,&hub); }
static void ax_sec(void* o,int32_t v){ (void)o; sec_inval(f_sa); f_sa=s_from+v*s_span*0.001f; if(f_sa>=360.f) f_sa-=360.f; sec_inval(f_sa); }
#define V_USCALE 144  /* 144/256 = 0.56 = mockup 54/96 */
static void nav_layout(void){ lv_obj_update_layout(v_dist); lv_obj_update_layout(v_unit);
  int wn=lv_obj_get_width(v_dist), wu=lv_obj_get_width(v_unit), gap=17, x0=(400-(wn+gap+wu))/2;
  int du=(gn_dist.line_height-gn_dist.base_line)-(gn_unit.line_height-gn_unit.base_line);   // put both baselines on one line
  lv_obj_set_pos(v_dist,x0,0); lv_obj_set_pos(v_unit,x0+wn+gap,du); }
static void hands_draw(lv_event_t* e){ lv_layer_t* L=lv_event_get_layer(e); lv_area_t a; lv_obj_get_coords(f_hands,&a);
  int cx=(a.x1+a.x2)/2, cy=(a.y1+a.y2)/2; if(f_hh<0) return;
  int ha=((f_hh%12)*60+f_mm)/2, ma=f_mm*6;  // degrees, 0 = 12 o'clock
  lv_draw_line_dsc_t l; lv_draw_line_dsc_init(&l); l.color=col(K_TXT); l.round_start=1; l.round_end=1;
  l.p1.x=cx; l.p1.y=cy; l.width=6; l.p2.x=cx+((118*lv_trigo_sin(ha))>>15); l.p2.y=cy-((118*lv_trigo_cos(ha))>>15); lv_draw_line(L,&l);
  l.width=3; l.p2.x=cx+((196*lv_trigo_sin(ma))>>15); l.p2.y=cy-((196*lv_trigo_cos(ma))>>15); lv_draw_line(L,&l);
  lv_draw_rect_dsc_t r; lv_draw_rect_dsc_init(&r); r.radius=LV_RADIUS_CIRCLE; r.bg_opa=LV_OPA_COVER; r.bg_color=col(K_TXT);
  lv_area_t h={cx-8,cy-8,cx+8,cy+8}; lv_draw_rect(L,&r,&h);
  if(f_sa>=0){ lv_draw_line_dsc_t q; lv_draw_line_dsc_init(&q); q.color=col(A_ORANGE); q.width=2; q.round_start=1; q.round_end=1;
    sec_pts(f_sa,cx,cy,&q.p1,&q.p2); lv_draw_line(L,&q); }
  r.bg_color=col(A_ORANGE); lv_area_t h3={cx-5,cy-5,cx+5,cy+5}; lv_draw_rect(L,&r,&h3);
  r.bg_color=col(K_BG); lv_area_t h2={cx-2,cy-2,cx+2,cy+2}; lv_draw_rect(L,&r,&h2); }

static lv_obj_t* pill(lv_obj_t* p,int w,int h,int x,int y){ lv_obj_t* c=lv_obj_create(p); lv_obj_remove_style_all(c); lv_obj_set_size(c,w,h);
  lv_obj_set_style_radius(c,h/2>36?36:h/2,0); lv_obj_set_style_bg_opa(c,LV_OPA_COVER,0); lv_obj_set_style_bg_color(c,col(K_CARD),0);
  lv_obj_set_style_border_width(c,1,0); lv_obj_set_style_border_color(c,col(K_CARD2),0); lv_obj_set_style_border_opa(c,140,0);  // 2.6.14: hairline edge = glass card
  lv_obj_remove_flag(c,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE); lv_obj_align(c,LV_ALIGN_CENTER,x,y); return c; }
static void sheet_click(lv_event_t* e){ (void)e; gw_faces_sheet(NULL,NULL); }
static lv_obj_t *t_head,*t_row[GW_TODO_ROWS],*t_box[GW_TODO_ROWS],*t_txt[GW_TODO_ROWS],*t_more,*t_empty[3];
static void (*todo_cb)(int)=NULL;
void gw_faces_set_todo_cb(void (*f)(int)){ todo_cb=f; }
static void todo_click(lv_event_t* e){ int i=(int)(intptr_t)lv_event_get_user_data(e); if(todo_cb) todo_cb(i); }
// app -> icon/colour (Chinese app labels as sent by the phone)
static void app_style(const char* a,const char** ic,uint32_t* c){ *ic=I_MSG; *c=K_ORG;
  if(strstr(a,"微信")||strstr(a,"WeChat")){ *ic=I_WX; *c=K_GRN; }
  else if(strstr(a,"信息")||strstr(a,"短信")||strstr(a,"Messages")){ *ic=I_MSG; *c=K_GRN; }
  else if(strstr(a,"邮件")||strstr(a,"Mail")||strstr(a,"Gmail")||strstr(a,"Outlook")){ *ic=I_MAIL; *c=K_BLUE; }
  else if(strstr(a,"电话")||strstr(a,"Phone")){ *ic=I_TEL; *c=K_GRN; } }

static void cc_click(lv_event_t* e){ int i=(int)(intptr_t)lv_event_get_user_data(e); if(cc_cb) cc_cb(i); }
static void cc_bg_click(lv_event_t* e){ (void)e; }
void gw_faces_set_night(bool on){ night=on; gw_grok_set_night(on); }
void gw_faces_set_fade(bool on){ fade=on; }
static bool slide_on=true; void gw_faces_set_slide(bool on){ slide_on=on; }
void gw_faces_dir(int d){ dir=d<0?-1:1; }
int gw_faces_page_x(void){ return (cur>0&&cur<GW_NPAGES&&pg[cur])?lv_obj_get_x(pg[cur]):0; }

// ---------- 2.6.12 vector turn arrows: own SDF rasterizer into a 120x120 A8 mask ----------
// Path = turtle (forward / true circular-arc turn, so inner+outer stroke edges are concentric), round caps,
// rounded-tip head (triangle offset by VRR = exact rounded triangle), auto-centred on the ink bbox.
// Coverage per pixel = 3x3 supersampled SDF (each sample box-filtered), i.e. ~1 px AA edge, identical in sim + device.
#define VA 120   /* arrow box, px */
#define VW 18    /* stroke width */
#define VRR 3.f  /* head corner radius */
#include "gw_mem.h"
void lv_image_cache_drop(const void* src);   /* lv_image_cache.h is not in lvgl.h */
typedef struct { uint8_t k; float ax,ay,bx,by,r0,a0,sw; } vprim_t;   // k: 0 seg, 1 arc(center ax,ay radius r0 start a0 sweep sw), 2 tri(a,b,(r0,a0))
static vprim_t vp[12]; static int vn; static float vtx,vty,vth; static uint8_t* v_vpx; static lv_image_dsc_t v_vdsc; static char v_built[4];
static float vminx,vminy,vmaxx,vmaxy;
static void vbb(float x,float y,float pad){ if(x-pad<vminx)vminx=x-pad; if(x+pad>vmaxx)vmaxx=x+pad; if(y-pad<vminy)vminy=y-pad; if(y+pad>vmaxy)vmaxy=y+pad; }
static void vfwd(float l){ float nx=vtx+sinf(vth)*l, ny=vty-cosf(vth)*l; vp[vn++]=(vprim_t){0,vtx,vty,nx,ny,0,0,0}; vbb(vtx,vty,VW/2.f); vbb(nx,ny,VW/2.f); vtx=nx; vty=ny; }
static void vturn(float deg,float R){ float s=deg>0?1.f:-1.f, rx=cosf(vth)*s, ry=sinf(vth)*s; float cx=vtx+rx*R, cy=vty+ry*R;   // centre on the turn side
  float a0=atan2f(vty-cy,vtx-cx), sw=deg*0.0174533f; vp[vn++]=(vprim_t){1,cx,cy,0,0,R,a0,sw};
  for(int i=0;i<=8;i++){ float a=a0+sw*i/8; vbb(cx+cosf(a)*R,cy+sinf(a)*R,VW/2.f); }
  vtx=cx+cosf(a0+sw)*R; vty=cy+sinf(a0+sw)*R; vth+=sw; }
static void vheadp(float len,float hw){ float dx=sinf(vth),dy=-cosf(vth),px=-dy,py=dx; float bx=vtx-dx*2,by=vty-dy*2;   // base overlaps shaft by 2 px
  float T[3][2]={{bx+dx*len,by+dy*len},{bx+px*hw,by+py*hw},{bx-px*hw,by-py*hw}};
  float la=hypotf(T[1][0]-T[2][0],T[1][1]-T[2][1]), lb=hypotf(T[0][0]-T[2][0],T[0][1]-T[2][1]), lc=hypotf(T[0][0]-T[1][0],T[0][1]-T[1][1]), P=la+lb+lc;
  float ix=(la*T[0][0]+lb*T[1][0]+lc*T[2][0])/P, iy=(la*T[0][1]+lb*T[1][1]+lc*T[2][1])/P;
  float ar=fabsf((T[1][0]-T[0][0])*(T[2][1]-T[0][1])-(T[2][0]-T[0][0])*(T[1][1]-T[0][1]))/2, rho=2*ar/P, f=(rho-VRR)/rho;
  for(int i=0;i<3;i++){ T[i][0]=ix+(T[i][0]-ix)*f; T[i][1]=iy+(T[i][1]-iy)*f; vbb(T[i][0],T[i][1],VRR); }
  vp[vn++]=(vprim_t){2,T[0][0],T[0][1],T[1][0],T[1][1],T[2][0],T[2][1],0}; }
static float vseg(float x,float y,float ax,float ay,float bx,float by){ float vx=bx-ax,vy=by-ay,wx=x-ax,wy=y-ay; float h=(wx*vx+wy*vy)/(vx*vx+vy*vy); h=h<0?0:(h>1?1:h); return hypotf(wx-vx*h,wy-vy*h); }
static float vprim(const vprim_t* q,float x,float y){
  if(q->k==0) return vseg(x,y,q->ax,q->ay,q->bx,q->by)-VW/2.f;
  if(q->k==1){ float a=atan2f(y-q->ay,x-q->ax)-q->a0; float t=q->sw>0?a:-a, S=fabsf(q->sw); while(t<0)t+=6.2831853f; while(t>6.2831853f)t-=6.2831853f;
    if(t<=S) return fabsf(hypotf(x-q->ax,y-q->ay)-q->r0)-VW/2.f;
    float e=q->a0+q->sw, d0=hypotf(x-q->ax-cosf(q->a0)*q->r0,y-q->ay-sinf(q->a0)*q->r0), d1=hypotf(x-q->ax-cosf(e)*q->r0,y-q->ay-sinf(e)*q->r0);
    return (d0<d1?d0:d1)-VW/2.f; }
  float P[3][2]={{q->ax,q->ay},{q->bx,q->by},{q->r0,q->a0}}; float d=1e9f; int in=1; float sg=0;
  for(int i=0;i<3;i++){ int j=(i+1)%3; float e=vseg(x,y,P[i][0],P[i][1],P[j][0],P[j][1]); if(e<d)d=e;
    float c=(P[j][0]-P[i][0])*(y-P[i][1])-(P[j][1]-P[i][1])*(x-P[i][0]); if(i==0) sg=c; else if((c>0)!=(sg>0)) in=0; }
  return (in?-d:d)-VRR; }
static void vbuild(const char* t){ vn=0; vtx=0; vty=0; vth=0; vminx=vminy=1e9f; vmaxx=vmaxy=-1e9f; float m=(t[0]=='L'||t[1]=='L')?-1:1;
  const float HL=30,HW=27;
  if(!strcmp(t,"S")){ vfwd(62); vheadp(HL,HW); }
  else if(t[0]=='L'||t[0]=='R'){ vfwd(40); vturn(90*m,22); vfwd(12); vheadp(HL,HW); }
  else if(t[0]=='S'){ vfwd(40); vturn(45*m,30); vfwd(10); vheadp(HL,HW); }
  else if(t[0]=='H'){ vfwd(50); vturn(135*m,17); vfwd(22); vheadp(HL-2,HW-2); }   // sharp: tight concentric bend, head back down
  else if(t[0]=='U'){ vfwd(40); vturn(180*m,21); vfwd(8); vheadp(HL-2,HW-1); }     // UL: up, bend LEFT, head down on the left (CN U-turn)
  else if(!strcmp(t,"A")){ vp[vn++]=(vprim_t){1,0,0,0,0,30,0,6.2831853f}; vp[vn++]=(vprim_t){0,0,0,0,0,0,0,0}; vbb(0,0,39); }
  float ox=VA/2.f-(vminx+vmaxx)/2, oy=VA/2.f-(vminy+vmaxy)/2;   // centre the ink bbox
  if(!v_vpx) v_vpx=gw_psram_calloc(VA*VA); if(!v_vpx) return;
  for(int py=0;py<VA;py++) for(int px=0;px<VA;px++){ float cov=0;
    for(int sy=0;sy<3;sy++) for(int sx=0;sx<3;sx++){ float x=px+(sx+.5f)/3-ox, y=py+(sy+.5f)/3-oy, d=1e9f;
      for(int i=0;i<vn;i++){ float e; if(!strcmp(t,"A")&&i==1) e=hypotf(x,y)-12.5f; else e=vprim(&vp[i],x,y); if(e<d)d=e; }
      if(!strcmp(t,"A")&&vn==2){ float ring=fabsf(hypotf(x,y)-30)-6; float dot=hypotf(x,y)-12.5f; d=ring<dot?ring:dot; }
      float c=0.5f-d*3; cov+=c<0?0:(c>1?1:c); }
    v_vpx[py*VA+px]=(uint8_t)(cov*255/9+.5f); }
  memset(&v_vdsc,0,sizeof v_vdsc); v_vdsc.header.magic=LV_IMAGE_HEADER_MAGIC; v_vdsc.header.cf=LV_COLOR_FORMAT_A8; v_vdsc.header.w=VA; v_vdsc.header.h=VA; v_vdsc.header.stride=VA;
  v_vdsc.data=v_vpx; v_vdsc.data_size=VA*VA; lv_image_cache_drop(&v_vdsc); snprintf(v_built,sizeof v_built,"%s",t); }
static void vdraw(lv_event_t* e){ lv_layer_t* L=lv_event_get_layer(e); lv_area_t a; lv_obj_get_coords(v_vec,&a);
  if(strcmp(v_built,v_turn)) vbuild(v_turn); if(!v_vpx||strcmp(v_built,v_turn)) return;
  lv_draw_image_dsc_t d; lv_draw_image_dsc_init(&d); d.src=&v_vdsc; d.recolor=col(K_TXT); d.recolor_opa=LV_OPA_COVER; d.antialias=1;
  d.scale_x=d.scale_y=(int32_t)(v_k*256+.5f); d.pivot.x=VA/2; d.pivot.y=VA/2; lv_area_t r={a.x1,a.y1,a.x1+VA-1,a.y1+VA-1}; lv_draw_image(L,&d,&r); }
static bool vknown(const char* t){ static const char* K[]={"S","L","R","SL","SR","HL","HR","UL","UR","A"}; for(int i=0;i<10;i++) if(t&&!strcmp(t,K[i])) return true; return false; }
// 1-bit 48x48 -> 96x96 A8: 3x3 soften (rounds stair steps) -> 2x bilinear -> steep re-threshold (narrow AA edge, not blur)
static void smooth_icon(const uint8_t* ic){ static uint8_t* s; if(!s) s=gw_psram_calloc(48*48); if(!v_px) v_px=gw_psram_calloc(96*96); if(!s||!v_px) return;
  for(int y=0;y<48;y++) for(int x=0;x<48;x++){ int acc=0; for(int dy=-1;dy<=1;dy++) for(int dx=-1;dx<=1;dx++){ int xx=x+dx, yy=y+dy;
      if(xx<0||yy<0||xx>47||yy>47) continue; int b=(ic[(yy*48+xx)/8]>>(7-((yy*48+xx)%8)))&1; acc+=b*((dx==0&&dy==0)?4:(dx==0||dy==0)?2:1); }
    s[y*48+x]=(uint8_t)(acc*255/16); }
  for(int y=0;y<96;y++) for(int x=0;x<96;x++){ float fx=(x+.5f)/2-.5f, fy=(y+.5f)/2-.5f; int x0=(int)floorf(fx), y0=(int)floorf(fy); float ax=fx-x0, ay=fy-y0;
    #define P(X,Y) ((X)<0||(Y)<0||(X)>47||(Y)>47?0:s[(Y)*48+(X)])
    float v=(1-ax)*(1-ay)*P(x0,y0)+ax*(1-ay)*P(x0+1,y0)+(1-ax)*ay*P(x0,y0+1)+ax*ay*P(x0+1,y0+1);
    #undef P
    int o=(int)((v-128)*3.5f+128); v_px[y*96+x]=o<0?0:(o>255?255:o); } }   // upscale + threshold: shape from the soft field, AA edge only ~1.2 px

// 2.6.35: face background photo from the app (gw_bg.cc). Lowest child of the FACE page; dimmed further at night.
static const void* bg_px; static lv_obj_t* f_bg; static lv_image_dsc_t bg_dsc;
static void bg_apply(void){ if(!pg[GW_PAGE_FACE]) return;
  if(!bg_px){ if(f_bg){ lv_obj_delete(f_bg); f_bg=NULL; } return; }
  if(!f_bg){ f_bg=lv_image_create(pg[GW_PAGE_FACE]); lv_obj_move_to_index(f_bg,0); lv_obj_center(f_bg); lv_obj_remove_flag(f_bg,LV_OBJ_FLAG_CLICKABLE); }
  memset(&bg_dsc,0,sizeof bg_dsc); bg_dsc.header.magic=LV_IMAGE_HEADER_MAGIC; bg_dsc.header.cf=LV_COLOR_FORMAT_RGB565; bg_dsc.header.w=466; bg_dsc.header.h=466;
  bg_dsc.header.stride=466*2; bg_dsc.data=bg_px; bg_dsc.data_size=466*466*2; lv_image_cache_drop(&bg_dsc); lv_image_set_src(f_bg,NULL); lv_image_set_src(f_bg,&bg_dsc);
  lv_obj_set_style_image_recolor(f_bg,lv_color_black(),0); lv_obj_set_style_image_recolor_opa(f_bg,night?LV_OPA_60:LV_OPA_TRANSP,0); }
void gw_faces_set_bg(const void* px){ bg_px=px; bg_apply(); }

void gw_faces_create(lv_obj_t* parent){
  if(root){ lv_obj_delete(root); lv_obj_delete(chat); lv_obj_delete(dotbar); lv_obj_delete(sheet); lv_obj_delete(cc); f_bg=NULL; root=NULL; cur=-1; }
  // chat-page decoration sits right above the native container (index 1), under xiaozhi's emoji/status labels
  chat=box(parent); lv_obj_move_to_index(chat,1);
  c_time=label(chat,&gu_sm,K_ORG,0,-192); lv_obj_set_style_text_letter_space(c_time,1,0);  // -192: clear of the Grok status line (-152)
  root=box(parent); lv_obj_set_style_bg_color(root,col(K_BG),0); lv_obj_set_style_radius(root,LV_RADIUS_CIRCLE,0); lv_obj_set_style_bg_opa(root,LV_OPA_COVER,0);
  for(int i=1;i<GW_NPAGES;i++){ pg[i]=box(root); lv_obj_add_flag(pg[i],LV_OBJ_FLAG_HIDDEN); }
  // ---- FACE: Wayfinder bezel + bold numerals + 3 circular complications
  lv_obj_t* p=pg[GW_PAGE_FACE]; bezel(p,0,false);
  f_sec=disc(p,10,K_ORG); lv_obj_add_flag(f_sec,LV_OBJ_FLAG_HIDDEN);
  f_date=label(p,F20,K_SEC,0,-83);
  f_pill=pill(p,180,50,0,78); f_km=label(f_pill,F20,K_TXT,0,0);
  f_time=label(p,&gu_hero,K_TXT,0,-22); lv_obj_set_style_text_letter_space(f_time,-4,0);
  f_g[0]=gauge(p,-100,100,K_GRN,&f_gv[0]); f_g[1]=gauge(p,0,112,K_ORG,&f_gv[1]); f_g[2]=gauge(p,100,100,K_CYAN,&f_gv[2]);
  f_glink2=arc(p,84,7,K_CARD2,K_BLUE); lv_obj_align(f_glink2,LV_ALIGN_CENTER,100,100); lv_obj_set_style_arc_opa(f_glink2,LV_OPA_TRANSP,LV_PART_MAIN);
  { lv_obj_t* hid[]={f_time,f_g[0],f_g[1],f_g[2],f_gv[0],f_gv[1],f_gv[2],f_glink2}; for(int i=0;i<8;i++) lv_obj_add_flag(hid[i],LV_OBJ_FLAG_HIDDEN); } // digital Ultra face retired
  for(int i=3;i<=12;i+=3){ bool q=true; char nb[4]; snprintf(nb,sizeof nb,"%d",i);   // gu_sm covers 0x20-0x7E: every digit present
    int rr=i==6?166:176; lv_obj_t* n=label(p,&gn_hour,q?K_TXT:K_SEC,(rr*lv_trigo_sin(i*30))>>15,-((rr*lv_trigo_cos(i*30))>>15)); lv_label_set_text(n,nb); }
  f_hands=box(p); lv_obj_add_event_cb(f_hands,hands_draw,LV_EVENT_DRAW_MAIN,NULL); f_hh=f_mm=-1; f_ss=-1; f_sa=-1;
  bg_apply();   // 2.6.35: re-attach the background photo (night recreate)
  // ---- RIDE: orange speed sweep (minute ring lives on FACE only)
  p=pg[GW_PAGE_RIDE];
  r_arc=arc(p,360,12,K_CARD,K_ORG); lv_obj_center(r_arc); lv_obj_add_flag(r_arc,LV_OBJ_FLAG_HIDDEN);   // HomeView: no dial
  lv_obj_t* h=label(p,F20,K_SEC,0,-121); lv_label_set_text(h,"速度");
  r_row=lv_obj_create(p); lv_obj_remove_style_all(r_row); lv_obj_set_size(r_row,LV_SIZE_CONTENT,LV_SIZE_CONTENT); lv_obj_align(r_row,LV_ALIGN_CENTER,0,-34);
  lv_obj_set_flex_flow(r_row,LV_FLEX_FLOW_ROW); lv_obj_set_flex_align(r_row,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_END,LV_FLEX_ALIGN_END); lv_obj_set_style_pad_column(r_row,5,0);
  lv_obj_remove_flag(r_row,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
  r_speed=lv_label_create(r_row); lv_obj_set_style_text_font(r_speed,&gn_hero,0); lv_obj_set_style_text_color(r_speed,col(K_TXT),0); lv_obj_set_style_text_letter_space(r_speed,-2,0);
  r_dot=disc(r_row,16,K_TXT); lv_obj_add_flag(r_dot,LV_OBJ_FLAG_HIDDEN);
  r_dec=lv_label_create(r_row); lv_obj_set_style_text_font(r_dec,&gn_hero,0); lv_obj_set_style_text_color(r_dec,col(K_TXT),0); lv_obj_set_style_text_letter_space(r_dec,-2,0);
  // 2.6.23: speed = integer label (right-aligned, fixed 2-digit cell) + ".d" label (left-aligned, fixed cell), same font
  // so the baseline is shared; the decimal point never moves as digits change (Nunito digits are proportional).
  { int dw=0; for(char c='0';c<='9';c++){ int w=lv_font_get_glyph_width(&gn_hero,c,0); if(w>dw) dw=w; } int pw=lv_font_get_glyph_width(&gn_hero,'.',0);
    r_iw=2*dw-2; lv_obj_set_width(r_speed,r_iw); lv_obj_set_style_text_align(r_speed,LV_TEXT_ALIGN_RIGHT,0);
    lv_obj_set_width(r_dec,pw+dw-2); lv_obj_set_style_text_align(r_dec,LV_TEXT_ALIGN_LEFT,0); lv_obj_set_style_pad_column(r_row,0,0); }
  r_unit=label(p,&gn_lab,K_SEC,0,29); lv_label_set_text(r_unit,"km/h");
  for(int i=0;i<2;i++){ int x=i?77:-77; r_card[i]=pill(p,146,70,x,102); r_f[i]=label(p,&gn_mid,K_TXT,x,90); r_fl[i]=label(p,F20,K_SEC,x,120); lv_label_set_text(r_fl[i],i?"用时":"里程 km"); }
  r_hint=label(p,F20,K_SEC,0,158); lv_label_set_text(r_hint,"打开 GrokWatch App");
  // ---- TODO: voice to-do list (Ultra style). Up to 5 rows, tap = check off.
  p=pg[GW_PAGE_TODO];
  t_head=label(p,F20,K_SEC,0,-150);
  for(int i=0;i<GW_TODO_ROWS;i++){ lv_obj_t* c=lv_obj_create(p); lv_obj_remove_style_all(c); t_row[i]=c; lv_obj_set_size(c,320,48);
    lv_obj_align(c,LV_ALIGN_CENTER,0,-112+i*54); lv_obj_set_style_bg_color(c,col(K_CARD),0); lv_obj_set_style_bg_opa(c,LV_OPA_COVER,0); lv_obj_set_style_radius(c,25,0);
    lv_obj_set_style_border_width(c,1,0); lv_obj_set_style_border_color(c,col(K_CARD2),0); lv_obj_set_style_border_opa(c,140,0);
    lv_obj_add_flag(c,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_GESTURE_BUBBLE); lv_obj_remove_flag(c,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(c,todo_click,LV_EVENT_CLICKED,(void*)(intptr_t)i);
    t_box[i]=disc(c,26,K_CARD); lv_obj_set_style_border_width(t_box[i],3,0); lv_obj_set_style_border_color(t_box[i],col(K_SEC),0); lv_obj_align(t_box[i],LV_ALIGN_LEFT_MID,14,0);
    t_txt[i]=lv_label_create(c); lv_obj_set_style_text_font(t_txt[i],F20,0); lv_obj_set_style_text_color(t_txt[i],col(K_TXT),0);
    lv_obj_set_width(t_txt[i],lv_obj_get_style_width(c,0)-66); lv_label_set_long_mode(t_txt[i],LV_LABEL_LONG_DOT); lv_obj_set_height(t_txt[i],28); lv_obj_set_style_text_line_space(t_txt[i],2,0); lv_obj_align(t_txt[i],LV_ALIGN_LEFT_MID,52,1); }
  t_more=label(p,F20,K_SEC,0,150);
  t_empty[0]=disc(p,112,K_CARD); lv_obj_align(t_empty[0],LV_ALIGN_CENTER,0,-22);
  t_empty[1]=lv_label_create(t_empty[0]); lv_obj_set_style_text_font(t_empty[1],&gu_icon,0); lv_obj_set_style_text_color(t_empty[1],col(K_ORG),0); lv_label_set_text(t_empty[1],I_MIC); lv_obj_center(t_empty[1]);
  t_empty[2]=label(p,F20,K_SEC,0,80); lv_obj_set_width(t_empty[2],300); lv_obj_set_style_text_align(t_empty[2],LV_TEXT_ALIGN_CENTER,0);
  // ---- NAV: big turn arrow
  p=pg[GW_PAGE_NAV];
  v_has=false;  // new image object: force the cached arrow to be re-applied (night toggle recreates pages mid-nav)
  v_img=lv_image_create(p); lv_obj_align(v_img,LV_ALIGN_CENTER,0,-108); lv_image_set_scale(v_img,320); lv_image_set_antialias(v_img,true);
  v_vec=lv_obj_create(p); lv_obj_remove_style_all(v_vec); lv_obj_set_size(v_vec,VA,VA); lv_obj_align(v_vec,LV_ALIGN_CENTER,0,-108); lv_obj_remove_flag(v_vec,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(v_vec,vdraw,LV_EVENT_DRAW_MAIN,NULL); lv_obj_add_flag(v_vec,LV_OBJ_FLAG_HIDDEN); v_turn[0]=0; v_lt_on=false;
  lv_obj_set_style_image_recolor(v_img,col(K_TXT),0); lv_obj_set_style_image_recolor_opa(v_img,LV_OPA_COVER,0);
  // number + unit as two labels on one baseline row: unit readable (26 px) with a 14 px gap
  v_row=lv_obj_create(p); lv_obj_remove_style_all(v_row); lv_obj_set_size(v_row,400,gn_dist.line_height); lv_obj_align(v_row,LV_ALIGN_CENTER,0,8);
  lv_obj_remove_flag(v_row,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
  v_dist=lv_label_create(v_row); lv_obj_set_style_text_font(v_dist,&gn_dist,0); lv_obj_set_style_text_color(v_dist,col(K_TXT),0); lv_obj_set_style_text_letter_space(v_dist,-1,0);
  v_unit=lv_label_create(v_row); lv_obj_set_style_text_font(v_unit,&gn_unit,0); lv_obj_set_style_text_color(v_unit,col(K_SEC),0);
  v_card=pill(p,290,60,0,112);
  v_dir=label(v_card,F21,K_TXT,0,0); lv_obj_set_width(v_dir,260); lv_label_set_long_mode(v_dir,LV_LABEL_LONG_DOT);
  v_meta=label(p,F20,K_SEC,0,168);
  // traffic light: small capsule right of the arrow (same row, never moves other elements); hidden when no data
  v_lt=pill(p,86,36,118,-108); v_ltd=disc(v_lt,12,K_RED); lv_obj_align(v_ltd,LV_ALIGN_LEFT_MID,14,0);
  v_ltn=label(v_lt,F20,K_TXT,0,0); lv_obj_align(v_ltn,LV_ALIGN_LEFT_MID,34,0); lv_obj_add_flag(v_lt,LV_OBJ_FLAG_HIDDEN);
  v_idle[0]=disc(p,140,K_CARD); lv_obj_align(v_idle[0],LV_ALIGN_CENTER,0,-50); lv_obj_set_style_border_width(v_idle[0],3,0);
  v_idle[1]=lv_label_create(v_idle[0]); lv_obj_set_style_text_font(v_idle[1],&gu_icon,0); lv_label_set_text(v_idle[1],I_NAV); lv_obj_center(v_idle[1]);
  v_idle[2]=label(p,F21,K_TXT,0,64); v_idle[3]=label(p,F20,K_SEC,0,102);
  // ---- page dots (top layer: also visible over the native chat page). Transient, like watchOS.
  dotbar=lv_obj_create(lv_layer_top()); lv_obj_remove_style_all(dotbar); lv_obj_set_size(dotbar,140,12); lv_obj_align(dotbar,LV_ALIGN_CENTER,0,196);   // y 426-432: clear of the "6" (ends 412), nav ETA row (ends 408) and the 6 o'clock tick (starts 439)
  lv_obj_set_flex_flow(dotbar,LV_FLEX_FLOW_ROW); lv_obj_set_flex_align(dotbar,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(dotbar,7,0); lv_obj_remove_flag(dotbar,LV_OBJ_FLAG_CLICKABLE);
  for(int i=0;i<GW_NCORE;i++){ dots[i]=disc(dotbar,7,K_TER); lv_obj_set_style_radius(dots[i],4,0); }
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
  cc=box(parent); lv_obj_set_style_radius(cc,LV_RADIUS_CIRCLE,0); lv_obj_set_style_bg_color(cc,col(K_BG),0); lv_obj_set_style_bg_opa(cc,LV_OPA_COVER,0); lv_obj_add_flag(cc,LV_OBJ_FLAG_HIDDEN|LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(cc,cc_bg_click,LV_EVENT_CLICKED,NULL);
  cc_stat=label(cc,F20,K_SEC,0,-150);
  static const char* IC[GW_CC_N]={I_SUN,I_VOL,I_MOON,I_DND}; static const char* LB[GW_CC_N]={"亮度","音量","夜间","勿扰"};
  for(int i=0;i<GW_CC_N;i++){ int x=(i%2)?64:-64, y=(i/2)?80:-56;
    cc_tile[i]=disc(cc,104,K_CARD2); lv_obj_align(cc_tile[i],LV_ALIGN_CENTER,x,y); lv_obj_add_flag(cc_tile[i],LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cc_tile[i],cc_click,LV_EVENT_CLICKED,(void*)(intptr_t)i);
    cc_ic[i]=lv_label_create(cc_tile[i]); lv_obj_set_style_text_font(cc_ic[i],&gu_sm,0); lv_label_set_text(cc_ic[i],IC[i]); lv_obj_align(cc_ic[i],LV_ALIGN_CENTER,0,-14);
    lv_obj_set_style_transform_scale(cc_ic[i],346,0); lv_obj_update_layout(cc_ic[i]); lv_obj_set_style_transform_pivot_x(cc_ic[i],lv_obj_get_width(cc_ic[i])/2,0); lv_obj_set_style_transform_pivot_y(cc_ic[i],lv_obj_get_height(cc_ic[i])/2,0);
    cc_val[i]=lv_label_create(cc_tile[i]); lv_obj_set_style_text_font(cc_val[i],F20,0); lv_obj_align(cc_val[i],LV_ALIGN_CENTER,0,24);
    cc_lb[i]=label(cc,F20,K_SEC,x,y+68); lv_label_set_text(cc_lb[i],LB[i]); }
  
  gw_grok_create(chat);
}
void gw_faces_sheet(const char* t,const char* b){ if(!sheet) return; if(!t&&!b){ lv_obj_add_flag(sheet,LV_OBJ_FLAG_HIDDEN); return; }
  lv_label_set_text(sh_app,""); lv_obj_add_flag(sh_app,LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(sh_t,t?t:""); lv_obj_set_style_text_color(sh_t,col(K_ORG),0); lv_label_set_text(sh_b,b?b:"");
  lv_obj_scroll_to_y(sh_scroll,0,LV_ANIM_OFF); lv_obj_remove_flag(sheet,LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(sheet);
  if(dotbar){ lv_anim_delete(dotbar,NULL); lv_obj_set_style_opa(dotbar,0,0); } }  // 2.6.18: sheet now really stays on top; hide page dots like the CC does

static const gw_cc_t* cc_st;
void gw_cc_open(const gw_cc_t* st,void (*on_tap)(int)){ if(!cc) return; if(st&&dotbar){ lv_anim_delete(dotbar,NULL); lv_obj_set_style_opa(dotbar,0,0); } if(!st){ lv_obj_add_flag(cc,LV_OBJ_FLAG_HIDDEN); return; }
  cc_cb=on_tap; cc_st=st; char b[96];
  snprintf(b,sizeof b,"%s%d%%  ·  %s  ·  手机 %s",st->charging?"充电 ":"",st->battery,st->ble&&st->wifi?"蓝牙 Wi-Fi":(st->ble?"蓝牙":(st->wifi?"Wi-Fi":"未连接")),st->phone_batt>=0?"":"--");
  if(st->phone_batt>=0){ size_t l=strlen(b); snprintf(b+l,sizeof b-l,"%d%%",st->phone_batt); }
  lv_label_set_text(cc_stat,b);
  bool on[GW_CC_N]={st->bright>0,st->volume>0,st->night,st->dnd};
  static const uint32_t AC[4]={A_YELLOW,A_PINK,A_ORANGE,A_INDIGO}; // brightness, volume, night (≈Night Shift), DND (Focus)
  for(int i=0;i<GW_CC_N;i++){ bool f=on[i]&&i>=2; lv_obj_set_style_bg_color(cc_tile[i],col(f?AC[i]:K_CARD2),0);
    lv_obj_set_style_text_color(cc_ic[i],f?col(K_TXT):col(AC[i]),0); lv_obj_set_style_text_color(cc_val[i],col(K_TXT),0); }
  snprintf(b,sizeof b,"%d%%",st->bright); lv_label_set_text(cc_val[0],b); snprintf(b,sizeof b,"%d%%",st->volume); lv_label_set_text(cc_val[1],b);
  lv_label_set_text(cc_val[2],st->night?"开":"关"); lv_label_set_text(cc_val[3],st->dnd?"开":"关");
  lv_obj_remove_flag(cc,LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(cc); }
bool gw_cc_is_open(void){ return cc&&!lv_obj_has_flag(cc,LV_OBJ_FLAG_HIDDEN); }
bool gw_cc_on_top(void){ return gw_cc_is_open()&&lv_obj_get_index(cc)>lv_obj_get_index(root); }  // sim audit
bool gw_faces_sheet_open(void){ return sheet&&!lv_obj_has_flag(sheet,LV_OBJ_FLAG_HIDDEN); }


// ---------- animations ----------
static void ax_x(void* o,int32_t v){ lv_obj_set_x((lv_obj_t*)o,v); }
static void ax_y(void* o,int32_t v){ lv_obj_set_y((lv_obj_t*)o,v); }
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
static void hide_done(lv_anim_t* a){ lv_obj_add_flag((lv_obj_t*)a->var,LV_OBJ_FLAG_HIDDEN); }
// 200 ms ease-out turn change: fade 0->1 + scale 0.94->1 (no overshoot)
static void ax_vk(void* o,int32_t v){ v_k=v/1000.f; lv_obj_invalidate((lv_obj_t*)o); }
static void ax_iscale(void* o,int32_t v){ lv_image_set_scale((lv_obj_t*)o,v); }
static void turn_in(lv_obj_t* o){ anim(o,ax_opa,0,LV_OPA_COVER,200,0,NULL);   // no transform layer: scale is applied in geometry (vector) / image scale (bitmap)
  if(o==v_vec) anim(o,ax_vk,940,1000,200,0,NULL); else anim(o,ax_iscale,300,320,200,0,NULL); }
static void fade_show(lv_obj_t* o,bool on,bool* st){ if(on==*st) return; *st=on; if(on){ lv_obj_remove_flag(o,LV_OBJ_FLAG_HIDDEN); anim(o,ax_opa,0,LV_OPA_COVER,180,0,NULL); }
  else { lv_anim_delete(o,ax_opa); lv_anim_t x; lv_anim_init(&x); lv_anim_set_var(&x,o); lv_anim_set_exec_cb(&x,ax_opa); lv_anim_set_values(&x,lv_obj_get_style_opa(o,0),0);
    lv_anim_set_duration(&x,140); lv_anim_set_path_cb(&x,lv_anim_path_ease_out); lv_anim_set_completed_cb(&x,hide_done); lv_anim_start(&x); } }   // exit faster than enter


// 2.6.14 second hand: one tick per real second + 180 ms ease-out (Apple style). Driven by a 50 ms wall-clock poll, not by a
// 1000 ms LVGL timer (that one beat against the second boundary: same second twice = stall, then +2 s = hard jump).
// Always animates from the on-screen angle, so a late frame or a missed second never snaps. Returns 1 if the hand moved.
static void (*sec_hook)(int);
void gw_faces_set_sec_hook(void (*h)(int)){ sec_hook=h; }
int gw_faces_sec(int ss){ if(cur!=GW_PAGE_FACE||!f_hands||ss==f_ss||ss<0||ss>59) return 0;
  float to=ss*6.f; if(f_ss<0||f_sa<0){ f_ss=ss; sec_inval(f_sa); f_sa=to; sec_inval(f_sa); if(sec_hook) sec_hook(ss); return 1; }
  f_ss=ss; lv_anim_delete(&f_sa,ax_sec); float d=to-f_sa; while(d<0) d+=360.f; while(d>=360.f) d-=360.f;
  if(sec_hook) sec_hook(ss);
  if(d>90.f){ sec_inval(f_sa); f_sa=to; sec_inval(f_sa); return 1; }   // clock set / resume: place, don't spin
  s_from=f_sa; s_span=d; anim(&f_sa,ax_sec,0,1000,180,0,lv_anim_path_ease_out); return 1; }
void gw_faces_show(int page, const gw_state_t* s){
  if(!root) return;
  bool enter=page!=cur; int prev=cur; cur=page;
  if(enter){ // dots: capsule morph + transient visibility
    for(int i=0;i<GW_NCORE;i++){ bool on=i==page; int w=lv_obj_get_width(dots[i]);
      lv_obj_set_style_bg_color(dots[i],col(on?K_ORG:K_TER),0); anim(dots[i],ax_w,w,on?24:7,160,0,NULL); }
    lv_obj_move_foreground(dotbar); { anim(dotbar,ax_opa,LV_OPA_COVER,LV_OPA_COVER,1,0,NULL); anim(dotbar,ax_opa,LV_OPA_COVER,0,400,1600,lv_anim_path_ease_in_out); }
  }
  if(enter) gw_grok_visible(page==GW_PAGE_CHAT);  // the mint character IS the chat page (was never shown on device)
  if(page==GW_PAGE_CHAT){ lv_obj_add_flag(root,LV_OBJ_FLAG_HIDDEN); char b[8]; snprintf(b,sizeof b,"%02d:%02d",s->hh,s->mm); lv_label_set_text(c_time,b); return; }
  // 2.6.18: raise the face only when the page changes. It used to run on every 1 s render() and buried an open Control
  // Center / sheet under the face within a second ("swipe up flashes, then snaps back to the clock").
  lv_obj_remove_flag(root,LV_OBJ_FLAG_HIDDEN); if(enter){ lv_obj_move_foreground(root); if(gw_cc_is_open()) lv_obj_move_foreground(cc); if(gw_faces_sheet_open()) lv_obj_move_foreground(sheet); }
  if(enter){ for(int i=1;i<GW_NPAGES;i++) (i==page)?lv_obj_remove_flag(pg[i],LV_OBJ_FLAG_HIDDEN):lv_obj_add_flag(pg[i],LV_OBJ_FLAG_HIDDEN);
    lv_obj_t* o=pg[page]; // Horizontal slide ALWAYS (fade flag only chose slide+fade vs slide-only, but used to disable the slide entirely,
    // so the swap was painted by the 16-line strip flush top->bottom and read as a vertical flip). +dir = enter from right.
    int off=prev<0?0:GW_SLIDE_PX*dir;
    lv_obj_set_x(o,0); lv_obj_set_style_opa(o,LV_OPA_COVER,0); lv_obj_set_y(o,0); if(off&&slide_on){ lv_obj_set_x(o,off); anim(o,ax_x,off,0,GW_SLIDE_MS,0,NULL); }
    prog=1000; /* no sweep on enter: one crisp full redraw (sweeps cost ~120ms/frame over QSPI) */ dir=1; }
  char b[96]; static const char* WE[]={"SUN","MON","TUE","WED","THU","FRI","SAT"};
  if(page==GW_PAGE_FACE){
    on_rim(f_sec,222,s->ss*6);
    { static const char* CN[]={"日","一","二","三","四","五","六"}; snprintf(b,sizeof b,"周%s %d",CN[s->wday%7],s->day); lv_label_set_text(f_date,b); (void)WE; }
    if(s->usage_pct>=0&&s->usage_reset&&s->usage_reset[0]) snprintf(b,sizeof b,"Grok Bot %d%% · %s",s->usage_pct,s->usage_reset);
    else if(s->usage_pct>=0) snprintf(b,sizeof b,"Grok Bot %d%%",s->usage_pct); else snprintf(b,sizeof b,"Grok Bot \xE2\x80\x94");   // never invent a number
    lv_label_set_text(f_km,b); lv_obj_update_layout(f_km); { int w=lv_obj_get_width(f_km)+44; lv_obj_set_width(f_pill,w<180?180:(w>300?300:w)); lv_obj_align(f_pill,LV_ALIGN_CENTER,0,78); }
    if(enter){ lv_anim_delete(&f_sa,ax_sec); sec_inval(f_sa); f_ss=s->ss; f_sa=s->ss*6.f; sec_inval(f_sa); } else gw_faces_sec(s->ss);
    if(s->hh!=f_hh||s->mm!=f_mm){ if(f_hh<0||enter) lv_obj_invalidate(f_hands); else hands_inval(); f_hh=s->hh; f_mm=s->mm; if(!enter) hands_inval(); }
    snprintf(b,sizeof b,"%02d:%02d",s->hh,s->mm); lv_label_set_text(f_time,b);
    t_bat=s->battery/100.f; uint32_t bc=s->charging?K_GRN:(s->battery<=20?K_RED:(s->battery<=40?K_YEL:K_GRN));
    lv_obj_set_style_arc_color(f_g[0],col(bc),LV_PART_INDICATOR);
    if(prog>=1000){ arc_set(f_g[0],135,270,t_bat); snprintf(b,sizeof b,"%d",s->battery); lv_label_set_text(f_gv[0],b); }
    lv_obj_set_style_text_color(f_gv[0],col(s->battery<=20?K_RED:K_TXT),0);
    // middle gauge: phone battery (notifications were removed in 2.6.2)
    arc_set(f_g[1],135,270,s->phone_batt>=0?s->phone_batt/100.f:0);
    if(s->phone_batt>=0) snprintf(b,sizeof b,"%d",s->phone_batt); else snprintf(b,sizeof b,"--"); lv_label_set_text(f_gv[1],b);
    lv_obj_set_style_text_color(f_gv[1],col(s->phone_batt>=0?K_TXT:K_SEC),0);
    // link: left half = Wi-Fi, right half = phone BLE
    arc_set(f_g[2],135,132,s->wifi?1:0); lv_arc_set_bg_angles(f_g[2],135,405);
    lv_arc_set_bg_angles(f_glink2,273,405); if(s->ble){ lv_obj_set_style_arc_opa(f_glink2,LV_OPA_COVER,LV_PART_INDICATOR); lv_arc_set_angles(f_glink2,273,405);} else lv_obj_set_style_arc_opa(f_glink2,LV_OPA_TRANSP,LV_PART_INDICATOR);
    lv_label_set_text(f_gv[2],s->ble?I_BT:I_WIFI); lv_obj_set_style_text_color(f_gv[2],col(s->ble?K_BLUE:(s->wifi?K_CYAN:K_TER)),0);
  } else if(page==GW_PAGE_RIDE){
    bool has=s->speed&&s->speed[0]; float v=0; if(has) sscanf(s->speed,"%f",&v);
    if(enter){ a_spd=v; t_spd=v; } else if(v!=t_spd){ anim(&t_spd,ax_spd,(int)(a_spd*10),(int)(v*10),400,0,NULL); t_spd=v; }
    if(prog>=1000) apply_prog();
    if(has){ int t10=(int)(v*10+0.5f); if(t10<0) t10=0; if(t10>9999) t10=9999; snprintf(b,sizeof b,"%d",t10/10); lv_label_set_text(r_speed,b); snprintf(b,sizeof b,".%d",t10%10); lv_label_set_text(r_dec,b);
      lv_obj_set_width(r_speed,t10>=1000?LV_SIZE_CONTENT:r_iw);   /* 100+ km/h: grow */ lv_obj_set_style_text_color(r_speed,col(K_TXT),0); lv_obj_set_style_text_color(r_dec,col(K_TXT),0); }
    else { lv_obj_set_width(r_speed,r_iw); lv_label_set_text(r_speed,"--"); lv_label_set_text(r_dec,""); lv_obj_set_style_text_color(r_speed,col(K_TER),0); }
    snprintf(b,sizeof b,"%d:%02d",s->ride_min/60,s->ride_min%60); lv_label_set_text(r_f[1],b);
    if(s->nav_active&&s->nav_eta&&s->nav_eta[0]){ lv_label_set_text(r_f[0],s->nav_eta); lv_label_set_text(r_fl[0],"到达"); }
    else { snprintf(b,sizeof b,"%.1f",s->ride_km); lv_label_set_text(r_f[0],b); lv_label_set_text(r_fl[0],"里程 km"); }
    lv_label_set_text(r_hint,s->ble?"等待 GPS":"App 未连接"); lv_obj_set_style_text_color(r_hint,col(K_SEC),0); lv_obj_set_style_text_opa(r_hint,190,0);
    has?lv_obj_add_flag(r_hint,LV_OBJ_FLAG_HIDDEN):lv_obj_remove_flag(r_hint,LV_OBJ_FLAG_HIDDEN);
  } else if(page==GW_PAGE_TODO){
    bool any=s->n_todo>0;
    if(s->todo_state==3&&!any) snprintf(b,sizeof b,"待办 · 未启用"); else if(s->todo_state==2&&!any) snprintf(b,sizeof b,"待办 · 离线"); else if(s->todo_state==1&&!any) snprintf(b,sizeof b,"待办 · 同步中"); else if(s->todo_open) snprintf(b,sizeof b,"待办 · %d",s->todo_open); else snprintf(b,sizeof b,"待办");
    lv_label_set_text(t_head,b);
    { int n=s->n_todo>GW_TODO_ROWS?GW_TODO_ROWS:s->n_todo; int rh=n<=4?60:48, gap=n<=4?10:6, lh=lv_font_get_line_height(F20), hs[GW_TODO_ROWS], tot=0;
      for(int i=0;i<n;i++){ lv_point_t sz; const char* tx=s->todo_text[i]?s->todo_text[i]:""; lv_text_get_size(&sz,tx,F20,0,2,lv_obj_get_width(t_txt[i]),LV_TEXT_FLAG_NONE);
        bool two=n<=4&&sz.y>lh+2; hs[i]=two?rh+lh-6:rh; lv_obj_set_height(t_txt[i],two?2*lh+2:lh+2); tot+=hs[i]+(i?gap:0); }
      for(int i=0,y=4-tot/2;i<n;i++){ lv_obj_set_height(t_row[i],hs[i]); lv_obj_set_style_radius(t_row[i],LV_MIN(hs[i],rh)/2,0); lv_obj_align(t_row[i],LV_ALIGN_TOP_MID,0,233+y); y+=hs[i]+gap; } }
    for(int i=0;i<GW_TODO_ROWS;i++){ if(i<s->n_todo){ bool d=s->todo_done[i];
        lv_label_set_text(t_txt[i],s->todo_text[i]?s->todo_text[i]:""); lv_obj_set_style_text_color(t_txt[i],col(K_TXT),0); lv_obj_set_style_text_opa(t_txt[i],d?150:255,0);
        lv_obj_set_style_text_decor(t_txt[i],d?LV_TEXT_DECOR_STRIKETHROUGH:LV_TEXT_DECOR_NONE,0);
        lv_obj_set_style_bg_color(t_box[i],col(d?K_TXT:K_CARD),0); lv_obj_set_style_border_color(t_box[i],col(d?K_TXT:K_SEC),0); lv_obj_set_style_opa(t_box[i],d?170:255,0); lv_obj_remove_flag(t_row[i],LV_OBJ_FLAG_HIDDEN); }
      else lv_obj_add_flag(t_row[i],LV_OBJ_FLAG_HIDDEN); }
    if(s->todo_open>s->n_todo){ snprintf(b,sizeof b,"还有 %d 条",s->todo_open-s->n_todo); lv_label_set_text(t_more,b); lv_obj_remove_flag(t_more,LV_OBJ_FLAG_HIDDEN);} else lv_obj_add_flag(t_more,LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(t_empty[2],s->todo_state==3?"服务器未启用待办\n请检查服务配置":s->todo_state==2?"连不上服务器\n检查服务与网络":"长按说话\n添加待办");
    for(int i=0;i<3;i+=2) any?lv_obj_add_flag(t_empty[i],LV_OBJ_FLAG_HIDDEN):lv_obj_remove_flag(t_empty[i],LV_OBJ_FLAG_HIDDEN);
  } else if(page==GW_PAGE_NAV){
    lv_obj_t* act[]={v_img,v_row,v_card,v_meta}; if(!s->nav_active){ lv_obj_add_flag(v_lt,LV_OBJ_FLAG_HIDDEN); v_lt_on=false; lv_obj_add_flag(v_vec,LV_OBJ_FLAG_HIDDEN); v_turn[0]=0; }
    for(int i=0;i<4;i++) s->nav_active?lv_obj_remove_flag(act[i],LV_OBJ_FLAG_HIDDEN):lv_obj_add_flag(act[i],LV_OBJ_FLAG_HIDDEN);
    for(int i=0;i<4;i+=(i==0?2:1)) s->nav_active?lv_obj_add_flag(v_idle[i],LV_OBJ_FLAG_HIDDEN):lv_obj_remove_flag(v_idle[i],LV_OBJ_FLAG_HIDDEN);
    if(s->nav_active){
      bool vec=vknown(s->nav_turn);
      if(vec){ if(strcmp(v_turn,s->nav_turn)){ snprintf(v_turn,sizeof v_turn,"%s",s->nav_turn); lv_obj_invalidate(v_vec); turn_in(v_vec); }
        lv_obj_remove_flag(v_vec,LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(v_img,LV_OBJ_FLAG_HIDDEN); }
      else { if(v_turn[0]){ v_turn[0]=0; v_has=false; } lv_obj_add_flag(v_vec,LV_OBJ_FLAG_HIDDEN);
        if(s->nav_icon&&(!v_has||memcmp(v_last,s->nav_icon,288))){ memcpy(v_last,s->nav_icon,288); v_has=true; smooth_icon(s->nav_icon);
          memset(&v_dsc,0,sizeof v_dsc); v_dsc.header.magic=LV_IMAGE_HEADER_MAGIC; v_dsc.header.cf=LV_COLOR_FORMAT_A8; v_dsc.header.w=96; v_dsc.header.h=96; v_dsc.header.stride=96;
          v_dsc.data=v_px; v_dsc.data_size=96*96; lv_image_set_src(v_img,&v_dsc); lv_obj_invalidate(v_img); turn_in(v_img); } }
      { char d[32]; const char* src=s->nav_dist?s->nav_dist:""; int j=0;
        for(const char* q=src; *q && j<26; ){ if(!strncmp(q,"公里",6)||!strncmp(q,"千米",6)){ j+=snprintf(d+j,sizeof d-j,(j&&d[j-1]==' ')?"km":" km"); q+=6; } else if(!strncmp(q,"米",3)){ j+=snprintf(d+j,sizeof d-j,(j&&d[j-1]==' ')?"m":" m"); q+=3; }
          else if((unsigned char)*q<0x80){ if(!(*q==' '&&j&&d[j-1]==' ')) d[j++]=*q; q++; } else q++; }
        d[j]=0; char* sp=strrchr(d,' '); const char* un="";
        if(sp){ *sp=0; un=sp+1; } if(strcmp(lv_label_get_text(v_dist),d)||strcmp(lv_label_get_text(v_unit),un)){ lv_label_set_text(v_dist,d); lv_label_set_text(v_unit,un); nav_layout(); } }   // unchanged text: no redraw/flicker
      if(strcmp(lv_label_get_text(v_dir),s->nav_dir?s->nav_dir:"")) lv_label_set_text(v_dir,s->nav_dir?s->nav_dir:"");
      { bool hd=s->nav_dur&&s->nav_dur[0], he=s->nav_eta&&s->nav_eta[0];   // no ETA: show only what we have, never a dangling "ETA"
        if(hd&&he) snprintf(b,sizeof b,"%s  " "\xC2\xB7" "  ETA %s",s->nav_dur,s->nav_eta); else if(hd) snprintf(b,sizeof b,"剩余 %s",s->nav_dur); else if(he) snprintf(b,sizeof b,"ETA %s",s->nav_eta); else b[0]=0;
        lv_label_set_text(v_meta,b); b[0]?lv_obj_remove_flag(v_meta,LV_OBJ_FLAG_HIDDEN):lv_obj_add_flag(v_meta,LV_OBJ_FLAG_HIDDEN); }
      if(s->light_s>0){ snprintf(b,sizeof b,"%d",s->light_s); lv_label_set_text(v_ltn,b);
        uint32_t lc=s->light_c=='G'?K_GRN:(s->light_c=='Y'?K_YEL:K_RED); lv_obj_set_style_bg_color(v_ltd,col(lc),0); fade_show(v_lt,true,&v_lt_on); }
      else fade_show(v_lt,false,&v_lt_on);
    } else {
      lv_obj_set_style_border_color(v_idle[0],col(K_TER),0); lv_obj_set_style_border_opa(v_idle[0],LV_OPA_COVER,0);
      lv_obj_set_style_text_color(v_idle[1],col(K_SEC),0);
      lv_label_set_text(v_idle[2],"暂无导航");
      lv_label_set_text(v_idle[3],s->ble?"手机开始导航后显示":"App 未连接"); }
  }
}

// ---------------------------------------------------------------------------------------------
// Standby clock (screen idle): hours over minutes, vector-drawn rounded digits (no font asset),
// 24 px round-capped strokes on split flip cards (2.6.36), tabular. Day: pale mint #E8EFC4; night: col() dims it to warm white.
// Fits the 215 px safe radius of the 466 round panel. Anti burn-in: whole block steps along an
// 8-point +-6 px orbit once per minute. Motion: 280 ms ease-out fade on enter, 520 ms card flip on minute change.
#include <math.h>
#define SB_H 1.30f                     /* digit centre-line box: 1 wide x SB_H tall */
#define SB_DW 108
#define SB_DH 134
#define SB_SW 24
#define SB_GX 10
#define SB_GY 12
#define SB_PAD 12                      /* 2.6.36 flip clock: glyph inset inside its card */
#define SB_CW (SB_DW+2*SB_PAD)
#define SB_CH (SB_DH+2*SB_PAD)
typedef struct { char t; float a,b,c,d,e; } sb_prim;   /* 'L' x1 y1 x2 y2 | 'A' cx cy r a0 a1 (deg, 0=3 o'clock, clockwise) */
static sb_prim sb_g[10][5]; static bool sb_ready=false;
static float sb_tangent(float cx,float cy,float r,float px,float py,int side){ /* angle (deg) of the tangent point seen from P */
  float dx=px-cx,dy=py-cy,dd=sqrtf(dx*dx+dy*dy); float a=atan2f(dy,dx)+side*acosf(r/dd); return a*57.29578f; }
static void sb_setup(void){
  float H=SB_H; int k;
  /* 0 */ sb_prim g0[]={{'A',.5,.5,.5,180,360},{'A',.5,H-.5,.5,0,180},{'L',0,.5,0,H-.5},{'L',1,.5,1,H-.5},{0}};
  /* 1 */ sb_prim g1[]={{'L',.6,0,.6,H},{'L',.6,0,.2,.3},{0}};
  /* 2: top bowl runs into the diagonal at its exact tangent point */
  float t2=sb_tangent(.5,.47,.47,0,H,-1); while(t2<180) t2+=360; float x2=.5f+.47f*cosf(t2/57.29578f), y2=.47f+.47f*sinf(t2/57.29578f);
  sb_prim g2[]={{'A',.5,.47,.47,180,t2},{'L',x2,y2,0,H},{'L',0,H,1,H},{0}};
  /* 3 */ sb_prim g3[]={{'A',.5,.30,.30,195,450},{'A',.5,H-.40,.40,270,520},{0}};
  /* 4: gentler diagonal */ sb_prim g4[]={{'L',.74,0,.74,H},{'L',.66,0,0,.86},{'L',0,.86,1,.86},{0}};
  /* 5: stem ends exactly where the bowl starts */
  float a5=212, x5=.5f+.46f*cosf(a5/57.29578f), y5=(H-.46f)+.46f*sinf(a5/57.29578f);
  sb_prim g5[]={{'L',.92,0,x5+.02f,0},{'L',x5+.02f,0,x5,y5},{'A',.5,H-.46f,.46,a5,510},{0}};
  /* 6: ascender leaves the bowl at its tangent */
  float t6=sb_tangent(.5,H-.5,.5,.62,0,-1); float x6=.5f+.5f*cosf(t6/57.29578f), y6=(H-.5f)+.5f*sinf(t6/57.29578f);
  sb_prim g6[]={{'A',.5,H-.5,.5,0,360},{'L',x6,y6,.62,0},{0}};
  /* 7 */ sb_prim g7[]={{'L',0,0,1,0},{'L',1,0,.36,H},{0}};
  /* 8: two stacked bowls that just touch (top one narrower) */ sb_prim g8[]={{'A',.5,.28,.28,0,360},{'A',.5,H-.37f,.37,0,360},{0}};
  /* 9: tail leaves the bowl at its tangent and stays inside the box */
  float t9=sb_tangent(.5,.5,.5,.38,H,-1); float x9=.5f+.5f*cosf(t9/57.29578f), y9=.5f+.5f*sinf(t9/57.29578f);
  sb_prim g9[]={{'A',.5,.5,.5,0,360},{'L',x9,y9,.38,H},{0}};
  sb_prim* all[10]={g0,g1,g2,g3,g4,g5,g6,g7,g8,g9};
  for(int d=0;d<10;d++){ for(k=0;all[d][k].t;k++) sb_g[d][k]=all[d][k]; sb_g[d][k].t=0; } sb_ready=true; }
static lv_obj_t *sb; static char sb_txt[5]="0000", sb_old[5]="0000"; static int sb_ox=0, sb_oy=0, sb_fp=1000;   /* flip progress 0..1000 */
static void sb_glyph(lv_layer_t* L,int dg,float x,float y,lv_color_t c,lv_opa_t opa){
  lv_draw_line_dsc_t ld; lv_draw_line_dsc_init(&ld); ld.color=c; ld.width=SB_SW; ld.round_start=ld.round_end=1; ld.opa=opa;
  lv_draw_arc_dsc_t ad; lv_draw_arc_dsc_init(&ad); ad.color=c; ad.width=SB_SW; ad.rounded=1; ad.opa=opa;
  float sx=SB_DW-SB_SW, sy=(SB_DH-SB_SW)/SB_H, s=sx<sy?sx:sy, ox=x+SB_SW/2.f+(SB_DW-SB_SW-s)/2, oy=y+SB_SW/2.f+(SB_DH-SB_SW-s*SB_H)/2;
  for(int i=0;sb_g[dg][i].t;i++){ const sb_prim* q=&sb_g[dg][i];
    if(q->t=='L'){ ld.p1.x=ox+q->a*s; ld.p1.y=oy+q->b*s; ld.p2.x=ox+q->c*s; ld.p2.y=oy+q->d*s; lv_draw_line(L,&ld); }
    else { ad.center.x=ox+q->a*s; ad.center.y=oy+q->b*s; ad.radius=(uint16_t)(q->c*s+SB_SW/2.f); ad.start_angle=q->d; ad.end_angle=q->e; lv_draw_arc(L,&ad); } } }
// 2.6.36 flip clock: each digit on a split card. A changed digit flips: the old top half folds down onto the hinge
// (new top shows behind it), then the new bottom half unfolds over the old bottom. Fold = clip toward the hinge.
static void sb_card(lv_layer_t* L,int dg,float x,float y,int c1,int c2,lv_opa_t opa,lv_opa_t shade){
  lv_area_t keep=L->_clip_area, ca={LV_MAX(keep.x1,(int)x),LV_MAX(keep.y1,c1),LV_MIN(keep.x2,(int)x+SB_CW-1),LV_MIN(keep.y2,c2-1)};
  if(ca.x1>ca.x2||ca.y1>ca.y2) return; L->_clip_area=ca;
  lv_draw_rect_dsc_t r; lv_draw_rect_dsc_init(&r); r.radius=16; r.bg_color=col(0x1C1E1B); r.bg_opa=opa;
  lv_area_t card={(int)x,(int)y,(int)x+SB_CW-1,(int)y+SB_CH-1}; lv_draw_rect(L,&r,&card);
  sb_glyph(L,dg,x+SB_PAD,y+SB_PAD,col(0xE8EFC4),opa);
  if(shade){ r.bg_color=lv_color_black(); r.bg_opa=(lv_opa_t)(shade*opa/255); lv_draw_rect(L,&r,&card); }
  L->_clip_area=keep; }
static void sb_draw(lv_event_t* e){ lv_layer_t* L=lv_event_get_layer(e); lv_area_t a; lv_obj_get_coords(sb,&a);
  float cx=(a.x1+a.x2)/2.f+sb_ox, cy=(a.y1+a.y2)/2.f+sb_oy, x0=cx-(2*SB_CW+SB_GX)/2.f, y0=cy-(2*SB_CH+SB_GY)/2.f;
  lv_opa_t opa=lv_obj_get_style_opa_recursive(sb,0); int h=SB_CH/2;
  for(int i=0;i<4;i++){ float x=x0+(i&1)*(SB_CW+SB_GX), y=y0+(i>>1)*(SB_CH+SB_GY); int top=(int)y, hy=top+h, bot=top+SB_CH;
    int dn=sb_txt[i]-'0', dold=sb_old[i]-'0';
    if(dn==dold||sb_fp>=1000){ sb_card(L,dn,x,y,top,bot,opa,0); }
    else { sb_card(L,dn,x,y,top,hy,opa,0); sb_card(L,dold,x,y,hy,bot,opa,0);
      if(sb_fp<500) sb_card(L,dold,x,y,hy-h*(500-sb_fp)/500,hy,opa,(lv_opa_t)(sb_fp*140/500));      // old top folding down, darkening
      else          sb_card(L,dn,x,y,hy,hy+h*(sb_fp-500)/500,opa,(lv_opa_t)((1000-sb_fp)*140/500)); }  // new bottom unfolding, lightening
    lv_draw_rect_dsc_t g; lv_draw_rect_dsc_init(&g); g.bg_color=lv_color_black(); g.bg_opa=opa;   // hinge gap follows standby fade, just like the cards
    lv_area_t ga={(int)x,hy-2,(int)x+SB_CW-1,hy+1}; lv_draw_rect(L,&g,&ga); } }
static void sb_flip_cb(void* o,int32_t v){ (void)o; sb_fp=v; if(sb) lv_obj_invalidate(sb); }
static const signed char sb_orbit[8][2]={{0,0},{5,-3},{6,3},{2,6},{-3,5},{-6,1},{-5,-4},{-1,-6}};
void gw_standby_create(lv_obj_t* parent){ if(!sb_ready) sb_setup();
  sb=lv_obj_create(parent); lv_obj_remove_style_all(sb); lv_obj_set_size(sb,466,466); lv_obj_center(sb);
  lv_obj_set_style_bg_color(sb,lv_color_black(),0); lv_obj_set_style_bg_opa(sb,LV_OPA_COVER,0);
  lv_obj_add_flag(sb,LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(sb,LV_OBJ_FLAG_SCROLLABLE); lv_obj_remove_flag(sb,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(sb,sb_draw,LV_EVENT_DRAW_MAIN_END,NULL); }
bool gw_standby_is_on(void){ return sb&&!lv_obj_has_flag(sb,LV_OBJ_FLAG_HIDDEN); }
static int sb_a=0; static void sb_opa(void* o,int32_t v){ sb_a=v; lv_obj_set_style_opa((lv_obj_t*)o,(lv_opa_t)v,0); }
static void sb_fade(int from,int ms){ lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a,sb); lv_anim_set_exec_cb(&a,sb_opa);
  lv_anim_set_values(&a,from,LV_OPA_COVER); lv_anim_set_duration(&a,ms); lv_anim_set_path_cb(&a,lv_anim_path_ease_out); lv_anim_start(&a); }
// 2.6.17: touch never woke the standby clock (only the keys called PowerSaveTimer::WakeUp). While shown, the standby
// layer is clickable: it swallows the tap (no click falls through to the page underneath) and asks the board to wake.
static void (*sb_tap)(void);
static void sb_press(lv_event_t* e){ (void)e; if(sb_tap) sb_tap(); }
void gw_standby_set_tap(void (*cb)(void)){ sb_tap=cb; if(sb) lv_obj_add_event_cb(sb,sb_press,LV_EVENT_PRESSED,NULL); }
// 2.6.19: enter = 360 ms ease-out fade in; leave = 220 ms ease-in fade out (exit faster than enter). The layer stays
// clickable during the fade-out so the waking touch can never land on the page underneath.
static void sb_out_done(lv_anim_t* a){ (void)a; if(sb){ lv_obj_add_flag(sb,LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(sb,LV_OBJ_FLAG_CLICKABLE); lv_obj_invalidate(lv_screen_active()); } }
void gw_standby_show(bool on){ if(!sb) return; lv_anim_delete(sb,sb_opa);
  if(on){ if(lv_obj_has_flag(sb,LV_OBJ_FLAG_HIDDEN)) sb_a=0; lv_obj_add_flag(sb,LV_OBJ_FLAG_CLICKABLE); lv_obj_remove_flag(sb,LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(sb); sb_fade(sb_a<LV_OPA_COVER?sb_a:0,360); return; }
  lv_anim_delete(&sb_fp,sb_flip_cb); sb_fp=1000; memcpy(sb_old,sb_txt,sizeof sb_old);
  if(lv_obj_has_flag(sb,LV_OBJ_FLAG_HIDDEN)) return;
  lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a,sb); lv_anim_set_exec_cb(&a,sb_opa); lv_anim_set_values(&a,sb_a,0);
  lv_anim_set_duration(&a,220); lv_anim_set_path_cb(&a,lv_anim_path_ease_in); lv_anim_set_completed_cb(&a,sb_out_done); lv_anim_start(&a); }
void gw_standby_set(int hh,int mm,uint32_t minute_count){
  if(!sb) return; char b[5]={(char)('0'+(hh/10)%10),(char)('0'+hh%10),(char)('0'+(mm/10)%10),(char)('0'+mm%10),0}; if(hh<0||mm<0) return;
  bool changed=strcmp(b,sb_txt)!=0; if(changed) memcpy(sb_old,sb_txt,5); memcpy(sb_txt,b,5);
  const signed char* o=sb_orbit[minute_count%8]; sb_ox=o[0]; sb_oy=o[1];
  if(changed&&gw_standby_is_on()){ lv_anim_t f; lv_anim_init(&f); lv_anim_set_var(&f,&sb_fp); lv_anim_set_exec_cb(&f,sb_flip_cb);
    lv_anim_set_values(&f,0,1000); lv_anim_set_duration(&f,520); lv_anim_set_path_cb(&f,lv_anim_path_ease_in_out); sb_fp=0; lv_anim_start(&f); }
  else { memcpy(sb_old,b,5); sb_fp=1000; } lv_obj_invalidate(sb); }

// Power status stays readable over imported photos; the charge screen is transient.
static lv_obj_t *power_badge, *power_screen, *power_value, *power_ring, *power_bolt, *power_title;
static void (*power_dismiss)(void);
static void power_pressed(lv_event_t* e){ (void)e; if(power_dismiss) power_dismiss(); lv_obj_add_flag(power_screen,LV_OBJ_FLAG_HIDDEN); }
void gw_power_create(lv_obj_t* parent, void (*dismiss)(void)){
  power_dismiss=dismiss;
  power_badge=label(parent,F20,K_TXT,0,0);
  lv_obj_set_style_bg_color(power_badge,lv_color_black(),0);
  lv_obj_set_style_bg_opa(power_badge,LV_OPA_COVER,0);
  lv_obj_set_style_pad_hor(power_badge,8,0); lv_obj_set_style_radius(power_badge,8,0);
  lv_obj_align(power_badge,LV_ALIGN_BOTTOM_MID,0,-20);
  power_screen=lv_obj_create(parent); lv_obj_remove_style_all(power_screen);
  lv_obj_set_size(power_screen,466,466); lv_obj_center(power_screen);
  lv_obj_set_style_bg_color(power_screen,lv_color_black(),0); lv_obj_set_style_bg_opa(power_screen,LV_OPA_COVER,0);
  lv_obj_remove_flag(power_screen,LV_OBJ_FLAG_SCROLLABLE); lv_obj_add_flag(power_screen,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(power_screen,power_pressed,LV_EVENT_PRESSED,NULL);
  power_title=label(power_screen,F21,K_GRN,0,-175);
  power_ring=arc(power_screen,300,12,K_CARD,K_GRN); lv_obj_center(power_ring);
  // Vector bolt avoids depending on a private-use glyph missing from gu_icon.
  static const lv_point_precise_t bolt_points[]={{28,0},{4,26},{20,26},{0,52}};
  power_bolt=lv_line_create(power_screen); lv_line_set_points(power_bolt,bolt_points,4);
  lv_obj_set_style_line_color(power_bolt,col(K_GRN),0); lv_obj_set_style_line_width(power_bolt,5,0);
  lv_obj_set_style_line_rounded(power_bolt,true,0); lv_obj_align(power_bolt,LV_ALIGN_CENTER,0,-60);
  power_value=label(power_screen,&gu_hero,K_TXT,0,20);
  lv_obj_t* pct=label(power_screen,F21,K_TXT,0,105); lv_label_set_text(pct,"%");
  lv_obj_t* hint=label(power_screen,F20,K_SEC,0,175); lv_label_set_text(hint,"估算电量 · 轻触返回");
}
void gw_power_show(int level, int charge_state, bool show, bool asleep, unsigned phase){
  bool charging=charge_state==1;
  if(!power_badge) return;
  if(asleep){ lv_obj_add_flag(power_badge,LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(power_screen,LV_OBJ_FLAG_HIDDEN); return; }
  char b[48];
  if(level<0) snprintf(b,sizeof b,"电量 --");
  else snprintf(b,sizeof b,"%s~%d%%",charging?"充电 ":charge_state==2?"接电 ":"电量 ",level);
  lv_label_set_text(power_badge,b); lv_obj_set_style_text_color(power_badge,col(charging?K_GRN:(level>=0&&level<=20?K_RED:K_TXT)),0);
  lv_obj_remove_flag(power_badge,LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(power_badge);
  if(!show){ lv_obj_add_flag(power_screen,LV_OBJ_FLAG_HIDDEN); return; }
  lv_obj_remove_flag(power_screen,LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(power_screen);
  lv_label_set_text(power_title,charging?"正在充电":charge_state==2?"已接电，未充电":"电源状态未知");
  if(level<0) snprintf(b,sizeof b,"--"); else snprintf(b,sizeof b,"%d",level); lv_label_set_text(power_value,b);
  arc_set(power_ring,270,359,level/100.f);
  // One visual step per existing 1 s UI tick; no extra wake timer or perpetual animation.
  lv_obj_set_style_opa(power_bolt,(phase&1)?LV_OPA_COVER:LV_OPA_50,0);
}
