#include "gw_redkey.h"
void gw_redkey_init(gw_redkey_t* k){ k->pend_ms=0; k->pending=0; k->last_dbl_ms=-100000; k->last_press_ms=-100000; }
int gw_redkey_poll(gw_redkey_t* k, int64_t now, uint8_t irq3, int flag){
  if(irq3&0x04){                                   // PMIC double: one power-menu event per double
    int dup = now-k->last_dbl_ms < 2*RK_DOUBLE_MS; k->pending=0; k->last_dbl_ms=now; return dup?RK_NONE:RK_DOUBLE; }
  if(flag){ int64_t gap=now-k->last_press_ms; k->last_press_ms=now; (void)gap;
    if(k->pending && now-k->pend_ms < RK_DOUBLE_MS){ k->pending=0; k->last_dbl_ms=now; return RK_DOUBLE; }
    if(now-k->last_dbl_ms < RK_DOUBLE_MS) return RK_NONE;   // tail of a double already reported
    k->pending=1; k->pend_ms=now; return RK_NONE; }
  // IRQ click/wake alone (flag missed): treat as a press only if nothing pending/just emitted
  // (same press reports its IRQ up to ~0.5 s after its flag -> ignore IRQ within 1 s of a flag)
  if((irq3&0x03) && !k->pending && now-k->last_dbl_ms >= RK_DOUBLE_MS && now-k->last_press_ms >= 1000){ k->last_press_ms=now; k->pending=1; k->pend_ms=now; return RK_NONE; }
  if(k->pending && now-k->pend_ms >= RK_DOUBLE_MS){ k->pending=0; return RK_CLICK; }
  return RK_NONE; }
