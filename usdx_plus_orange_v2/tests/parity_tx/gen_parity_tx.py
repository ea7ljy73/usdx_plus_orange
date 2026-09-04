#!/usr/bin/env python3
import re, os
base='/home/jgalera/develop/arduino/usdx/usdx_plus_orange'
def lines(p): return open(p).read().split('\n')
def strip_c(s): return re.sub(r'/\*.*?\*/','',s,flags=re.S)
def get_ssb(src):
    clean=strip_c(src)
    lines_c=clean.split('\n')
    for idx,ln in enumerate(lines_c):
        if 'ssb(int16_t in)' in ln and 'inline' in ln:
            depth=ln.count('{')-ln.count('}')
            out=[ln]
            for j in range(idx+1,len(lines_c)):
                pre=lines_c[j].split('//')[0]
                depth+=pre.count('{')-pre.count('}')
                out.append(lines_c[j])
                if depth==0: return '\n'.join(out)
    return None
leg=lines(f'{base}/usdx-legazy/usdx-legazy.ino')
v2=lines(f'{base}/usdx_plus_orange_v2/tx.h')
ssb_leg=get_ssb('\n'.join(leg))
ssb_v2=get_ssb('\n'.join(v2))

PREAMBLE="""#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define F_SAMP_TX 4800
#define _F_SAMP_TX 4800
#define _UA 600
#define USB 1
#define LSB 0
#define CW_MODE 2
#define MORE_MIC_GAIN 1
static volatile uint8_t mode=USB,filt=0,drive=2,tx=0,vox_thresh=4,amp=0,dig_mode=0;
static volatile uint8_t vox=0,keyerControl=0,keyerState=0;
static int16_t OCR1BL,OCR1AL;
static uint8_t lut[256];
#define abs(x) ((x)<0?-(x):(x))
#define magn(i,q) (abs(i)>abs(q)?abs(i)+(abs(q)>>2):abs(q)+(abs(i)>>2))
#define _vox(a) ((void)0)
static inline int16_t arctan3(int16_t q,int16_t i){
#define _atan2(z) (_UA/8 + _UA/22 - _UA/22 * z) * z
  int16_t r;
  if(abs(q)>abs(i)) r=_UA/4-_atan2(abs(i)/abs(q));
  else r=(i==0)?0:_atan2(abs(q)/abs(i));
  r=(i<0)?_UA/2-r:r;
  return (q<0)?-r:r;
}
#undef _atan2
static void dummy(void){}
"""

open('tx_legacy.c','w').write(PREAMBLE + ssb_leg.replace('inline int16_t ssb(', 'static inline int16_t ssb(') + """
volatile int16_t legacy_df_out;
void legacy_ssb(int16_t in){ legacy_df_out = ssb(in); }
""")
open('tx_v2.c','w').write(PREAMBLE + ssb_v2.replace('inline int16_t ssb(', 'static inline int16_t ssb(') + """
volatile int16_t v2_df_out;
void v2_ssb(int16_t in){ v2_df_out = ssb(in); }
""")
print("ssb legacy OK" if ssb_leg else "ssb legacy MISSING")
print("ssb v2     OK" if ssb_v2 else "ssb v2 MISSING")