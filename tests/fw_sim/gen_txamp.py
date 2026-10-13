#!/usr/bin/env python3
# gen_txamp.py [prod|lab] - extrae ssb() REAL de usdx-legazy.ino y de tx.h a
# dos TUs C (txamp_legacy.c / txamp_new.c) con tx=1 y MAX_DP definidos como
# en el firmware, exponiendo df Y amplitud. (parity_tx solo compara df, con
# tx=0 y sin MAX_DP: la envolvente nunca se comparaba con legacy.)
import re, sys
lab = len(sys.argv) > 1 and sys.argv[1] == 'lab'
def get_ssb(path):
    src = re.sub(r'/\*.*?\*/', '', open(path, encoding='utf-8', errors='replace').read(), flags=re.S).split('\n')
    for idx, ln in enumerate(src):
        if 'ssb(int16_t in)' in ln and 'inline' in ln:
            depth, out = ln.count('{') - ln.count('}'), [ln]
            for j in range(idx + 1, len(src)):
                pre = src[j].split('//')[0]
                depth += pre.count('{') - pre.count('}')
                out.append(src[j])
                if depth == 0:
                    return '\n'.join(out)
    sys.exit('ssb() no encontrada en ' + path)

PRE = r'''// GENERADO por gen_txamp.py - NO editar.
#include <stdint.h>
#include <stdlib.h>
#define _F_SAMP_TX 4800
#define _UA 600
#define USB 1
#define LSB 0
#define MORE_MIC_GAIN 1
#define CARRIER_COMPLETELY_OFF_ON_LOW 1
#define COMP_TH 128
#define MAX_DP ((filt == 0) ? _UA : (filt == 3) ? _UA / 4 : _UA / 2)
static volatile uint8_t mode = USB, filt = 0, drive = 4, tx = 1, vox_thresh = 4, amp = 0, vox = 0, quad = 0;
static volatile uint8_t dig_mode = 0, comp_enable = 0, tx_lowcut = 0;
static int16_t comp_env = 0, lc_lp = 0;
static uint8_t lut[256];
#define abs(x) ((x) < 0 ? -(x) : (x))
#define magn(i, q) (abs(i) > abs(q) ? abs(i) + abs(q) / 4 : abs(q) + abs(i) / 4)
#define _vox(a) ((void)0)
static inline int16_t arctan3(int16_t q, int16_t i) {
#define _atan2(z) (_UA / 8 + _UA / 22 - _UA / 22 * z) * z
  int16_t r;
  if(abs(q) > abs(i)) r = _UA / 4 - _atan2(abs(i) / abs(q));
  else r = (i == 0) ? 0 : _atan2(abs(q) / abs(i));
  r = (i < 0) ? _UA / 2 - r : r;
  return (q < 0) ? -r : r;
}
#undef _atan2
'''
POST = '''
int16_t %(p)s_df;
uint8_t %(p)s_amp;
void %(p)s_ssb(int16_t in) {
  static uint8_t init;
  if(!init) { for(int i = 0; i < 256; i++) lut[i] = (uint8_t)i; init = 1; } // LUT lineal: amp = envolvente
  %(p)s_df = ssb(in);
  %(p)s_amp = amp;
}
void %(p)s_cfg(uint8_t m, uint8_t f, uint8_t d, uint8_t dig) { mode = m; filt = f; drive = d; dig_mode = dig; }
'''
for name, path in (('legacy', '../../usdx-legazy/usdx-legazy.ino'), ('new', '../../usdx-lab/tx.h' if lab else '../../tx.h')):
    body = get_ssb(path).replace('inline int16_t ssb(', 'static int16_t ssb(')
    open('txamp_%s.c' % name, 'w').write(PRE + body + POST % {'p': name})
divs = [l for l in open('../../usdx-lab/tx.h' if lab else '../../tx.h').read().split('\n') if l.startswith('#define TX_DIV')]
open('txamp_div.h', 'w').write('// GENERADO por gen_txamp.py: macros de division de tx.h\n' + '\n'.join(divs) + '\n')
print('txamp_legacy.c / txamp_new.c generados (ssb con tx=1, df + amplitud)')
