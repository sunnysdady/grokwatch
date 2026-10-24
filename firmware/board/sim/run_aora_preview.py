#!/usr/bin/env python3
"""Build real LVGL + Grok UI with sanitizers; render without touching hardware."""
import argparse
import concurrent.futures
import os
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--lvgl', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
here = Path(__file__).resolve().parent
board = here.parent
lvgl = a.lvgl.resolve()
out = a.output.resolve()
obj = out / 'obj'
obj.mkdir(parents=True, exist_ok=True)
flags = ['-std=gnu11', '-O1', '-g', '-fsanitize=address,undefined',
         '-fno-omit-frame-pointer', '-DLV_CONF_INCLUDE_SIMPLE', '-DLV_LVGL_H_INCLUDE_SIMPLE',
         '-I' + str(here), '-I' + str(board), '-I' + str(lvgl)]
sources = sorted((lvgl / 'src').rglob('*.c')) + [board / 'gw_grok.c', board / 'gz_20.c', here / 'aora_preview.c']
def compile_one(item):
    index, source = item
    dest = obj / f'{index}.o'
    # LVGL is fixed at the supplied checkout. Always rebuild the changing UI.
    if source.is_relative_to(board) or not dest.exists():
        subprocess.run(['cc', *flags, '-c', str(source), '-o', str(dest)], check=True, capture_output=True)
    return str(dest)
try:
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        objects = list(pool.map(compile_one, enumerate(sources)))
except subprocess.CalledProcessError as exc:
    print(exc.stderr.decode(errors='replace'))
    raise SystemExit(exc.returncode)
exe = out / 'preview'
subprocess.run(['cc', *flags, *objects, '-lm', '-o', str(exe)], check=True)
subprocess.run([str(exe), str(out)], check=True, env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
