// Host test: red key driver layer (PMIC raw values -> click/double) feeding the real standby state machine.
#include <stdio.h>
#include "gw_redkey.h"
#include "gw_sbsm.h"
static int fails=0,n=0; static int64_t T; static gw_redkey_t k; static gw_sbsm_t m; static sb_ctx_t c;
#define CHK(name,cond) do{ n++; if(cond) printf("OK   %s\n",name); else { fails++; printf("FAIL %s\n",name);} }while(0)
static int clicks, dbls;
static void poll(uint8_t irq,int flag){ int r=gw_redkey_poll(&k,T,irq,flag);
  if(r==RK_CLICK){ clicks++; gw_sbsm_event(&m,SB_EV_RED_CLICK,&c,T); }
  if(r==RK_DOUBLE){ dbls++; gw_sbsm_event(&m,SB_EV_RED_DOUBLE,&c,T); }
  T+=120; }
static void idle(int ms){ for(int t=0;t<ms;t+=120){ poll(0,0); if(m.st==SB_ENTERING||m.st==SB_WAKING) gw_sbsm_event(&m,SB_EV_ANIM_DONE,&c,T);} }
static void reset(void){ T=100000; gw_redkey_init(&k); gw_sbsm_init(&m,T); sb_ctx_t z={0}; c=z; c.page=1; clicks=dbls=0; }
// one physical press as seen on 2.6.21 hardware: flag on the next poll; IRQ bits (variant) a few polls later
static void press(uint8_t irq_late,int late_polls){ poll(0,1); for(int i=0;i<late_polls;i++) poll(0,0); if(irq_late) poll(irq_late,0); }
int main(void){ char b[96];
  const struct { uint8_t irq; int late; const char* nm; } V[]={{0,0,"flag only (0x81, active)"},{0x03,1,"flag + 0x03 late"},{0x02,3,"flag + 0x02 (wake) late"},{0x01,0,"flag + 0x01 next poll"},{0x03,4,"flag + 0x03 480ms late"}};
  for(int v=0;v<5;v++){ reset(); int ok=1;
    for(int i=0;i<10;i++){ int before=m.st==SB_ACTIVE; press(V[v].irq,V[v].late); idle(900); int after=m.st==SB_ACTIVE; if(before==after) ok=0; }
    snprintf(b,96,"10 single clicks each toggle standby: %s (clicks=%d dbl=%d)",V[v].nm,clicks,dbls); CHK(b,ok&&clicks==10&&dbls==0); }
  reset(); poll(0x02,0); idle(900); CHK("IRQ 0x02 alone (flag lost) = 1 click",clicks==1&&dbls==0);
  reset(); poll(0x03,0); idle(900); CHK("IRQ 0x03 alone = 1 click",clicks==1);
  reset(); poll(0,1); poll(0,0); poll(0,1); idle(300); poll(0x04,0); idle(900); CHK("double (2 flags 240ms + PMIC 0x04) = 1 double, 0 click",dbls==1&&clicks==0);
  reset(); poll(0x04,0); idle(900); CHK("PMIC 0x04 alone = 1 double",dbls==1&&clicks==0);
  reset(); poll(0x07,1); idle(900); CHK("raw 0x07 = double",dbls==1&&clicks==0);
  reset(); press(0,0); idle(900); press(0,0); idle(900); CHK("two clicks 1 s apart = 2 clicks",clicks==2&&dbls==0);
  reset(); for(int i=0;i<50;i++) poll(0,0); CHK("no input = nothing",clicks==0&&dbls==0);
  printf("REDKEY %d/%d passed\n",n-fails,n); return fails?1:0; }
