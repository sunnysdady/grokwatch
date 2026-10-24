// GrokWatch v3 critique rounds. Real LVGL 9 render, 466 round.
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#define W 466
LV_FONT_DECLARE(gu_hero) LV_FONT_DECLARE(gu_num) LV_FONT_DECLARE(gu_sm) LV_FONT_DECLARE(gz_20) LV_FONT_DECLARE(hv_hero) LV_FONT_DECLARE(hv_sm) LV_FONT_DECLARE(hv_icon)
static uint16_t fb[W*W]; static lv_display_t* d; static uint32_t tick=0; static uint32_t tk(void){return tick;}
static void flush(lv_display_t* dd,const lv_area_t* a,uint8_t* px){int w=a->x2-a->x1+1;for(int y=a->y1;y<=a->y2;y++)memcpy(&fb[y*W+a->x1],px+((long)y*W+a->x1)*2,w*2);lv_display_flush_ready(dd);}
typedef struct{ const char* tag; uint32_t bg_top,bg_bot,low,mid,sec,hero,accent; const lv_font_t *hf,*sf,*nf; int turn_fill,latin_sec; } Cfg;
static Cfg c; static int night;
static void save(const char* n){ for(int i=0;i<3;i++){tick+=50;lv_timer_handler();} lv_refr_now(d);
  char p[256]; snprintf(p,sizeof p,"raw/%s_%s_%s.ppm",c.tag,n,night?"night":"day"); FILE* f=fopen(p,"wb"); fprintf(f,"P6\n%d %d\n255\n",W,W);
  for(int i=0;i<W*W;i++){uint16_t q=fb[i];fputc(((q>>11)&31)*255/31,f);fputc(((q>>5)&63)*255/63,f);fputc((q&31)*255/31,f);} fclose(f);}
static lv_obj_t* S;
static void page(uint32_t top,uint32_t bot){ if(S) lv_obj_delete(S); S=lv_obj_create(lv_screen_active()); lv_obj_remove_style_all(S); lv_obj_set_size(S,W,W);
  lv_obj_set_style_bg_color(S,lv_color_hex(top),0); lv_obj_set_style_bg_grad_color(S,lv_color_hex(bot),0); lv_obj_set_style_bg_grad_dir(S,top==bot?LV_GRAD_DIR_NONE:LV_GRAD_DIR_VER,0);
  lv_obj_set_style_bg_opa(S,255,0); lv_obj_remove_flag(S,LV_OBJ_FLAG_SCROLLABLE);}
static lv_obj_t* T(const char* s,const lv_font_t* f,uint32_t col,int x,int y){ lv_obj_t* l=lv_label_create(S); lv_label_set_text(l,s);
  lv_obj_set_style_text_font(l,f,0); lv_obj_set_style_text_color(l,lv_color_hex(col),0); lv_obj_align(l,LV_ALIGN_CENTER,x,y); return l;}
static lv_obj_t* R(int x,int y,int w,int h,uint32_t col,int rad){ lv_obj_t* o=lv_obj_create(S); lv_obj_remove_style_all(o); lv_obj_set_size(o,w,h);
  lv_obj_align(o,LV_ALIGN_CENTER,x,y); lv_obj_set_style_radius(o,rad,0); lv_obj_set_style_bg_color(o,lv_color_hex(col),0); lv_obj_set_style_bg_opa(o,255,0); return o;}
static void arc(int r,int w,uint32_t col,int a0,int a1){ lv_obj_t* a=lv_arc_create(S); lv_obj_set_size(a,2*r,2*r); lv_obj_center(a); lv_arc_set_bg_angles(a,a0,a1);
  lv_obj_set_style_arc_color(a,lv_color_hex(col),0); lv_obj_set_style_arc_width(a,w,0); lv_obj_set_style_arc_rounded(a,1,0); lv_obj_set_style_arc_opa(a,0,LV_PART_INDICATOR); lv_obj_remove_style(a,NULL,LV_PART_KNOB);}
static int heroY(void){ return c.hf==&hv_hero?-16:-22; }
static void ride(void){ page(c.bg_top,c.bg_bot);
  arc(216,8,c.low?c.low:(night?0x1E1913:0x23423E),135,405); arc(216,8,c.mid?c.mid:(night?0x3E352A:0x6F918B),135,135+155);
  T("23",c.hf,c.hero,0,heroY());
  if(c.latin_sec) T("18.6 km    0:52",c.sf,c.sec,0,heroY()+100); else T("18.6 公里   52 分钟",&gz_20,c.sec,0,70); save("ride"); }
static uint32_t ac; static int aw; static void arrowcb(lv_event_t* e){ lv_layer_t* L=lv_event_get_layer(e); lv_draw_line_dsc_t ld; lv_draw_line_dsc_init(&ld);
  ld.color=lv_color_hex(ac); ld.width=aw; ld.round_start=ld.round_end=1; int x=265,y=(c.nf==&hv_hero)?96:122;
  lv_point_precise_t p[][2]={{{x,y+62},{x,y}},{{x,y},{x-64,y}},{{x-64,y},{x-38,y-26}},{{x-64,y},{x-38,y+26}}};
  for(int i=0;i<4;i++){ld.p1=p[i][0];ld.p2=p[i][1];lv_draw_line(L,&ld);} }
