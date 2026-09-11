#!/usr/bin/env python3
"""gen_ab.py — Fase 0: genera ab_tx.c desde tx.h con envolvente capturable.

Igual extracción que gen_parity_tx.py pero con tx=1, LUT lineal y captura
de (df, amp) por muestra para reconstruir la SSB polar en el harness A/B.
"""
import re

BASE = '/home/jgalera/develop/arduino/usdx/usdx_plus_orange'

with open(f'{BASE}/usdx_plus_orange_v2/tx.h') as f:
    src = f.read()


def strip_c(s):
    return re.sub(r'/\*.*?\*/', '', s, flags=re.S)


def get_ssb(src):
    clean = strip_c(src).split('\n')
    for idx, ln in enumerate(clean):
        if 'ssb(int16_t in)' in ln and 'inline' in ln:
            depth = ln.count('{') - ln.count('}')
            out = [ln]
            for j in range(idx + 1, len(clean)):
                pre = clean[j].split('//')[0]
                depth += pre.count('{') - pre.count('}')
                out.append(clean[j])
                if depth == 0:
                    return '\n'.join(out)
    return None


ssb = get_ssb(src)
assert ssb, "ssb() no encontrada en tx.h"

PREAMBLE = """#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define F_SAMP_TX 4800
#define _F_SAMP_TX 4800
#define _UA 600
#define USB 1
#define LSB 0
#define CW_MODE 2
#define MORE_MIC_GAIN 1
#define MAX_DP ((filt == 0) ? _UA : (filt == 3) ? _UA / 4 : _UA / 2)
volatile uint8_t mode = USB, filt = 0, drive = 2, tx = 1, vox_thresh = 4, amp = 0;
volatile uint8_t vox = 0, quad = 0;
volatile uint8_t dig_mode = 0; // F3.7: expuesto para A/B (bool en fw, truthiness igual)
static int16_t OCR1BL, OCR1AL;
static uint8_t lut[256];
#define abs(x) ((x) < 0 ? -(x) : (x))
#define magn(i,q) (abs(i) > abs(q) ? abs(i) + (abs(q) >> 2) : abs(q) + (abs(i) >> 2))
#define _vox(a) ((void)0)
static inline int16_t arctan3(int16_t q, int16_t i) {
#define _atan2(z) (_UA / 8 + _UA / 22 - _UA / 22 * z) * z
  int16_t r;
  if (abs(q) > abs(i)) r = _UA / 4 - _atan2(abs(i) / abs(q));
  else r = (i == 0) ? 0 : _atan2(abs(q) / abs(i));
  r = (i < 0) ? _UA / 2 - r : r;
  return (q < 0) ? -r : r;
}
#undef _atan2
void ab_tx_init(void) {
  for (int i = 0; i < 256; i++) lut[i] = (uint8_t)i;  // LUT lineal
  tx = 1;
}
"""

open('ab_tx.c', 'w').write(
    PREAMBLE + ssb.replace('inline int16_t ssb(', 'static inline int16_t ssb(') + """
int16_t ab_df_out;
uint8_t ab_amp_out;
void ab_ssb(int16_t in){ ab_df_out = ssb(in); ab_amp_out = amp; }
void ab_dig_mode(uint8_t v){ dig_mode = v; }
""")
print("ab_tx.c generado (tx=1, LUT lineal)")
