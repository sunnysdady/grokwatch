#!/usr/bin/env python3
"""Run against pristine pinned upstream and LVGL v9.5.0 (already downloaded).
Use --unpatched for the negative control: it MUST fail under AddressSanitizer.
"""
import argparse
import concurrent.futures
import os
from pathlib import Path
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('--lvgl', required=True, type=Path)
p.add_argument('--upstream', required=True, type=Path)
p.add_argument('--unpatched', action='store_true')
a = p.parse_args()
here = Path(__file__).resolve().parent
board = here.parent
repo = board.parents[1]
lvgl = a.lvgl.resolve()
upstream = a.upstream.resolve()
source = Path('main/display/lvgl_display/lvgl_display.cc')
if not (lvgl / 'src/font/lv_font.c').exists():
    raise SystemExit('LVGL source missing')
with tempfile.TemporaryDirectory(prefix='gw-font-') as temp:
    work = Path(temp)
    target = work / source
    target.parent.mkdir(parents=True)
    target.write_text((upstream / source).read_text())
    if not a.unpatched:
        subprocess.run(['git', 'apply', '--include=' + str(source),
                        str(repo / 'firmware-patch/upstream-outside-board.patch')], cwd=work, check=True)
    content = target.read_text()
    start = content.index('bool LvglDisplay::SetTextFont(')
    end = content.index('\nbool LvglDisplay::AddTextGlyphs(', start)
    (work / 'set_text_font.inc').write_text(content[start:end])
    nav = (board / 'nav_ui.cc').read_text()
    cb_start = nav.index('static void on_text_font_changed(')
    cb_end = nav.index('\nstatic void build_ui()', cb_start)
    (work / 'font_changed.inc').write_text(nav[cb_start:cb_end])
    flags = ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
             '-DCONFIG_BOARD_TYPE_M5STACK_STOPWATCH=1', '-DLV_CONF_INCLUDE_SIMPLE',
             '-DLV_LVGL_H_INCLUDE_SIMPLE', '-I' + str(board / 'sim'), '-I' + str(board),
             '-I' + str(lvgl), '-I' + str(work),
             '-I' + str(upstream / source.parent)]
    sources = sorted((lvgl / 'src').rglob('*.c')) + [board / n for n in
        ['gw_ultra.c', 'gw_grok.c', 'gw_model.c', 'gz_20.c']] + sorted(board.glob('gu_*.c'))
    def compile_one(item):
        index, path = item
        obj = work / (str(index) + '.o')
        subprocess.run([os.environ.get('CC', 'cc'), '-std=gnu11', *flags,
                        '-c', str(path), '-o', str(obj)], check=True, capture_output=True)
        return str(obj)
    try:
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            objects = list(pool.map(compile_one, enumerate(sources)))
    except subprocess.CalledProcessError as e:
        print(e.stderr.decode(errors='replace'))
        raise SystemExit(e.returncode)
    exe = work / 'font-lifetime'
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', *flags,
                    str(here / 'font_lifetime.cpp'), *objects, '-lm', '-o', str(exe)], check=True)
    result = subprocess.run([str(exe)], env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
    if result.returncode:
        raise SystemExit(result.returncode)
    # The submitted reproducer demonstrates the original UAF, independently of
    # the callback test above. Its fix mode intentionally uses a static font.
    repro_obj = work / 'repro.o'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=gnu11', *flags,
                    '-c', str(board / 'sim/repro_font_uaf.c'), '-o', str(repro_obj)], check=True)
    repro = work / 'repro'
    subprocess.run([os.environ.get('CXX', 'c++'), *flags, str(repro_obj), *objects,
                    '-lm', '-o', str(repro)], check=True)
    env = {**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0', 'NOPOISON': '1'}
    bug = subprocess.run([str(repro), 'bug'], env=env, capture_output=True)
    assert bug.returncode != 0 and b'heap-use-after-free' in bug.stderr, bug.stderr.decode()
    print('Submitted reproducer: genuine ASan heap-use-after-free without manual poisoning')
    subprocess.run([str(repro), 'fix'], env=env, check=True)

