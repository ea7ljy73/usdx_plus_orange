#!/usr/bin/env python3
# gen_rxtx.py - extrae switch_rxtx() REAL de hw.h (produccion) y de
# usdx-legazy.ino a rxtx_under_test.h, cada una en su namespace, para
# ejecutarlas sobre el mismo estado simulado y comparar todo lo que hacen.
import sys
def func(path, sig):
    src = open(path, encoding='utf-8', errors='replace').read()
    i = src.index(sig)
    j = src.index('{', i)
    depth, k = 0, j
    while True:
        if src[k] == '{': depth += 1
        elif src[k] == '}':
            depth -= 1
            if depth == 0: break
        k += 1
    return src[i:k + 1]
lab = len(sys.argv) > 1 and sys.argv[1] == 'lab'
prod = func('../../usdx-lab/hw.h' if lab else '../../hw.h', 'void switch_rxtx(uint8_t tx_enable)')
leg = func('../../usdx-legazy/usdx-legazy.ino', 'void switch_rxtx(uint8_t tx_enable)')
open('rxtx_under_test.h', 'w').write(
    '// GENERADO por gen_rxtx.py - NO editar.\nnamespace prod {\n%s\n}\nnamespace legacy {\n%s\n}\n' % (prod, leg))
print('rxtx_under_test.h: switch_rxtx produccion (%d lineas) y legacy (%d lineas)' % (prod.count('\n') + 1, leg.count('\n') + 1))
