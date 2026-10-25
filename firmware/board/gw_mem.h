// 2.6.12: large UI buffers live in PSRAM, not internal DRAM (gw_health requires >=24 KB internal heap low-water).
#pragma once
#include <stdlib.h>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
static inline void* gw_psram_calloc(size_t n){ void* p=heap_caps_calloc(1,n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT); return p?p:calloc(1,n); }
#else
static inline void* gw_psram_calloc(size_t n){ return calloc(1,n); }
#endif
