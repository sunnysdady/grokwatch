#!/bin/sh
# Host LVGL simulator for the GrokWatch UI. No hidden deps: builds LVGL from the repo's managed_components.
# Prereqs: cc, python3 + Pillow, and `idf.py reconfigure` once (populates managed_components).
# Usage: sh main/boards/m5stack/stopwatch/sim/build.sh [outdir]
set -e
# -m32: build 32-bit like the ESP32 (cbin_font assumes 32-bit pointers; 64-bit segfaults)
HERE=$(cd "$(dirname "$0")" && pwd); B=$(cd "$HERE/.." && pwd); ROOT=$(cd "$B/../../../.." && pwd)
MC=$ROOT/managed_components; LV=$MC/lvgl__lvgl; FT=$MC/78__xiaozhi-fonts
OUT=${1:-$HERE/out}; W=${SIM_WORK:-$HERE/.work}; if [ -n "$SIM64" ]; then CC="${CC:-cc}"; X=-DGW_SIM_NO_CBIN; else CC="${CC:-cc} -m32"; X=; fi; CF="-O1 -w $X -I$HERE -I$B -I$FT/include -I$LV -DLV_CONF_INCLUDE_SIMPLE -DLV_LVGL_H_INCLUDE_SIMPLE"
for d in "$LV/src" "$FT/cbin/font_noto_sans_common_30_4.bin"; do [ -e "$d" ] || { echo "missing $d — run idf.py reconfigure first"; exit 1; }; done
mkdir -p "$W/lv" "$OUT"
if [ ! -f "$W/liblv.a" ]; then echo "building LVGL..."; n=0
  find "$LV/src" -name '*.c' | while read f; do o="$W/lv/$(echo "$f" | md5sum | cut -c1-12).o"; $CC $CF -c "$f" -o "$o"; done
  ar rcs "$W/liblv.a" "$W"/lv/*.o; fi
$CC $CF -DGW_FONT_BIN="\"$FT/cbin/font_noto_sans_common_30_4.bin\"" -o "$W/sim" "$HERE/sim.c" "$B"/gw_ultra.c "$B"/gw_grok.c "$B"/gw_model.c \
  "$B"/gu_*.c "$B"/gz_*.c "$FT/src/cbin_font.c" "$W/liblv.a" -lm
rm -f "$OUT"/*.png; "$W/sim" "$OUT"; ${PYTHON:-python3} "$HERE/mkimg.py" "$OUT"; echo "screens: $OUT  sheet: $OUT/contact_sheet.png"
