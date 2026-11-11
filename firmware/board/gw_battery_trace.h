#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Raw evidence only: no displayed percentage or inferred reference SOC.
 * The caller owns storage and synchronization. These structs are RAM objects,
 * not an on-flash format (padding and endianness must not be serialized).
 * A new boot requires a separate trace. Never silently replace old records. */
typedef struct {
    uint64_t uptime_ms;
    uint32_t boot;
    int32_t vbat_mv, vin_mv;
    int8_t charge_pin, power_mode;
    uint8_t valid;
} gw_battery_record_t;
enum { GW_TRACE_OK, GW_TRACE_FULL, GW_TRACE_REBOOT, GW_TRACE_TIME };
typedef struct {
    gw_battery_record_t *records;
    size_t capacity, count;
    uint32_t boot;
    int error;
} gw_battery_trace_t;

/* Explicitly starting a new trace discards the previous RAM index only.
 * Do not call this to recover from an error before exporting old evidence. */
static inline bool gw_battery_trace_init(gw_battery_trace_t *t,
        gw_battery_record_t *storage, size_t capacity, uint32_t boot) {
    if (!t) return false;
    memset(t, 0, sizeof(*t));
    if (!storage || !capacity || capacity > SIZE_MAX / sizeof(*storage)) return false;
    t->records = storage; t->capacity = capacity; t->boot = boot;
    return true;
}
static inline bool gw_battery_trace_append(gw_battery_trace_t *t,
        const gw_battery_record_t *r) {
    if (!t || !r || !t->records || t->error) return false;
    if (r->boot != t->boot) { t->error = GW_TRACE_REBOOT; return false; }
    if (t->count && r->uptime_ms <= t->records[t->count - 1].uptime_ms) {
        t->error = GW_TRACE_TIME; return false;
    }
    if (t->count >= t->capacity) { t->error = GW_TRACE_FULL; return false; }
    t->records[t->count++] = *r;
    return true;
}
/* offset is also the sequence number within this boot's trace. */
static inline size_t gw_battery_trace_page(const gw_battery_trace_t *t,
        size_t offset, gw_battery_record_t *out, size_t capacity) {
    if (!t || !t->records || !out || offset >= t->count) return 0;
    size_t n = t->count - offset;
    if (n > capacity) n = capacity;
    if (n) memmove(out, t->records + offset, n * sizeof(*out));
    return n;
}
