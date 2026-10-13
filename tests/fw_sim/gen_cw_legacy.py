#!/usr/bin/env python3
# gen_cw_legacy.py - extrae el decoder CW REAL de usdx-legazy.ino (etapa de
# audio cw_decode + printsym + dec2 OLD_CW) a cw_legacy.h, dentro de
# namespace legacy, para compararlo paso a paso con cw.h de produccion.
import re, sys
src = open('../../usdx-legazy/usdx-legazy.ino', encoding='utf-8', errors='replace').read().split('\n')

def find(pred, start=0):
    for i in range(start, len(src)):
        if pred(src[i]):
            return i
    sys.exit('gen_cw_legacy: marcador no encontrado')

a0 = find(lambda l: l.startswith('static int32_t avg = 256;'))
a1 = find(lambda l: l.startswith('//#define NEW_CW'), a0)
b0 = find(lambda l: l.startswith('#else // OLD_CW'), a1) + 1
b1 = find(lambda l: l.startswith('#endif //OLD_CW'), b0)
m = find(lambda l: l.startswith('const char m2c[] PROGMEM'))

out = ['// GENERADO por gen_cw_legacy.py desde usdx-legazy.ino - NO editar.',
       '#pragma once',
       'namespace legacy {',
       'static uint8_t menumode = 1; // fuera de MAIN: sin escrituras LCD del decoder',
       'static inline void stepsize_showcursor() {}',
       'void dec2();',
       src[m]]
out += src[a0:a1]
out += src[b0:b1]
out += ['} // namespace legacy', '']
open('cw_legacy.h', 'w').write('\n'.join(out))
print('cw_legacy.h: %d lineas de legacy (%d-%d, %d-%d)' % (a1 - a0 + b1 - b0, a0 + 1, a1, b0 + 1, b1))
