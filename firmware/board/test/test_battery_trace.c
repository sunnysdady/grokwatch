#include "../gw_battery_trace.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void) {
    gw_battery_record_t rows[4], page[3];
    gw_battery_trace_t t;
    assert(!gw_battery_trace_init(&t, NULL, 4, 73));
    assert(!gw_battery_trace_init(&t, rows, 0, 73));
    assert(gw_battery_trace_init(&t, rows, 4, 73));
    gw_battery_record_t r = {0};
    r.boot = 73; r.uptime_ms = 5000; r.vbat_mv = 4190;
    r.vin_mv = 5100; r.charge_pin = 0; r.power_mode = 2; r.valid = 1;
    assert(gw_battery_trace_append(&t, &r));
    r.uptime_ms = 10000; r.vin_mv = 0; r.charge_pin = 1; r.vbat_mv = 4100;
    assert(gw_battery_trace_append(&t, &r)); /* unplug is data, not a failure */
    r.uptime_ms = 15000; r.valid = 0; r.vbat_mv = -1;
    assert(gw_battery_trace_append(&t, &r)); /* keep failed reads */
    assert(gw_battery_trace_page(&t, 1, page, 3) == 2);
    assert(page[0].vin_mv == 0 && page[1].valid == 0 && page[1].vbat_mv == -1);
    assert(gw_battery_trace_page(&t, 3, page, 3) == 0);
    assert(gw_battery_trace_page(&t, (size_t)-1, page, 3) == 0);
    assert(gw_battery_trace_page(&t, 0, NULL, 3) == 0);
    r.uptime_ms = 20000; r.valid = 1; r.vbat_mv = 3390;
    assert(gw_battery_trace_append(&t, &r));
    gw_battery_record_t before[4]; memcpy(before, rows, sizeof rows);
    r.uptime_ms = 25000;
    assert(!gw_battery_trace_append(&t, &r));
    assert(t.error == GW_TRACE_FULL && t.count == 4);
    assert(memcmp(before, rows, sizeof rows) == 0); /* no ring overwrite */
    assert(gw_battery_trace_init(&t, rows, 4, 73));
    r.boot = 74;
    assert(!gw_battery_trace_append(&t, &r) && t.error == GW_TRACE_REBOOT);
    r.boot = 73;
    assert(!gw_battery_trace_append(&t, &r)); /* errors are sticky */
    assert(gw_battery_trace_init(&t, rows, 4, 73));
    assert(gw_battery_trace_append(&t, &r));
    assert(!gw_battery_trace_append(&t, &r) && t.error == GW_TRACE_TIME);
    assert(gw_battery_trace_init(&t, rows, 4, 73));
    assert(gw_battery_trace_append(&t, &r));
    r.uptime_ms--;
    assert(!gw_battery_trace_append(&t, &r) && t.error == GW_TRACE_TIME);
    puts("Battery trace: unplug, invalid reads, paging, overflow, reboot and clock regression passed");
}
