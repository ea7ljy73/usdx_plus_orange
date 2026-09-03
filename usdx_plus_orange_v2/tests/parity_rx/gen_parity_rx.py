#!/usr/bin/env python3
import re, os
base='/home/jgalera/develop/arduino/usdx/usdx_plus_orange'

def lines(p): return open(p).read().split('\n')

def strip_c(s):
    # comment/string-aware stripper: preserves strings and char literals
    out = []
    i = 0
    n = len(s)
    while i < n:
        c = s[i]
        nxt = s[i+1] if i + 1 < n else ''
        if c == '/' and nxt == '/':
            while i < n and s[i] != '\n':
                i += 1
            continue
        if c == '/' and nxt == '*':
            i += 2
            while i + 1 < n and not (s[i] == '*' and s[i+1] == '/'):
                if s[i] == '\n':
                    out.append('\n')
                i += 1
            i += 2
            continue
        if c == '"':
            out.append(c)
            i += 1
            while i < n:
                if s[i] == '\\':
                    out.append(s[i])
                    i += 1
                    if i < n:
                        out.append(s[i])
                        i += 1
                    continue
                if s[i] == '"':
                    out.append(c)
                    i += 1
                    break
                out.append(s[i])
                i += 1
            continue
        if c == "'":
            out.append(c)
            i += 1
            while i < n:
                if s[i] == '\\':
                    out.append(s[i])
                    i += 1
                    if i < n:
                        out.append(s[i])
                        i += 1
                    continue
                if s[i] == "'":
                    out.append(c)
                    i += 1
                    break
                out.append(s[i])
                i += 1
            continue
        out.append(c)
        i += 1
    return ''.join(out)

DEFINED = set(['NEW_RX', 'AF_OUT', 'OUTLET', 'CW_DECODER'])

def preprocess(src, defined):
    out, stack, active = [], [], True
    for ln in src.split('\n'):
        m = re.match(r'\s*#\s*ifdef\s+(\w+)', ln)
        if m:
            stack.append(active); active = active and (m.group(1) in defined); continue
        m = re.match(r'\s*#\s*ifndef\s+(\w+)', ln)
        if m:
            stack.append(active); active = active and (m.group(1) not in defined); continue
        m = re.match(r'\s*#\s*if\b', ln)
        if m:
            stack.append(active); active = active and ('1' in ln or 'cw_tone' not in ln); continue
        m = re.match(r'\s*#\s*else\b', ln)
        if m:
            active = stack[-1] and (not active); continue
        m = re.match(r'\s*#\s*endif\b', ln)
        if m:
            active = stack.pop(); continue
        if active:
            out.append(ln)
    return '\n'.join(out)

def extract_fn(src, name):
    clean = strip_c(preprocess(src, DEFINED))
    lc = clean.split('\n')
    rx = re.compile(r'^\s*(?:inline\s+)?(?:int16_t|void|int|uint8_t|bool)\s+%s\s*\(' % re.escape(name))
    for idx, ln in enumerate(lc):
        if rx.match(ln) and not ln.rstrip().endswith(';'):
            depth = ln.count('{') - ln.count('}')
            if depth == 0 and '{' in ln:
                return ln
            out = [ln]
            for j in range(idx + 1, len(lc)):
                pre = lc[j].split('//')[0]
                depth += pre.count('{') - pre.count('}')
                out.append(lc[j])
                if depth == 0:
                    return '\n'.join(out)
    return None

LEG = lines(f'{base}/usdx-legazy/usdx-legazy.ino')
V2  = lines(f'{base}/usdx_plus_orange_v2/rx.h')
LEGF = open(f'{base}/usdx-legazy/usdx_filter.h').read()
V2F  = open(f'{base}/usdx_plus_orange_v2/usdx_filter.h').read()

def get_filter(src):
    return extract_fn(src, 'filt_var')

rx_fns = ['process_agc_fast', 'process_agc', 'process_nr', 'slow_dsp',
          'process', 'sdr_rx_common_q', 'sdr_rx_common_i']
cic_fns = ['sdr_rx_00', 'sdr_rx_01', 'sdr_rx_02', 'sdr_rx_03', 'sdr_rx_04',
           'sdr_rx_05', 'sdr_rx_06', 'sdr_rx_07']

def collect(src):
    body = {}
    for f in rx_fns:
        body[f] = extract_fn(src, f)
    for f in cic_fns:
        body[f] = extract_fn(src, f)
    return body

L = collect('\n'.join(LEG))
V = collect('\n'.join(V2))

HDR = r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#ifndef INT16_MAX
#define INT16_MAX 32767
#endif
#define abs(x) ((x)<0?-(x):(x))
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define magn(i,q) (abs(i)>abs(q)?abs(i)+(abs(q)>>2):abs(q)+(abs(i)>>2))
#define F_SAMP_PWM (78125 / 1)
#define F_SAMP_RX 62500
#define F_ADC_CONV (192307 / 2)
#define R 4
#define HI(x) ((x) >> 8)
#define LO(x) ((x) & 0xFF)
#define EA(y, x, one_over_alpha) (y) = (y) + ((x) - (y)) / (one_over_alpha);
#define USB 1
#define LSB 0
#define AM 3
#define FM 4
#define CW_MODE 2

