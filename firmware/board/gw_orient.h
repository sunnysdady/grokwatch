// 2.6.34 auto 180° flip: decides "upright / upside down" from the accelerometer. Pure C (host-tested in test/).
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#define GW_ORIENT_TH   0.5f   // |up| must pass 0.5 g (~30° past horizontal) to change; between = keep (hysteresis)
#define GW_ORIENT_N    6      // consecutive agreeing samples (10 Hz -> 0.6 s) before a flip (debounce)
typedef struct { int st; int cand; int n; } gw_orient_t;   // st: 0 upright, 1 flipped
// up = gravity along the screen's "top" axis in g (+1 = top edge points up), mag = |a| in g. Returns st.
int gw_orient_feed(gw_orient_t* o, float up, float mag);
#ifdef __cplusplus
}
#endif
