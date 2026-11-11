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
 assert(gw_power_sample_valid(4100,5000,0,5100,0,10));
 assert(gw_power_sample_valid(3700,0,1,0,1,100));
 assert(!gw_power_sample_valid(4100,-1,0,5000,0,10));
 assert(!gw_power_sample_valid(4100,5000,0,-1,0,10));
 assert(!gw_power_sample_valid(4100,5000,-1,5000,-1,10));
 assert(!gw_power_sample_valid(4100,5000,0,5000,1,10));
 assert(!gw_power_sample_valid(4100,5000,1,0,1,10));
 assert(!gw_power_sample_valid(4100,0,1,5000,1,10));
 assert(!gw_power_sample_valid(0,5000,0,5000,0,10));
 assert(!gw_power_sample_valid(4600,5000,0,5000,0,10));
 assert(!gw_power_sample_valid(4100,5000,0,5000,0,101));
 assert(!gw_power_sample_valid(4100,5000,0,5000,0,-1));
 puts("Power policy, sensor states and bounded sample checks passed");
}
