#!/usr/bin/env python3
# gen_ui.py <prod|lab> - extrae del .ino REAL las funciones de UI que se
# simulan en host (display_tick, on_rit) a ui_under_test.h y comprueba por
# lectura del fuente los invariantes de arranque/loop que no se pueden ejecutar.
import re, sys
target = sys.argv[1] if len(sys.argv) > 1 else 'prod'
path = {'prod': '../../usdx_plus_orange.ino', 'lab': '../../usdx-lab/usdx-lab.ino'}[target]
src = open(path, encoding='utf-8').read()

def func(sig):
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

body = {n: func(s) for n, s in (('display_tick', 'void display_tick()'), ('on_rit', 'void on_rit()'), ('on_vfosel', 'void    on_vfosel()'),
                                 ('setup', 'void setup()'), ('loop', 'void loop()'))}
open('ui_under_test.h', 'w').write('// GENERADO por gen_ui.py desde %s - NO editar.\n#define UI_PROD %d\n%s\n\n%s\n\n%s\n' % (path, target == 'prod', body['on_rit'], body['on_vfosel'], body['display_tick']))

fails = 0
def lint(ok, name):
    global fails
    print(('ok ' if ok else 'FAIL ') + 'L ' + name)
    fails += not ok

def strip(code):  # sin comentarios, para no dar por bueno un texto comentado
    return re.sub(r'//[^\n]*', '', code)

su, lo = strip(body['setup']), strip(body['loop'])
p_load, p_rit, p_rx = su.find('menu_load_all()'), su.find('rit = 0;'), su.find('start_rx()')
lint(p_load >= 0 and p_rit > p_load, 'setup: rit = 0 DESPUES de cargar la EEPROM (legacy 5100)')
p_vox = su.find('vox = 0;')
lint(p_vox > p_load >= 0, 'setup: vox = 0 despues de cargar (nunca TX al arrancar)')
lint(su.find('on_pwm()') > p_load, 'setup: lut PA construida con pwm_min/max cargados')
lint(0 <= p_rx < su.find('vfo_apply()', p_rx), 'setup: start_rx antes de programar el SI5351 (orden legacy)')
if target == 'prod':
    lint('cat_apply();' in lo, 'loop: cat_apply() presente (FA/MD fuera de TX)')
    lint('cat_serial_event()' in strip(src[src.index('void serialEvent()'):]), 'serialEvent -> cat_serial_event')
lint('display_tick()' in lo and '!tx' in lo[lo.find('last_display'):], 'loop: sin refresco de LCD durante TX')
lint(re.search(r'do_tune\(\)\s*{[^}]*if\(tx \|\| vox_tx\)\s*return;', strip(src), re.S) is not None,
     'do_tune: no sintoniza durante TX')
sys.exit(1 if fails else 0)
