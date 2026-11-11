#pragma once
#include <stdbool.h>
#include <stdint.h>
/* UI policy only. Charge-pin inactive does not prove external power or full charge. */
typedef struct { bool charging; int64_t until; } gw_power_t;
static inline bool gw_power_update(gw_power_t* p, bool valid, bool charging, int64_t now) {
    if (!valid) { p->until = 0; return false; }
    bool started = charging && !p->charging;
    p->charging = charging;
    if (started) p->until = now + 8000;
    if (!charging) p->until = 0;
    return started;
}
static inline bool gw_power_visible(const gw_power_t* p, int64_t now, bool busy) {
    return p->charging && now < p->until && !busy;
}

/* 0 battery, 1 actively charging, 2 external supply without active charge,
 * -1 incomplete sensor sample. Never infer full charge from CHG_STAT alone. */
static inline int gw_charge_status(int vin_mv, int charge_pin) {
    if (vin_mv < 0) return -1;
    if (vin_mv <= 4000) return 0;
    if (charge_pin == 0) return 1;
    return charge_pin == 1 ? 2 : -1;
}

/* Sequential I2C reads are not atomic. Accept only a bounded window whose
 * bracketing supply/CHG readings agree; failures must not enter the estimator. */
static inline bool gw_power_sample_valid(int mv, int vin_before, int pin_before,
                                         int vin_after, int pin_after, int64_t span_ms) {
    return mv >= 2500 && mv <= 4500 && span_ms >= 0 && span_ms <= 100 &&
           vin_before >= 0 && vin_after >= 0 &&
           (pin_before == 0 || pin_before == 1) && pin_before == pin_after &&
           (vin_before > 4000) == (vin_after > 4000);
}
