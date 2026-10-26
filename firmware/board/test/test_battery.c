#include "../gw_battery.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 gw_battery_t b={0}; assert(gw_battery_update(&b,-1,false,0)==-1);
 assert(gw_battery_update(&b,3440,false,0)==5);
 // Reproduce reported 5 -> 50% raw-voltage step in two minutes.
 for(int t=1;t<=120000;t+=100){
  int before=b.level; int n=gw_battery_update(&b,3800,true,t);
  assert(n>=before && n-before<=1); assert(n<=5+t/30000);
 }
 assert(b.level<=9);
 // Polling 1000 times must not accelerate filtering or percentage changes.
 int held=b.level,q=b.mv_q8;
 for(int i=0;i<1000;i++) assert(gw_battery_update(&b,4200,true,120000)==held);
 assert(b.mv_q8==q);
 assert(gw_battery_update(&b,9999,true,121000)==held);
 assert(gw_battery_update(&b,4200,true,3600000)<=held+1);
 int before=b.level;
 for(int t=3601000;t<3700000;t+=1000) gw_battery_update(&b,4200,false,t);
 assert(b.level==before); // Unplug/rebound is not newly stored charge.
 gw_battery_t low={0};assert(gw_battery_update(&low,3000,false,0)==0);
 gw_battery_t high={0};assert(gw_battery_update(&high,4400,true,0)==100);
 puts("Battery estimator: voltage-step, polling, invalid, sleep, unplug and bounds PASS");
}