static void nav(int now){ int fill=now&&c.turn_fill==1;
  if(fill) page(c.accent,c.accent); else page(c.bg_top,c.bg_bot);
  uint32_t hc = fill?0x2B1503:(now?c.accent:c.hero), sc= fill?0x5A2E08:c.sec; ac=hc; aw=c.hf==&hv_hero?22:26;
  lv_obj_add_event_cb(S,arrowcb,LV_EVENT_DRAW_POST_END,NULL); lv_obj_invalidate(S);
  if(c.nf==&hv_hero){ lv_obj_t* n=T(now?"30":"350",&hv_hero,hc,-12,30); lv_obj_update_layout(S);
      lv_obj_t* u=T("m",&hv_sm,now?hc:c.sec,0,0); lv_obj_align_to(u,n,LV_ALIGN_OUT_RIGHT_BOTTOM,6,-30);
      T(now?"左转  科技园南路":"科技园南路",&gz_20,sc,0,124); }
  else { T(now?"30 m":"350 m",c.nf,hc,0,44); T(now?"左转 科技园南路":"科技园南路",&gz_20,sc,0,108); } save(now?"nav_turn_now":"nav"); }
static void cc(void){ page(c.bg_top,c.bg_bot);
  const char* ic[4]={"\xEF\x86\x85","\xEF\x80\xA8","\xEF\x86\x86","\xEF\x87\xB6"}; int on[4]={1,1,night,0};
  for(int i=0;i<4;i++){ int x=(i%2)?78:-78,y=(i/2)?78:-78; lv_obj_t* tl=R(x,y,144,144,on[i]?c.mid:c.low,48); if(c.turn_fill==2){ lv_obj_set_style_bg_color(tl,lv_color_hex(night?0xFFE8C8:0xFFFFFF),0); lv_obj_set_style_bg_opa(tl,on[i]?(night?40:58):(night?14:20),0);}
    lv_obj_t* l=T(ic[i],c.hf==&hv_hero?&hv_icon:&gu_sm,on[i]?c.hero:c.sec,x,y);
    if(c.hf!=&hv_hero){ lv_obj_set_style_transform_scale(l,460,0); lv_obj_set_style_transform_pivot_x(l,LV_PCT(50),0); lv_obj_set_style_transform_pivot_y(l,LV_PCT(50),0);} }
  save("control_center"); }
static void all(void){ ride(); nav(0); nav(1); cc(); }
int main(void){ lv_init(); lv_tick_set_cb(tk); d=lv_display_create(W,W); static uint8_t buf[W*W*2];
  lv_display_set_buffers(d,buf,NULL,sizeof buf,LV_DISPLAY_RENDER_MODE_DIRECT); lv_display_set_flush_cb(d,flush);
  night=0;
  /* v2 baseline */
  c=(Cfg){"r0",0x17504B,0x17504B,0x123F3B,0x24706A,0x9CCBC1,0xE8EFC4,0xF79431,&gu_hero,&gu_sm,&gu_num,1,0}; all();
  /* R1: chroma down (S .55->.30), keep lightness; mint hero kept */
  c=(Cfg){"r1",0x22423F,0x22423F,0x1A3431,0x2F5752,0xA3B9B4,0xE8EFC4,0xF79431,&gu_hero,&gu_sm,&gu_num,1,0}; all();
  /* R2: white hero, Nunito 500 150px, latin secondary, soft vertical light (top lighter) */
  c=(Cfg){"r2",0x2A4D49,0x182E2B,0x1A3431,0x35605A,0xB9CCC7,0xF4F6F2,0xF79431,&hv_hero,&hv_sm,&hv_sm,1,1}; all();
  /* R3: turn-now keeps the face, accent only on arrow+number; nav distance in hero font; tiles one step */
  c=(Cfg){"r3",0x2A4D49,0x182E2B,0x203E3A,0x3A6560,0xB9CCC7,0xF4F6F2,0xF7A04A,&hv_hero,&hv_sm,&hv_hero,0,1}; all();
  /* R3 night: warm near-black, same structure; hero ~36%, secondary ~48% */
  c=(Cfg){"r4",0x2A4D49,0x182E2B,0,0,0xB9CCC7,0xF4F6F2,0xF7A04A,&hv_hero,&hv_sm,&hv_hero,2,1}; all();
  night=1; c=(Cfg){"r4",0x15110D,0x0B0907,0,0,0x7A6E5E,0x5E5245,0xA8703A,&hv_hero,&hv_sm,&hv_hero,2,1}; all(); night=0;
  night=0; c=(Cfg){"r5",0x213F3B,0x213F3B,0,0,0xB9CCC7,0xF4F6F2,0xF7A04A,&hv_hero,&hv_sm,&hv_hero,2,1}; all();
  night=1; c=(Cfg){"r5",0x100D0A,0x100D0A,0,0,0x7A6E5E,0x5E5245,0xA8703A,&hv_hero,&hv_sm,&hv_hero,2,1}; all();
  night=1; c=(Cfg){"r3",0x15110D,0x0B0907,0x0E0B08,0x2A231B,0x7A6E5E,0x5E5245,0xA8703A,&hv_hero,&hv_sm,&hv_hero,0,1}; all();
  return 0; }
