// Red (PMIC M5PM1) key decoder, pure C, host-testable (sim/test_redkey.c).
// Inputs per 120 ms poll: IRQ_STATUS3 (read + cleared: b0 click, b1 wake, b2 double) and BTN_FLAG (BTN_STATUS b7,
// auto-clears on read = exactly one per physical press). Serial on 2.6.21 showed IRQ click/wake bits only arrive
// in some states (0x03 in standby, nothing while active after the first IRQ), while BTN_FLAG came every time
// (0x81). So: BTN_FLAG is the press source; IRQ bits only add/confirm a double.
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { RK_NONE=0, RK_CLICK=1, RK_DOUBLE=2 };
#define RK_DOUBLE_MS 400   // a 2nd press within this = double; a single click is emitted after this much quiet
typedef struct { int64_t pend_ms; int pending; int64_t last_dbl_ms, last_press_ms; } gw_redkey_t;
void gw_redkey_init(gw_redkey_t* k);
int gw_redkey_poll(gw_redkey_t* k, int64_t now_ms, uint8_t irq3, int flag);   // returns RK_*
#ifdef __cplusplus
}
#endif
