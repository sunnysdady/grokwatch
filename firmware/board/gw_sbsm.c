#include "gw_sbsm.h"
const char* gw_sbsm_state_str(sb_state_t s){ static const char* N[]={"active","entering","standby","waking"}; return s<=SB_WAKING?N[s]:"?"; }
void gw_sbsm_init(gw_sbsm_t* m,int64_t now){ m->st=SB_ACTIVE; m->last_input_ms=now; m->last_change_ms=now-SB_DEBOUNCE_MS; m->prev_page=1; }
static bool awake(const gw_sbsm_t* m){ return m->st==SB_ACTIVE||m->st==SB_WAKING; }
static int enter(gw_sbsm_t* m,const sb_ctx_t* c,int64_t now){ m->prev_page=c->page; m->st=SB_ENTERING; m->last_change_ms=now;
  return SB_A_SHOW|SB_A_DIM|(c->overlay_open?SB_A_CLOSE_OVERLAY:0); }
static int wake(gw_sbsm_t* m,int64_t now,int extra){ m->st=SB_WAKING; m->last_change_ms=now; m->last_input_ms=now;
  return SB_A_HIDE|SB_A_RESTORE|extra; }
// idle timeout for the current context; 0 = never auto-enter
static int64_t idle_ms(const sb_ctx_t* c){ if(c->nav_active||c->ride_active||c->voice_busy||c->overlay_open) return 0; return SB_IDLE_MS; }
int gw_sbsm_event(gw_sbsm_t* m,sb_event_t ev,const sb_ctx_t* c,int64_t now){
  bool db=now-m->last_change_ms<SB_DEBOUNCE_MS;   // a second press right after a transition is ignored
  switch(ev){
  case SB_EV_ANIM_DONE: if(m->st==SB_ENTERING) m->st=SB_STANDBY; else if(m->st==SB_WAKING) m->st=SB_ACTIVE; return 0;
  case SB_EV_TICK: if(awake(m)){ int64_t t=idle_ms(c); if(c->voice_busy||c->overlay_open||c->nav_active||c->ride_active) m->last_input_ms=now;  // exceptions defer: timer restarts after
                     if(m->st==SB_ACTIVE&&t&&now-m->last_input_ms>=t) return enter(m,c,now); } return 0;
  case SB_EV_ACTIVITY: if(awake(m)) m->last_input_ms=now; return 0;
  case SB_EV_RED_CLICK: if(db) return 0;
    if(awake(m)) return enter(m,c,now)|SB_A_HAPTIC;
    if(m->st==SB_ENTERING) return 0;   // 2.6.22: pressing red to sleep while the idle fade already runs must not wake it
    return wake(m,now,SB_A_GOTO_PREV);
  case SB_EV_TOUCH: if(awake(m)){ m->last_input_ms=now; return SB_A_PASS; }
    return wake(m,now,SB_A_SWALLOW|SB_A_GOTO_PREV);          // the waking touch is eaten, no swipe/tap reaches the page
  case SB_EV_KEY_YELLOW: if(awake(m)){ m->last_input_ms=now; return SB_A_PASS; } return wake(m,now,SB_A_GOTO_PREV);   // wake only, no page step
  case SB_EV_KEY_BLUE:   if(awake(m)){ m->last_input_ms=now; return SB_A_PASS; } return wake(m,now,SB_A_PASS);        // wake + talk at once
  case SB_EV_RED_DOUBLE: if(awake(m)){ m->last_input_ms=now; return SB_A_PASS; } return wake(m,now,SB_A_PASS);        // power menu on top
  case SB_EV_NAV_START: case SB_EV_NAV_TURN: if(awake(m)){ m->last_input_ms=now; return 0; } return wake(m,now,SB_A_GOTO_NAV);
  case SB_EV_VOICE:   if(awake(m)){ m->last_input_ms=now; return 0; } return wake(m,now,SB_A_GOTO_CHAT);
  case SB_EV_SUMMARY: if(awake(m)){ m->last_input_ms=now; return 0; } return wake(m,now,0);
  } return 0; }
