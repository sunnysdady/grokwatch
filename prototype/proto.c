// GrokWatch HomeView-round proto: real LVGL 9 render, 466 round. Standby glyphs copied from gw_ultra.c (3da3d28).
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#define W 466
LV_FONT_DECLARE(gu_hero) LV_FONT_DECLARE(gu_num) LV_FONT_DECLARE(gu_sm) LV_FONT_DECLARE(gu_lab) LV_FONT_DECLARE(gz_20)
static uint16_t fb[W*W]; static lv_display_t* d; static uint32_t tick=0; static uint32_t tk(void){return tick;}
static void flush(lv_display_t* dd,const lv_area_t* a,uint8_t* px){int w=a->x2-a->x1+1;for(int y=a->y1;y<=a->y2;y++)memcpy(&fb[y*W+a->x1],px+((long)y*W+a->x1)*2,w*2);lv_display_flush_ready(dd);}
static int night=0;
#define K4 0x3F7F79
#define MINT 0xE8EFC4
#define DIM 0x8FB3A8
#define ORG 0xF79431
/* night: map any colour to warm white #FFE8C8 by luminance, capped at ~32% */
static lv_color_t C(uint32_t h){ if(!night) return lv_color_hex(h);
  int r=h>>16&255,g=h>>8&255,b=h&255; float L=(0.3f*r+0.59f*g+0.11f*b)/255.f; if(h==0xF79431){ return lv_color_make(150,96,48);} float k=L*0.34f;
  return lv_color_make(255*k,232*k,200*k); }
static void save(const char* n){ for(int i=0;i<3;i++){tick+=50;lv_timer_handler();} lv_refr_now(d);
  char p[256]; snprintf(p,sizeof p,"/workspace/homeview-round/raw/%s_%s.ppm",n,night?"night":"day"); FILE* f=fopen(p,"wb"); fprintf(f,"P6\n%d %d\n255\n",W,W);
  for(int i=0;i<W*W;i++){uint16_t c=fb[i];fputc(((c>>11)&31)*255/31,f);fputc(((c>>5)&63)*255/63,f);fputc((c&31)*255/31,f);} fclose(f); printf("%s\n",p);}
static lv_obj_t* S; // page root
static void page(void){ if(S) lv_obj_delete(S); S=lv_obj_create(lv_screen_active()); lv_obj_remove_style_all(S); lv_obj_set_size(S,W,W);
  lv_obj_set_style_bg_color(S,lv_color_black(),0); lv_obj_set_style_bg_opa(S,255,0); lv_obj_remove_flag(S,LV_OBJ_FLAG_SCROLLABLE);}
static lv_color_t CT(uint32_t c,const lv_font_t* f){ if(!night||f==&gu_hero||f==&gu_num) return C(c); if(c==ORG) return lv_color_make(190,122,60);
  float k=c==MINT?0.52f:(c==DIM?0.45f:(c==K4?0.42f:0.30f)); return lv_color_make(255*k,232*k,200*k);}
static lv_obj_t* T(const char* s,const lv_font_t* f,uint32_t c,int x,int y){ lv_obj_t* l=lv_label_create(S); lv_label_set_text(l,s);
  lv_obj_set_style_text_font(l,f,0); lv_obj_set_style_text_color(l,CT(c,f),0); lv_obj_align(l,LV_ALIGN_CENTER,x,y); return l;}
static lv_obj_t* R(int x,int y,int w,int h,uint32_t c,int rad){ lv_obj_t* o=lv_obj_create(S); lv_obj_remove_style_all(o); lv_obj_set_size(o,w,h);
  lv_obj_align(o,LV_ALIGN_CENTER,x,y); lv_obj_set_style_radius(o,rad,0); lv_obj_set_style_bg_color(o,C(c),0); lv_obj_set_style_bg_opa(o,255,0); return o;}
static void arc(int r,int w,uint32_t c,int a0,int a1){ lv_obj_t* a=lv_arc_create(S); lv_obj_set_size(a,2*r,2*r); lv_obj_center(a); lv_arc_set_bg_angles(a,a0,a1);
  lv_obj_set_style_arc_color(a,C(c),0); lv_obj_set_style_arc_width(a,w,0); lv_obj_set_style_arc_rounded(a,1,0); lv_obj_set_style_arc_opa(a,0,LV_PART_INDICATOR);
  lv_obj_remove_style(a,NULL,LV_PART_KNOB); lv_obj_remove_flag(a,LV_OBJ_FLAG_CLICKABLE);}