/* ---- AVR stubs ---- */
static volatile uint8_t ADMUX_s, ADCSRA_s;
static volatile int16_t ADC_s, OCR1AL_s;
#define ADMUX ADMUX_s
#define ADCSRA ADCSRA_s
#define ADSC 6
#define OCR1AL OCR1AL_s

/* shared IQ test signal (single definition in driver) */
extern int16_t iq_i[];
extern int16_t iq_q[];
extern int iq_i_idx;
extern int iq_q_idx;

static void interrupts(void){}

/* globals owned by the TU (static declarations; defined at the tail) */
static volatile uint8_t mode;
static volatile uint8_t agc;
static volatile uint8_t volume;
static volatile uint8_t nr;
static volatile uint8_t filt;
static volatile uint8_t att2;
static volatile uint8_t cw_tone;
'''

SUF = {'L': '_legacy', 'V': '_v2'}

def make_tu(tag, body, filter_src):
    s = SUF[tag]
    txt = HDR
    txt += f'''
typedef void (*func_t)(void);
static volatile func_t func_ptr;
static volatile uint8_t admux[3]={{0,1,2}};
static volatile uint8_t _init = 1;
static volatile int16_t i, q, ocomb, qh;
static uint32_t absavg256 = 0;
static volatile uint32_t _absavg256 = 0;
static int16_t ozi1, ozi2;
static uint32_t amp32 = 0;
static volatile uint32_t _amp32 = 0;
static int16_t gain = 1024;
#define DECAY_FACTOR 400
static int16_t centiGain = 128;
static uint16_t decayCount = DECAY_FACTOR;
static uint8_t tc = 0;
static int16_t stub_adc(void) {{
  if((ADMUX_s & 0x0f) == 0) return iq_q[iq_q_idx++];
  else                      return iq_i[iq_i_idx++];
}}
#define ADC stub_adc()
static int16_t i_s0za1, i_s0zb0, i_s0zb1, i_s1za1, i_s1zb0, i_s1zb1;
static int16_t q_s0za1, q_s0zb0, q_s0zb1, q_s1za1, q_s1zb0, q_s1zb1, q_ac2;
#define M_SR 1
'''
    def add(b):
        nonlocal txt
        b = b.strip()
        if not b.startswith('static'):
            b = 'static ' + b
        txt += '\n' + b + '\n'
    add(body['process_agc_fast'])
    add(body['process_agc'])
    add(body['process_nr'])
    add(filter_src)
    add(body['slow_dsp'])
    add(body['process'])
    txt += '''
static void sdr_rx_00();
static void sdr_rx_01();
static void sdr_rx_02();
static void sdr_rx_03();
static void sdr_rx_04();
static void sdr_rx_05();
static void sdr_rx_06();
static void sdr_rx_07();
static inline int16_t sdr_rx_common_q();
static inline int16_t sdr_rx_common_i();
'''
    for f in cic_fns:
        add(body[f])
    add(body['sdr_rx_common_q'])
    add(body['sdr_rx_common_i'])
    txt += f'''
static volatile uint8_t mode=USB,agc=0,volume=12,nr=0,filt=0,att2=2,cw_tone=1;
static void rx_cfg(uint8_t _mode, uint8_t _agc, uint8_t _volume, uint8_t _nr,
            uint8_t _filt, uint8_t _att2, uint8_t _cw_tone){{
  mode=_mode; agc=_agc; volume=_volume; nr=_nr; filt=_filt; att2=_att2; cw_tone=_cw_tone;
}}
static int16_t rx_last_audio(void){{ return OCR1AL_s; }}
static void rx_run(int steps){{ while(steps--) func_ptr(); }}
static void rx_init(void){{
  memset((void*)&i, 0, sizeof(i));
  memset((void*)&q, 0, sizeof(q));
  ocomb=0; qh=0; absavg256=0; _absavg256=0; ozi1=0; ozi2=0;
  iq_i_idx=0; iq_q_idx=0;
  func_ptr = sdr_rx_00;
}}
/* unique exported names */
void rx_cfg{s}(uint8_t _m,uint8_t _a,uint8_t _v,uint8_t _n,uint8_t _f,uint8_t _a2,uint8_t _c){{ rx_cfg(_m,_a,_v,_n,_f,_a2,_c); }}
void rx_init{s}(void){{ rx_init(); }}
void rx_run{s}(int st){{ rx_run(st); }}
int16_t rx_last_audio{s}(void){{ return rx_last_audio(); }}
'''
    return txt

open('rx_legacy.c', 'w').write(make_tu('L', L, get_filter(LEGF)))
open('rx_v2.c', 'w').write(make_tu('V', V, get_filter(V2F)))
print("generated")
for k in rx_fns + cic_fns:
    if not L.get(k): print("legacy MISSING:", k)
    if not V.get(k): print("v2 MISSING:", k)