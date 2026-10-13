#!/usr/bin/env python3
# gen_si5351_legacy.py - extrae la clase SI5351 REAL de usdx-legazy.ino
# (rama NEW_TX activa) a si5351_legacy.h dentro de namespace legacy, con su
# propio bus I2C grabador, para compararla con si5351.h de produccion.
import sys
src = open('../../usdx-legazy/usdx-legazy.ino', encoding='utf-8', errors='replace').read().split('\n')
a = next(i for i, l in enumerate(src) if l.startswith('I2C i2c;'))
assert src[a + 1].startswith('class SI5351 {'), 'marcador SI5351 no encontrado'
b = next(i for i in range(a, len(src)) if src[i].startswith('static SI5351 si5351;'))
out = ['// GENERADO por gen_si5351_legacy.py desde usdx-legazy.ino - NO editar.', '#pragma once',
       '#undef BB0', '#undef BB1', '#undef BB2', '#undef FAST', '#undef _MSC', '#undef SI_CLK_OE',
       'namespace legacy {', 'I2CRec i2c;'] + src[a + 1:b] + ['SI5351 si5351;', '} // namespace legacy', '']
open('si5351_legacy.h', 'w').write('\n'.join(out))
print('si5351_legacy.h: clase SI5351 de legacy (%d-%d)' % (a + 2, b))