/* ---- palette: one hue (teal) + mint hero + single orange accent */
#define BG 0x000000
#define K1 0x0E2F2D  /* card deep */
#define K2 0x134B49
#define K3 0x226A64
#define DIM 0x8FB3A8
#define ORG 0xF79431
/* ---- standby vector digits (copied, flaws fixed) */
#define SB_H 1.30f
#define SB_DW 118
#define SB_DH 160
#define SB_SW 26
typedef struct{char t;float a,b,c,d,e;}P; static P G[10][5],G9C[5],G9S[5];
static float tg(float cx,float cy,float r,float px,float py,int s){float dx=px-cx,dy=py-cy,dd=sqrtf(dx*dx+dy*dy);return (atan2f(dy,dx)+s*acosf(r/dd))*57.29578f;}
static void gsetup(void){ float H=SB_H; int k;
 P g0[]={{'A',.5,.5,.5,180,360},{'A',.5,H-.5,.5,0,180},{'L',0,.5,0,H-.5},{'L',1,.5,1,H-.5},{0}};
 P g1[]={{'L',.6,0,.6,H},{'L',.6,0,.2,.3},{0}};
 float t2=tg(.5,.47,.47,0,H,-1); while(t2<180)t2+=360; float x2=.5f+.47f*cosf(t2/57.29578f),y2=.47f+.47f*sinf(t2/57.29578f);
 P g2[]={{'A',.5,.47,.47,180,t2},{'L',x2,y2,0,H},{'L',0,H,1,H},{0}};
 P g3[]={{'A',.5,.30,.30,195,450},{'A',.5,H-.40,.40,270,520},{0}};
 P g4[]={{'L',.74,0,.74,H},{'L',.66,0,0,.86},{'L',0,.86,1,.86},{0}};
 /* 5 FIX: stem and bowl meet on the bowl's own arc start point (no extra +.02 jog) */
 float a5=205,bx=.5f,by=H-.46f,x5=bx+.46f*cosf(a5/57.29578f),y5=by+.46f*sinf(a5/57.29578f);
 P g5[]={{'L',.92,0,x5,0},{'L',x5,0,x5,y5},{'A',bx,by,.46,a5,505},{0}};
 float t6=tg(.5,H-.5,.5,.62,0,-1); float x6=.5f+.5f*cosf(t6/57.29578f),y6=(H-.5f)+.5f*sinf(t6/57.29578f);
 P g6[]={{'A',.5,H-.5,.5,0,360},{'L',x6,y6,.62,0},{0}};
 P g7[]={{'L',0,0,1,0},{'L',1,0,.36,H},{0}};
 /* 8 FIX: top bowl centre at r so its stroke stays inside the box; bowls share a tangent at y=.56 */
 P g8[]={{'A',.5,.29,.29,0,360},{'A',.5,.58f+.36f,.36,0,360},{0}};
 /* 9 FIX: tail ends at x=.62 (mirror of 6), never past the box */
 float t9=tg(.5,.5,.5,.40,H,-1); float x9=.5f+.5f*cosf(t9/57.29578f),y9=.5f+.5f*sinf(t9/57.29578f);
 P g9[]={{'A',.5,.5,.5,0,360},{'L',x9,y9,.40,H},{0}}; P g9c[]={{'A',.5,.5,.5,0,360},{'A',.2,.5,.8,0,72},{0}};
 P* all[10]={g0,g1,g2,g3,g4,g5,g6,g7,g8,g9}; for(int q=0;q<10;q++){for(k=0;all[q][k].t;k++)G[q][k]=all[q][k];G[q][k].t=0;} for(k=0;g9c[k].t;k++)G9C[k]=g9c[k]; G9C[k].t=0; memcpy(G9S,G[9],sizeof G9S);}
static char sbt[5]="1009";
static void glyph(lv_layer_t* L,int dg,float x,float y,lv_color_t c){ lv_draw_line_dsc_t ld; lv_draw_line_dsc_init(&ld); ld.color=c; ld.width=SB_SW; ld.round_start=ld.round_end=1;
 lv_draw_arc_dsc_t ad; lv_draw_arc_dsc_init(&ad); ad.color=c; ad.width=SB_SW; ad.rounded=1;
 float sx=SB_DW-SB_SW,sy=(SB_DH-SB_SW)/SB_H,s=sx<sy?sx:sy,ox=x+SB_SW/2.f+(SB_DW-SB_SW-s)/2,oy=y+SB_SW/2.f;
 for(int i=0;G[dg][i].t;i++){P*q=&G[dg][i]; if(q->t=='L'){ld.p1.x=ox+q->a*s;ld.p1.y=oy+q->b*s;ld.p2.x=ox+q->c*s;ld.p2.y=oy+q->d*s;lv_draw_line(L,&ld);}
  else{ad.center.x=ox+q->a*s;ad.center.y=oy+q->b*s;ad.radius=(uint16_t)(q->c*s+SB_SW/2.f);ad.start_angle=q->d;ad.end_angle=q->e;lv_draw_arc(L,&ad);}}}
