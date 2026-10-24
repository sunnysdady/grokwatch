// GrokWatch v3 final: R5 tokens + Nunito Medium 280/150, secondary 26, CJK 24.
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#define W 466
LV_FONT_DECLARE(hv_ride) LV_FONT_DECLARE(hv_nav) LV_FONT_DECLARE(hv_sec) LV_FONT_DECLARE(hv_zh24) LV_FONT_DECLARE(hv_icon)
static uint16_t fb[W*W]; static lv_display_t* d; static uint32_t tick=0; static uint32_t tk(void){return tick;}
static void flush(lv_display_t* dd,const lv_area_t* a,uint8_t* px){int w=a->x2-a->x1+1;for(int y=a->y1;y<=a->y2;y++)memcpy(&fb[y*W+a->x1],px+((long)y*W+a->x1)*2,w*2);lv_display_flush_ready(dd);}
static int night; static uint32_t BG,HERO,SEC,ACC,OVL,ARCT,ARCF;
static void tones(void){ if(!night){BG=0x213F3B;HERO=0xF4F6F2;SEC=0xBFD0CC;ACC=0xF7A04A;OVL=0xFFFFFF;ARCT=0x2D4F4A;ARCF=0x8FB0AA;}
  else{BG=0x100D0A;HERO=0x6A5D4E;SEC=0x857867;ACC=0xB07540;OVL=0xFFE8C8;ARCT=0x211B15;ARCF=0x4A3F33;} }
static void save(const char* n){ for(int i=0;i<3;i++){tick+=50;lv_timer_handler();} lv_refr_now(d);
  char p[256]; snprintf(p,sizeof p,"raw/%s_%s.ppm",n,night?"night":"day"); FILE* f=fopen(p,"wb"); fprintf(f,"P6\n%d %d\n255\n",W,W);
  for(int i=0;i<W*W;i++){uint16_t q=fb[i];fputc(((q>>11)&31)*255/31,f);fputc(((q>>5)&63)*255/63,f);fputc((q&31)*255/31,f);} fclose(f);}
static lv_obj_t* S;
static void page(void){ if(S) lv_obj_delete(S); S=lv_obj_create(lv_screen_active()); lv_obj_remove_style_all(S); lv_obj_set_size(S,W,W);
  lv_obj_set_style_bg_color(S,lv_color_hex(BG),0); lv_obj_set_style_bg_opa(S,255,0); lv_obj_remove_flag(S,LV_OBJ_FLAG_SCROLLABLE);}
static lv_obj_t* T(const char* s,const lv_font_t* f,uint32_t col,int x,int y){ lv_obj_t* l=lv_label_create(S); lv_label_set_text(l,s);
  lv_obj_set_style_text_font(l,f,0); lv_obj_set_style_text_color(l,lv_color_hex(col),0); lv_obj_align(l,LV_ALIGN_CENTER,x,y); return l;}
static void arc(int r,int w,uint32_t col,int a0,int a1){ lv_obj_t* a=lv_arc_create(S); lv_obj_set_size(a,2*r,2*r); lv_obj_center(a); lv_arc_set_bg_angles(a,a0,a1);
  lv_obj_set_style_arc_color(a,lv_color_hex(col),0); lv_obj_set_style_arc_width(a,w,0); lv_obj_set_style_arc_rounded(a,1,0); lv_obj_set_style_arc_opa(a,0,LV_PART_INDICATOR); lv_obj_remove_style(a,NULL,LV_PART_KNOB);}
/* digit box of Nunito 280: cap height ~0.70em. Centre the INK (not the line box) at y=-18 */
static void ride(const char* v,int fill,const char* name){ page(); arc(216,8,ARCT,135,405); arc(216,8,ARCF,135,135+fill);
  lv_obj_t* h=T(v,&hv_ride,HERO,0,0); lv_obj_set_style_text_letter_space(h,-6,0);
  if(strlen(v)>=3){ lv_obj_set_style_transform_scale(h,180,0); lv_obj_update_layout(S); lv_obj_set_style_transform_pivot_x(h,lv_obj_get_width(h)/2,0); lv_obj_set_style_transform_pivot_y(h,lv_obj_get_height(h)/2,0); }
  lv_obj_align(h,LV_ALIGN_CENTER,0,-30);
  T("18.6 km     0:52",&hv_sec,SEC,0,130); save(name); }
