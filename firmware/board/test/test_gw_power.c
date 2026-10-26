#include "../gw_power.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 gw_power_t p={0};
 assert(!gw_power_update(&p,true,false,0));
 assert(gw_power_update(&p,true,true,100));
 assert(gw_power_visible(&p,100,false));
 assert(!gw_power_visible(&p,100,true));
 assert(!gw_power_update(&p,true,true,1000));
 assert(gw_power_visible(&p,8099,false));
 assert(!gw_power_visible(&p,8100,false));
 assert(!gw_power_update(&p,true,false,8200));
 assert(!gw_power_visible(&p,8200,false));
 assert(gw_power_update(&p,true,true,8300));
 p.until=0; assert(!gw_power_visible(&p,8400,false));
 assert(!gw_power_update(&p,false,false,8500));
 assert(!gw_power_update(&p,true,true,8600)); /* read failure must not retrigger */
 assert(gw_charge_status(5000,0)==1);
 assert(gw_charge_status(5000,1)==2);
 assert(gw_charge_status(0,0)==0);
 assert(gw_charge_status(4000,0)==0);
 assert(gw_charge_status(4001,0)==1);
 assert(gw_charge_status(-1,0)==-1);
 assert(gw_charge_status(5000,-1)==-1);
 assert(gw_charge_status(0,-1)==0);
 puts("Power policy and sensor states passed");
}
