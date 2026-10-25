#include "gw_orient.h"
int gw_orient_feed(gw_orient_t* o, float up, float mag){
  if(mag<0.6f||mag>1.4f){ o->n=0; return o->st; }   // shaking / free fall: no opinion
  int want=up>GW_ORIENT_TH?0:up<-GW_ORIENT_TH?1:o->st;
  if(want==o->st){ o->n=0; return o->st; }
  if(want!=o->cand){ o->cand=want; o->n=0; }
  if(++o->n>=GW_ORIENT_N){ o->st=want; o->n=0; }
  return o->st;
}
