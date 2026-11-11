#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "gw_power.h"

/* Portable model seam for M5PM1 voltage reads. Uses the custom OCV-SOC table
 * and explicit charge-complete concepts documented by Espressif's
 * adc_battery_estimation 0.3.0. This is not the ESP ADC driver API.
 * No default cell curve: a measured/validated profile must be supplied.
 * Percent is in hundredths; points ascend in voltage. Never treat a loaded
 * discharge trace as OCV without documenting the measurement conditions. */
typedef struct { int mv; int percent_x100; } gw_battery_point_t;
typedef struct { const gw_battery_point_t* points; size_t count; } gw_battery_curve_t;

static inline bool gw_battery_curve_valid(gw_battery_curve_t curve) {
    if (!curve.points || curve.count < 2 || curve.count > 101) return false;
    if (curve.points[0].percent_x100 != 0 ||
        curve.points[curve.count-1].percent_x100 != 10000) return false;
    for (size_t i=0; i<curve.count; ++i) {
        const gw_battery_point_t p=curve.points[i];
        if (p.mv<2500 || p.mv>4500 || p.percent_x100<0 || p.percent_x100>10000)
            return false;
        if (i && (p.mv<=curve.points[i-1].mv ||
                  p.percent_x100<curve.points[i-1].percent_x100)) return false;
    }
    return true;
}

/* -1 is unknown, not empty. Validation precedes division to reject duplicate
 * voltage knots and reversed/corrupt profiles. No temporal slew limiting. */
static inline int gw_battery_curve_percent(gw_battery_curve_t curve, int mv) {
    if (mv<2500 || mv>4500 || !gw_battery_curve_valid(curve)) return -1;
    if (mv<=curve.points[0].mv) return 0;
    for (size_t i=1; i<curve.count; ++i) {
        const gw_battery_point_t a=curve.points[i-1], b=curve.points[i];
        if (mv<=b.mv) {
            const int64_t numerator=(int64_t)(mv-a.mv)*(b.percent_x100-a.percent_x100);
            return a.percent_x100+(int)((numerator+(b.mv-a.mv)/2)/(b.mv-a.mv));
        }
    }
    return 10000;
}

enum {
    GW_BATTERY_UNKNOWN=-1, GW_BATTERY_DISCHARGING=0,
    GW_BATTERY_CHARGING=1, GW_BATTERY_CONNECTED=2, GW_BATTERY_FULL=3
};

/* full_verified must come from an independently established completion
 * signal, never merely CHG_STAT==1 or a displayed 100%. Contradictory or
 * absent VIN/CHG samples cannot be promoted to full. */
static inline int gw_battery_charge_state(int vin_mv, int charge_pin, bool full_verified) {
    int state=gw_charge_status(vin_mv,charge_pin);
    if (charge_pin!=0 && charge_pin!=1) return GW_BATTERY_UNKNOWN;
    return state==GW_BATTERY_CONNECTED && full_verified ? GW_BATTERY_FULL : state;
}
