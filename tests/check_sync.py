#!/usr/bin/env python3
# check_sync.py — regla de sincronía producción <-> usdx-lab (AGENTS.md).
# Los módulos compartidos deben ser idénticos byte a byte; rx.h solo puede
# diferir en los taps del monitor (líneas con 'eng_'). menu.h, usdx_settings.h
# y el .ino divergen por diseño: se informa del tamaño de la diferencia.
import difflib, os, sys
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
rd = lambda p: open(os.path.join(root, p), encoding='utf-8').read().split('\n')
fails = 0
for f in ['cw.h', 'display.h', 'hw.h', 'i2c.h', 'lpf.h', 'perf.h', 'si5351.h', 'tx.h', 'usdx_filter.h', 'vfo.h']:
    ok = rd(f) == rd('usdx-lab/' + f)
    print(('ok   ' if ok else 'FAIL ') + f + (' identico' if ok else ' DIFIERE entre produccion y lab'))
    fails += not ok
strip = lambda ls: [l for l in ls if l.strip()]
ok = strip(rd('rx.h')) == strip([l for l in rd('usdx-lab/rx.h') if 'eng_' not in l])
print(('ok   ' if ok else 'FAIL ') + 'rx.h (DSP identico; el lab solo anade taps eng_*)')
fails += not ok
for a, b in [('menu.h', 'usdx-lab/menu.h'), ('usdx_settings.h', 'usdx-lab/usdx_settings.h'),
             ('usdx_plus_orange.ino', 'usdx-lab/usdx-lab.ino')]:
    n = sum(1 for l in difflib.unified_diff(rd(a), rd(b), lineterm='', n=0) if l[:1] in '+-' and l[:3] not in ('+++', '---'))
    print('info %s: %d lineas distintas (divergencia por diseno)' % (a, n))
sys.exit(1 if fails else 0)
