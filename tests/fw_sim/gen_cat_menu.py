#!/usr/bin/env python3
# gen_cat_menu.py - extrae la tabla MENU[] REAL de usdx_plus_orange.ino a
# cat_menu.h para cat_sim (numeros de menu, tipos, rangos, eslots y callbacks
# tal cual: el CAT EX/AG/GT/... se prueba contra la tabla de produccion).
import re
src = open('../../usdx_plus_orange.ino', encoding='utf-8').read()
i = src.index('const MenuParam MENU[] PROGMEM = {')
j = src.index('};', i) + 2
k = src.index('const int8_t MENU_COUNT', j)
tab = src[i:j].replace(' PROGMEM', '')
cnt = src[k:src.index('\n', k)]
open('cat_menu.h', 'w').write('// GENERADO por gen_cat_menu.py desde usdx_plus_orange.ino - NO editar.\n%s\n%s\n' % (tab, cnt))