static uint32_t ac; static void arrowcb(lv_event_t* e){ lv_layer_t* L=lv_event_get_layer(e); lv_draw_line_dsc_t ld; lv_draw_line_dsc_init(&ld);
  ld.color=lv_color_hex(ac); ld.width=22; ld.round_start=ld.round_end=1; int x=264,y=104;
  lv_point_precise_t p[][2]={{{x,y+58},{x,y}},{{x,y},{x-62,y}},{{x-62,y},{x-37,y-25}},{{x-62,y},{x-37,y+25}}};
  for(int i=0;i<4;i++){ld.p1=p[i][0];ld.p2=p[i][1];lv_draw_line(L,&ld);} }
static void nav(const char* dist,int now,const char* name){ page(); uint32_t hc=now?ACC:HERO; ac=hc;
  lv_obj_add_event_cb(S,arrowcb,LV_EVENT_DRAW_POST_END,NULL); lv_obj_invalidate(S);
  /* number + unit as one centred group */
  lv_obj_t* g=lv_obj_create(S); lv_obj_remove_style_all(g); lv_obj_set_size(g,LV_SIZE_CONTENT,LV_SIZE_CONTENT); lv_obj_set_flex_flow(g,LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(g,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_END,LV_FLEX_ALIGN_END); lv_obj_set_style_pad_column(g,8,0);
  lv_obj_t* n=lv_label_create(g); lv_label_set_text(n,dist); lv_obj_set_style_text_font(n,&hv_nav,0); lv_obj_set_style_text_color(n,lv_color_hex(hc),0); lv_obj_set_style_text_letter_space(n,-3,0);
  lv_obj_t* u=lv_label_create(g); lv_label_set_text(u,strchr(dist,'.')?"km":"m"); lv_obj_set_style_text_font(u,&hv_sec,0); lv_obj_set_style_text_color(u,lv_color_hex(now?hc:SEC),0); lv_obj_set_style_pad_bottom(u,30,0);
  lv_obj_align(g,LV_ALIGN_CENTER,0,40);
  T(now?"左转  科技园南路":"科技园南路",&hv_zh24,SEC,0,134); save(name); }
static void cc(void){ page(); const char* ic[4]={"\xEF\x86\x85","\xEF\x80\xA8","\xEF\x86\x86","\xEF\x87\xB6"}; int on[4]={1,1,night,0};
  for(int i=0;i<4;i++){ int x=(i%2)?78:-78,y=(i/2)?78:-78; lv_obj_t* o=lv_obj_create(S); lv_obj_remove_style_all(o); lv_obj_set_size(o,144,144); lv_obj_align(o,LV_ALIGN_CENTER,x,y);
    lv_obj_set_style_radius(o,48,0); lv_obj_set_style_bg_color(o,lv_color_hex(OVL),0); lv_obj_set_style_bg_opa(o,on[i]?(night?40:58):(night?14:20),0);
    T(ic[i],&hv_icon,on[i]?HERO:SEC,x,y); } save("control_center"); }
int main(void){ lv_init(); lv_tick_set_cb(tk); d=lv_display_create(W,W); static uint8_t buf[W*W*2];
  lv_display_set_buffers(d,buf,NULL,sizeof buf,LV_DISPLAY_RENDER_MODE_DIRECT); lv_display_set_flush_cb(d,flush);
  for(night=0;night<2;night++){ tones(); ride("23",155,"ride"); nav("350",0,"nav"); nav("30",1,"nav_turn_now"); cc();
    ride("8",50,"x_ride_1digit"); ride("45",300,"x_ride_45"); ride("188",340,"x_ride_188"); nav("1200",0,"x_nav_1200"); nav("1.2",0,"x_nav_1.2km"); }
  return 0; }
