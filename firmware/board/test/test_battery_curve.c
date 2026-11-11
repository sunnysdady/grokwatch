#include "../gw_battery_curve.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

int main(void) {
    /* Synthetic nonlinear fixture for arithmetic, NOT a production cell profile. */
    const gw_battery_point_t points[]={{3300,0},{3600,2000},{3800,5000},{4200,10000}};
    const gw_battery_curve_t curve={points,4};
    assert(gw_battery_curve_valid(curve));
    for (size_t i=0;i<4;++i)
        assert(gw_battery_curve_percent(curve,points[i].mv)==points[i].percent_x100);
    assert(gw_battery_curve_percent(curve,3500)==1333);
    assert(gw_battery_curve_percent(curve,3700)==3500);
    assert(gw_battery_curve_percent(curve,4000)==7500);
    assert(gw_battery_curve_percent(curve,3000)==0);
    assert(gw_battery_curve_percent(curve,4400)==10000);
    assert(gw_battery_curve_percent(curve,INT_MIN)==-1);
    assert(gw_battery_curve_percent(curve,INT_MAX)==-1);
    int previous=-1;
    for(int mv=2500;mv<=4500;++mv) {
        int value=gw_battery_curve_percent(curve,mv);
        assert(value>=previous && value>=0 && value<=10000);
        previous=value;
    }
    gw_battery_point_t bad[]={{3300,0},{3300,10000}};
    assert(!gw_battery_curve_valid((gw_battery_curve_t){bad,2}));
    assert(gw_battery_curve_percent((gw_battery_curve_t){bad,2},3300)==-1);
    bad[1].mv=3200;
    assert(!gw_battery_curve_valid((gw_battery_curve_t){bad,2}));
    bad[1].mv=4200; bad[1].percent_x100=9900;
    assert(!gw_battery_curve_valid((gw_battery_curve_t){bad,2}));
    assert(!gw_battery_curve_valid((gw_battery_curve_t){NULL,2}));
    assert(!gw_battery_curve_valid((gw_battery_curve_t){points,1}));
    assert(!gw_battery_curve_valid((gw_battery_curve_t){points,102}));
    const gw_battery_point_t reversed[]={{3300,0},{3500,5000},{3800,4000},{4200,10000}};
    assert(!gw_battery_curve_valid((gw_battery_curve_t){reversed,4}));
    const gw_battery_point_t plateau[]={{3300,0},{3500,0},{4200,10000}};
    assert(gw_battery_curve_percent((gw_battery_curve_t){plateau,3},3400)==0);
    for(int proof=0;proof<=1;++proof) {
        assert(gw_battery_charge_state(-1,1,proof)==GW_BATTERY_UNKNOWN);
        assert(gw_battery_charge_state(5000,-1,proof)==GW_BATTERY_UNKNOWN);
        assert(gw_battery_charge_state(0,-1,proof)==GW_BATTERY_UNKNOWN);
        assert(gw_battery_charge_state(0,1,proof)==GW_BATTERY_DISCHARGING);
        assert(gw_battery_charge_state(5000,0,proof)==GW_BATTERY_CHARGING);
    }
    assert(gw_battery_charge_state(5000,1,false)==GW_BATTERY_CONNECTED);
    assert(gw_battery_charge_state(5000,1,true)==GW_BATTERY_FULL);
    puts("Battery curve/state: nonlinear interpolation, 2001-point sweep, invalid profiles and full-proof gates PASS");
}
