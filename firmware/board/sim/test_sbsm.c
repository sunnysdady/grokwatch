// Host unit test: every entry/exit path of the standby state machine (spec docs/relay/STANDBY-SPEC.md).
#include <stdio.h>
#include "gw_sbsm.h"
static int fails=0, n=0; static gw_sbsm_t m; static int64_t T; static sb_ctx_t c;
#define CHK(name,cond) do{ n++; if(cond) printf("OK   %s\n",name); else { fails++; printf("FAIL %s (st=%s)\n",name,gw_sbsm_state_str(m.st)); } }while(0)
static int ev(sb_event_t e){ return gw_sbsm_event(&m,e,&c,T); }
static void reset(int page){ T=100000; gw_sbsm_init(&m,T); sb_ctx_t z={0}; c=z; c.page=page; }
static void idle_to_standby(void){ for(int i=0;i<25;i++){ T+=1000; ev(SB_EV_TICK);} T+=400; ev(SB_EV_ANIM_DONE); }
static void advance(int ms){ T+=ms; }
int main(void){ int a;
  // ---- entry ----
  reset(2); for(int i=0;i<19;i++){ T+=1000; ev(SB_EV_TICK);} CHK("idle 19s: still active",m.st==SB_ACTIVE);
  T+=1000; a=ev(SB_EV_TICK); CHK("idle 20s -> entering, SHOW+DIM",m.st==SB_ENTERING&&(a&SB_A_SHOW)&&(a&SB_A_DIM));
  ev(SB_EV_ANIM_DONE); CHK("fade done -> standby, prev page kept",m.st==SB_STANDBY&&m.prev_page==2);
  reset(1); T+=15000; ev(SB_EV_ACTIVITY); T+=15000; ev(SB_EV_TICK); CHK("touch activity restarts idle timer",m.st==SB_ACTIVE);
  const char* ex[]={"nav active","ride active","voice busy","overlay open"};
  for(int k=0;k<4;k++){ reset(1); if(k==0)c.nav_active=1; if(k==1)c.ride_active=1; if(k==2)c.voice_busy=1; if(k==3)c.overlay_open=1;
    for(int i=0;i<120;i++){ T+=1000; ev(SB_EV_TICK);} char b[64]; snprintf(b,64,"exception %s: no auto standby 120s",ex[k]); CHK(b,m.st==SB_ACTIVE);
    c.nav_active=c.ride_active=c.voice_busy=c.overlay_open=0; for(int i=0;i<19;i++){ T+=1000; ev(SB_EV_TICK);} snprintf(b,64,"after %s ends: full 20s again",ex[k]); CHK(b,m.st==SB_ACTIVE); }
  reset(3); advance(300); a=ev(SB_EV_RED_CLICK); CHK("red click -> enter + haptic",m.st==SB_ENTERING&&(a&SB_A_SHOW)&&(a&SB_A_HAPTIC));
  reset(1); c.overlay_open=1; advance(300); a=ev(SB_EV_RED_CLICK); CHK("red with control center open -> closes overlay",(a&SB_A_CLOSE_OVERLAY)!=0);
  reset(1); c.nav_active=1; advance(300); a=ev(SB_EV_RED_CLICK); CHK("red during nav still enters (manual)",m.st==SB_ENTERING);
  // ---- exit ----
  reset(3); idle_to_standby(); advance(300); a=ev(SB_EV_TOUCH); CHK("touch wakes, swallowed, back to prev page",m.st==SB_WAKING&&(a&SB_A_SWALLOW)&&(a&SB_A_GOTO_PREV)&&(a&SB_A_HIDE)&&(a&SB_A_RESTORE)&&!(a&SB_A_PASS));
  ev(SB_EV_ANIM_DONE); CHK("wake fade done -> active",m.st==SB_ACTIVE);
  reset(2); idle_to_standby(); advance(300); a=ev(SB_EV_RED_CLICK); CHK("red click wakes to prev",m.st==SB_WAKING&&(a&SB_A_GOTO_PREV));
  reset(2); idle_to_standby(); a=ev(SB_EV_KEY_YELLOW); CHK("yellow wakes only (no page step)",(a&SB_A_HIDE)&&!(a&SB_A_PASS)&&(a&SB_A_GOTO_PREV));
  reset(2); idle_to_standby(); a=ev(SB_EV_KEY_BLUE); CHK("blue wakes + talk passes",(a&SB_A_HIDE)&&(a&SB_A_PASS));
  reset(2); idle_to_standby(); a=ev(SB_EV_RED_DOUBLE); CHK("red double wakes + power menu",(a&SB_A_HIDE)&&(a&SB_A_PASS));
  reset(2); idle_to_standby(); a=ev(SB_EV_NAV_START); CHK("new nav wakes to nav page",(a&SB_A_HIDE)&&(a&SB_A_GOTO_NAV));
  reset(2); idle_to_standby(); a=ev(SB_EV_NAV_TURN); CHK("nav turn wakes to nav page",(a&SB_A_HIDE)&&(a&SB_A_GOTO_NAV));
  reset(2); idle_to_standby(); a=ev(SB_EV_VOICE); CHK("voice reply wakes to chat page",(a&SB_A_HIDE)&&(a&SB_A_GOTO_CHAT));
  reset(2); idle_to_standby(); a=ev(SB_EV_SUMMARY); CHK("ride summary wakes (stays, sheet shows)",(a&SB_A_HIDE));
  // ---- awake keys keep normal job ----
  reset(1); CHK("awake yellow passes",ev(SB_EV_KEY_YELLOW)&SB_A_PASS); CHK("awake blue passes",ev(SB_EV_KEY_BLUE)&SB_A_PASS); CHK("awake touch passes",ev(SB_EV_TOUCH)&SB_A_PASS);
  reset(1); CHK("awake nav frame: no action",ev(SB_EV_NAV_TURN)==0);
  // ---- debounce / interruption ----
  reset(1); advance(300); ev(SB_EV_RED_CLICK); advance(100); a=ev(SB_EV_RED_CLICK); CHK("red x2 within 250ms ignored",a==0&&m.st==SB_ENTERING);
  advance(200); a=ev(SB_EV_RED_CLICK); CHK("red during enter fade: keeps entering (no accidental wake)",m.st==SB_ENTERING&&a==0);
  reset(1); for(int i=0;i<20;i++){T+=1000; ev(SB_EV_TICK);} advance(270); a=ev(SB_EV_RED_CLICK); CHK("red 270ms after idle-enter starts: stays going to standby",m.st==SB_ENTERING&&a==0);
  reset(1); idle_to_standby(); ev(SB_EV_TOUCH); a=ev(SB_EV_TOUCH); CHK("second touch during wake fade: no re-enter",m.st==SB_WAKING&&!(a&SB_A_SHOW));
  reset(1); idle_to_standby(); a=ev(SB_EV_TICK); CHK("standby tick: no action",a==0&&m.st==SB_STANDBY);
  reset(1); idle_to_standby(); ev(SB_EV_TOUCH); for(int i=0;i<5;i++){T+=1000; ev(SB_EV_TICK);} CHK("after wake: idle timer fresh",m.st!=SB_ENTERING);
  // ---- 2.6.37 deep standby: screen off after 10 min in standby, keys (not touch) wake ----
  reset(2); idle_to_standby(); for(int i=0;i<594;i++){ T+=1000; a=ev(SB_EV_TICK); } CHK("599.4s since enter: not deep yet",!m.deep&&!(a&SB_A_DEEP));
  T+=1000; a=ev(SB_EV_TICK); CHK("600.4s since enter -> DEEP once",m.deep&&a==SB_A_DEEP&&m.st==SB_STANDBY);
  T+=1000; a=ev(SB_EV_TICK); CHK("deep: next tick no action",a==0);
  advance(300); a=ev(SB_EV_TOUCH); CHK("deep: touch ignored",a==0&&m.st==SB_STANDBY&&m.deep);
  a=ev(SB_EV_RED_CLICK); CHK("deep: red wakes + UNDEEP",m.st==SB_WAKING&&(a&SB_A_UNDEEP)&&(a&SB_A_HIDE)&&!m.deep);
  reset(2); idle_to_standby(); for(int i=0;i<600;i++){ T+=1000; ev(SB_EV_TICK); } a=ev(SB_EV_KEY_BLUE); CHK("deep: blue wakes + talk + UNDEEP",(a&SB_A_UNDEEP)&&(a&SB_A_PASS));
  reset(2); idle_to_standby(); for(int i=0;i<600;i++){ T+=1000; ev(SB_EV_TICK); } a=ev(SB_EV_NAV_TURN); CHK("deep: nav turn wakes + UNDEEP",(a&SB_A_UNDEEP)&&(a&SB_A_GOTO_NAV));
  reset(2); idle_to_standby(); a=ev(SB_EV_TOUCH); CHK("plain standby wake: no UNDEEP",!(a&SB_A_UNDEEP));
  printf("SBSM %d/%d passed\n",n-fails,n); return fails?1:0; }