static void sbdraw(lv_event_t* e){ lv_layer_t* L=lv_event_get_layer(e); float x0=233-(2*SB_DW+12)/2.f,y0=233-(2*SB_DH+16)/2.f;
 for(int r=0;r<2;r++)for(int k=0;k<2;k++)glyph(L,sbt[r*2+k]-'0',x0+k*(SB_DW+12),y0+r*(SB_DH+16),C(MINT));}
static void standby(const char* hhmm,const char* name){ page(); memcpy(sbt,hhmm,4); lv_obj_add_event_cb(S,sbdraw,LV_EVENT_DRAW_POST_END,NULL); lv_obj_invalidate(S); save(name);}
/* ---- shared chrome: small top time + status dot, follows the circle */
static void top(const char* s){ T(s,&gu_sm,DIM,0,-186); }
static void card3(const char* v[3],const char* l[3],int y){ for(int i=0;i<3;i++){int x=(i-1)*120; R(x,y,112,78,K1,28); T(v[i],&gu_sm,MINT,x,y-11); T(l[i],&gz_20,K4,x,y+18);} }
static void ride(int state){ page(); top("21:18");
 if(state==0){ /* riding */
  arc(222,8,K1,135,405); if(night) lv_obj_set_style_arc_color(lv_obj_get_child(S,-1),lv_color_make(56,51,44),0); arc(222,8,K3,135,135+170); if(night) lv_obj_set_style_arc_color(lv_obj_get_child(S,-1),lv_color_make(82,74,64),0);   /* speed arc: 23/40 */
  T("23",&gu_hero,MINT,0,-40); T("km/h",&gu_sm,K4,0,40);
  const char* v[3]={"18.6","0:52","142"}; const char* l[3]={"公里","用时","心率"}; card3(v,l,128); save("02_ride"); return; }
 if(state==1){ T("--",&gu_hero,K3,0,-40); T("等待 GPS 定位",&gz_20,DIM,0,40); R(0,128,240,64,K1,32); T("到空旷处更快",&gz_20,K4,0,128); save("03_ride_wait_gps"); return; }
 if(state==2){ T("--",&gu_hero,K2,0,-40); T("手机未连接",&gz_20,DIM,0,40); R(0,128,260,64,K1,32); T("打开 GrokWatch App",&gz_20,K4,0,128); save("04_ride_no_phone"); return; }
 if(state==3){ /* ready / empty */ T("0",&gu_hero,MINT,0,-40); T("km/h",&gu_sm,K4,0,40); R(0,128,200,72,K3,36); T("开始骑行",&gz_20,MINT,0,128); save("05_ride_ready"); return; }
 if(state==4){ /* summary */ T("骑行结束",&gz_20,DIM,0,-130); T("42",&gu_hero,MINT,0,-40); T("km",&gu_sm,K4,0,40);
  const char* v[3]={"1:48","23.4","138"}; const char* l[3]={"用时","均速","心率"}; card3(v,l,128); save("06_ride_summary"); }}
static void arrow(lv_layer_t* L,uint32_t c){ lv_draw_line_dsc_t ld; lv_draw_line_dsc_init(&ld); ld.color=C(c); ld.width=22; ld.round_start=ld.round_end=1;
 int x=253,y=150; lv_point_precise_t p[][2]={{{x,y+52},{x,y-4}},{{x,y-4},{x-58,y-4}},{{x-58,y-4},{x-34,y-28}},{{x-58,y-4},{x-34,y+20}}};
 for(int i=0;i<4;i++){ld.p1=p[i][0];ld.p2=p[i][1];lv_draw_line(L,&ld);} }
static uint32_t arrow_c; static void navdraw(lv_event_t* e){ arrow(lv_event_get_layer(e),arrow_c); }
static void nav(int st){ page(); top("21:19");
 if(st==2){ T("--",&gu_hero,K2,0,-40); T("打开高德导航后显示",&gz_20,DIM,0,40); save("08_nav_idle"); return; }
 arrow_c=st==1?ORG:MINT; R(0,-80,128,128,st==1?0x3A2410:K1,64); lv_obj_add_event_cb(S,navdraw,LV_EVENT_DRAW_POST_END,NULL);
 T(st==1?"30":"350",&gu_num,st==1?ORG:MINT,-10,30); T("m",&gu_sm,K4,st==1?46:56,40);
 T("左转 · 科技园南路",&gz_20,DIM,0,82); R(0,150,220,60,K1,30); T("12 分钟 · 21:31",&gz_20,K4,0,150);
 save(st==1?"07b_nav_turn_now":"07_nav"); }
