#!/usr/bin/env python3
"""Mejoras de RX medidas con el firmware real en simavr (escenario rxt).

uso: rx_mejoras.py <usdx_avr_sim> <firmware.elf> <firmware.syms>
Arranca la radio con una EEPROM real (ajustes de menu), inyecta senal y ruido
en I/Q y analiza el audio de salida. Sale 0 si AGC Start, AGC Hang y NR hacen
lo que deben. Requiere numpy.
"""
import os, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor
import numpy as np

SIM, ELF, SYMS = sys.argv[1:4]
FS = 7812.5
EE = {'agc': 0x190, 'nr': 0x198, 'agc_start': 0x2C1, 'agc_hang': 0x2C2}  # 0x150 + 8*eslot; banco extra 0x2C0 + (eslot-32)
TMP = tempfile.mkdtemp()
EE0 = os.path.join(TMP, 'ee0.bin')

def run(seq, ee=None, save=False):
    fn = tempfile.mktemp(suffix='.f32', dir=TMP)
    a = [SIM, ELF, '--syms', SYMS, '--scenario', 'rxt', '--quiet', '--seq', ';'.join(':'.join(str(x) for x in g) for g in seq), '--aud', fn]
    a += ['--ee-save', EE0] if save else ['--ee-load', EE0]
    for k, v in (ee or {}).items(): a += ['--ee', '%d=%d' % (EE[k], v)]
    subprocess.run(a, capture_output=True, text=True)
    return np.fromfile(fn, dtype=np.float32).astype(float)

def many(jobs):
    ks = list(jobs)
    with ThreadPoolExecutor(4) as ex: return dict(zip(ks, ex.map(lambda k: run(*jobs[k]), ks)))

def rms_t(x, blk=0.05):
    n = int(blk * FS); m = len(x) // n; y = x[:m * n].reshape(m, n)
    return np.arange(m) * blk, np.sqrt(((y - y.mean(1, keepdims=True)) ** 2).mean(1))

def win(x, t0, t1):
    t, e = rms_t(x); return e[(t >= t0) & (t < t1)].mean()

db = lambda v: 20 * np.log10(max(v, 1e-6))
fails = 0
def check(name, ok):
    global fails; fails += not ok; print('   [%s] %s' % ('PASS' if ok else 'FAIL', name))

run([(6, 1000, 0, 1)], save=True)  # primer arranque: reset de fabrica -> EEPROM valida

print('== AGC Start: senal debil (3 cuentas) desde el encendido, rms de audio por segundo')
r = many({s: ([(8, 1000, 3, 0.5)], {'agc_start': s}) for s in (0, 1)})
e = {s: rms_t(r[s], 1.0)[1] for s in r}
for s in (0, 1): print('   %s: %s' % ('ON ' if s else 'OFF', ' '.join('%5.1f' % v for v in e[s])))
check('AGC Start: primer segundo >= 3x mas audio', e[1][0] >= 3 * e[0][0])
check('AGC Start: mismo regimen final (+-15 %)', abs(e[1][-1] / e[0][-1] - 1) < 0.15 or e[1][-1] > e[0][-1])

print('== AGC Hang (AGC Slow): voz 20 cuentas 0,4 s / pausa 0,5 s con ruido 2; luego senal 20 dB mas debil')
seq = [(5, 1000, 20, 2)] + [(0.4, 1000, 20, 2), (0.5, 1000, 0, 2)] * 6 + [(5, 1000, 2, 2)]
r = many({h: (seq, {'agc': 2, 'agc_hang': h}) for h in (1, 5)})
pump = {}; rec = {}
for h in (1, 5):
    g0 = np.mean([win(r[h], 5 + 0.9 * i + 0.45, 5 + 0.9 * i + 0.55) for i in range(2, 6)])
    g1 = np.mean([win(r[h], 5 + 0.9 * (i + 1) - 0.12, 5 + 0.9 * (i + 1) - 0.02) for i in range(2, 6)])
    pump[h] = db(g1 / g0); rec[h] = win(r[h], 10.4 + 1.0, 10.4 + 1.5) / win(r[h], 10.4 + 4.0, 10.4 + 4.5)
    print('   Hang %d: bombeo del ruido en la pausa %+.1f dB; senal debil a +1 s = %.0f %% del nivel final' % (h, pump[h], 100 * rec[h]))
check('AGC Hang 1 (legacy) bombea > 2,5 dB', pump[1] > 2.5)
check('AGC Hang 5 bombea < 1 dB', pump[5] < 1)
check('AGC Hang 5: senal debil recuperada en 1 s (> 70 %)', rec[5] > 0.7)

print('== NR (AGC Fast, ruido 4 cuentas): ruido solo, palabras 0,3 s (40 cuentas) y pausas 0,3 s, en dB re NR 0')
seq = [(9, 1000, 0, 4)] + [(0.3, 1000, 40, 4), (0.3, 1000, 0, 4)] * 6 + [(3, 1000, 0, 4)]
L = (0, 1, 2, 4, 6, 8)
r = many({n: (seq, {'nr': n}) for n in L})
m = {}
for n in L:
    nz = win(r[n], 6.5, 9)
    wd = np.mean([win(r[n], 9 + 0.6 * i + 0.1, 9 + 0.6 * i + 0.29) for i in range(1, 6)])
    gp = np.mean([win(r[n], 9 + 0.6 * i + 0.4, 9 + 0.6 * i + 0.59) for i in range(1, 6)])
    m[n] = (nz, wd, gp)
    print('   NR %d: ruido solo %+6.1f dB   palabras %+6.1f dB   pausas %+6.1f dB' % ((n,) + tuple(db(a / b) for a, b in zip(m[n], m[0]))))
check('NR 1 = NR 0 (paridad legacy)', all(abs(db(a / b)) < 0.1 for a, b in zip(m[1], m[0])))
for n, d in ((2, 6), (4, 12), (6, 18), (8, 24)):
    check('NR %d: palabras intactas (+-1 dB)' % n, abs(db(m[n][1] / m[0][1])) < 1)
    check('NR %d: ruido solo atenuado ~%d dB' % (n, d), db(m[n][0] / m[0][0]) < -(d - 2))
check('NR 4: pausas atenuadas > 8 dB', db(m[4][2] / m[0][2]) < -8)

print('RX_MEJORAS: %s' % ('PASS' if not fails else 'FAIL (%d)' % fails))
sys.exit(1 if fails else 0)
