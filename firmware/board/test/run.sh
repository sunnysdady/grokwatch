#!/bin/sh
# Host unit tests (needs only a C compiler). Usage: sh run.sh
set -e; cd "$(dirname "$0")"; CC=${CC:-cc}
$CC -std=gnu11 -Wall -Wno-misleading-indentation -O1 -o test_gw_model test_gw_model.c ../gw_model.c && ./test_gw_model
$CC -std=gnu11 -Wall -O1 -o test_gw_health test_gw_health.c ../gw_health.c && ./test_gw_health
$CC -std=gnu11 -Wall -O1 -o test_gw_input test_gw_input.c ../gw_input.c && ./test_gw_input
$CC -std=gnu11 -Wall -O1 -o test_gw_orient test_gw_orient.c ../gw_orient.c && ./test_gw_orient