static void cc(void){ page(); T("21:18  76%",&gu_sm,DIM,0,-186);
 const char* l[4]={"亮度 60%","音量 60%","夜间","勿扰"}; const char* ic[4]={"\xEF\x86\x85","\xEF\x80\xA8","\xEF\x86\x86","\xEF\x87\xB6"}; int on[4]={1,1,night,0};
 for(int i=0;i<4;i++){int x=(i%2)?70:-70,y=(i/2)?70:-70; R(x,y,130,130,on[i]?K3:K1,36); T(ic[i],&gu_sm,on[i]?MINT:K4,x,y-18); T(l[i],&gz_20,on[i]?MINT:K4,x,y+24);}
 T("向下滑关闭",&gz_20,K4,0,180); save("09_control_center"); }
static void todo(int empty){ page(); T("待办",&gz_20,DIM,0,-170);
 if(empty){ T("0",&gu_hero,K3,0,-30); T("说“记一下”就能添加",&gz_20,K4,0,60); save("11_todo_empty"); return; }
 const char* t[4]={"买内胎 700x25C","周六六点半集合","给尾灯充电","还张三 200 块"}; int dn[4]={0,0,1,0}; int hw[4]={150,190,190,150};
 for(int i=0;i<4;i++){int y=-108+i*66; R(0,y,2*hw[i]+20,56,dn[i]?BG:K1,28); R(-hw[i]+24,y,22,22,dn[i]?K3:BG,11);
  if(!dn[i]){lv_obj_t* o=R(-hw[i]+24,y,22,22,K1,11); lv_obj_set_style_border_width(o,3,0); lv_obj_set_style_border_color(o,night?lv_color_make(115,104,90):C(K4),0);}
  lv_obj_t* l=T(t[i],&gz_20,dn[i]?K4:MINT,0,y); lv_obj_align(l,LV_ALIGN_LEFT_MID,233-hw[i]+44,y);}
 T("还有 3 项",&gz_20,K4,0,170); save("10_todo"); }
static void grok(int st){ page();
 R(0,-40,150,150,st==2?K2:K3,75);  /* character: one soft disc, two eyes */
 int eh=st==2?8:26; R(-28,-50,16,eh,MINT,8); R(28,-50,16,eh,MINT,8);
 if(st==0){ arc(90,5,K3,0,360); arc(106,3,K2,0,360); { lv_obj_t* c=lv_obj_get_child(S,-1),*b=lv_obj_get_child(S,-2); lv_obj_align(c,LV_ALIGN_CENTER,0,-40); lv_obj_align(b,LV_ALIGN_CENTER,0,-40);} T("在听…",&gz_20,DIM,0,95); T("明天早上深圳会下雨吗？",&gz_20,MINT,0,135); save("12_grok_listening"); }
 if(st==1){ for(int i=0;i<3;i++) R(-28+i*28,80,14,14,i==1?MINT:K4,7); T("想一想",&gz_20,K4,0,120); save("13_grok_thinking"); }
 if(st==2){ R(0,-6,40,10,MINT,5); lv_obj_t* l=T("明早多云转阵雨，降雨概率 60%。\n建议带雨衣，注意刹车距离。",&gz_20,MINT,0,105);
  lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0); lv_obj_set_style_text_line_space(l,6,0); save("14_grok_speaking"); }
 if(st==3){ T("Grok 离线",&gz_20,DIM,0,80); T("检查 Wi-Fi",&gz_20,K4,0,115); save("15_grok_offline"); }}
int main(void){ lv_init(); lv_tick_set_cb(tk); d=lv_display_create(W,W); static uint8_t buf[W*W*2];
 lv_display_set_buffers(d,buf,NULL,sizeof buf,LV_DISPLAY_RENDER_MODE_DIRECT); lv_display_set_flush_cb(d,flush); gsetup();
 lv_obj_set_style_bg_color(lv_screen_active(),lv_color_black(),0);
 for(night=0;night<2;night++){ standby("1009","01_standby"); standby("5889","01b_standby_glyphs_5889"); standby("2345","01c_standby_glyphs_2345"); standby("6701","01d_standby_glyphs_6701"); standby("1959","01e_standby_9-straight"); memcpy(G[9],G9C,sizeof G9S); standby("1959","01f_standby_9-curved"); memcpy(G[9],G9S,sizeof G9S);
  for(int s=0;s<5;s++) ride(s); nav(0); nav(1); nav(2); cc(); todo(0); todo(1); for(int s=0;s<4;s++) grok(s); }
 return 0; }
