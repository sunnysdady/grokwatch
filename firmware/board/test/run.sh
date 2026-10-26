#!/bin/sh
# Host unit tests (C compiler and Python 3). Usage: sh run.sh
set -e; cd "$(dirname "$0")"; CC=${CC:-cc}
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/grokwatch-tests.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT
trap 'exit 1' HUP INT TERM
$CC -std=gnu11 -Wall -Wno-misleading-indentation -O1 -o "$test_dir/test_gw_model" test_gw_model.c ../gw_model.c
"$test_dir/test_gw_model"
$CC -std=gnu11 -Wall -O1 -o "$test_dir/test_gw_health" test_gw_health.c ../gw_health.c
"$test_dir/test_gw_health"
$CC -std=gnu11 -Wall -O1 -o "$test_dir/test_gw_input" test_gw_input.c ../gw_input.c
"$test_dir/test_gw_input"
$CC -std=gnu11 -Wall -O1 -o "$test_dir/test_gw_orient" test_gw_orient.c ../gw_orient.c
"$test_dir/test_gw_orient"
$CC -std=gnu11 -Wall -Wextra -O1 -I.. -o "$test_dir/test_sbsm" ../sim/test_sbsm.c ../gw_sbsm.c
"$test_dir/test_sbsm"

$CC -std=gnu11 -Wall -Wextra -Werror -O1 -o "$test_dir/test_gw_power" test_gw_power.c
"$test_dir/test_gw_power"

$CC -std=gnu11 -Wall -Wextra -Werror -O1 -o "$test_dir/test_console_write" test_console_write.c
"$test_dir/test_console_write"

$CC -std=gnu11 -Wall -Wextra -Werror -O1 -o "$test_dir/test_battery" test_battery.c
"$test_dir/test_battery"
# Full LVGL rendering regression, opt in when the pinned LVGL source is installed.
if [ "${GW_TEST_UI:-0}" = 1 ]; then
  SIM_SCENARIO=--power sh ../sim/build.sh "${GW_UI_OUT:-$test_dir/ui}"
fi
