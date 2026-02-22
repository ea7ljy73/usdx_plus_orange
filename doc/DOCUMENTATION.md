# uSDX Plus Orange - Technical Documentation

**Version:** 5.14  
**Author:** EA7LJY - Julian  
**Date:** February 2026

---

## Table of Contents

1. [Overview](#1-overview)
2. [TX Signal Chain](#2-tx-signal-chain)
3. [RX Signal Chain](#3-rx-signal-chain)
4. [DSP Functions](#4-dsp-functions)
5. [Mode-Specific Processing](#5-mode-specific-processing)
6. [SI5351 Frequency Synthesis](#6-si5351-frequency-synthesis)
7. [Signal Flow Summary](#7-signal-flow-summary)
8. [File Structure](#8-file-structure)

---

## 1. Overview

uSDX Plus Orange is a software-defined radio (SDR) transceiver for ATMEGA328P-based hardware. It implements a complete SDR signal chain using:

- **TX:** Direct Digital Synthesis (DDS) with SI5351 for frequency synthesis
- **RX:** Quadrature Sampling Detector (QSD) with ADC sampling
- **DSP:** Optimized integer arithmetic for AVR microcontroller

### Key Specifications

| Parameter | Value |
|----------|-------|
| Platform | ATMEGA328P @ 20MHz |
| Flash | 32KB |
| RAM | 2KB |
| ADC | 10-bit @ 62.5kSPS (RX) |
| TX Modes | SSB (USB/LSB), CW, AM, FM |
| RX Modes | SSB (USB/LSB), CW, AM, FM |

### Supported HF Bands

| Band | Frequency Range | Center Freq | Typical Mode |
|------|-----------------|-------------|--------------|
| 80m | 3.5 - 3.9 MHz | 3.700 MHz | LSB |
| 60m | 5.3515 - 5.3665 MHz | 5.357 MHz | USB |
| 40m | 7.0 - 7.3 MHz | 7.150 MHz | LSB |
| 30m | 10.1 - 10.150 MHz | 10.125 MHz | USB |
| 20m | 14.0 - 14.350 MHz | 14.200 MHz | USB |
| 17m | 18.068 - 18.168 MHz | 18.118 MHz | USB |
| 15m | 21.0 - 21.450 MHz | 21.300 MHz | USB |
| 10m | 28.0 - 29.700 MHz | 28.500 MHz | USB |

---

## 2. TX Signal Chain

The TX chain converts microphone audio into RF signals.

### 2.1 TX Block Diagram

```
MICROPHONE INPUT
       |
       v
   +---------+     +-------------+     +--------------+     +--------------+
   |   ADC   | --> |   Voice     | --> |   SSB        | --> |  Phase to    |
   |  (ADC)  |     |  Processing |     |  Generation  |     |  Frequency   |
   +---------+     +-------------+     +--------------+     +   Converter   |
       |                   |                     |                    |
       v                   v                     v                    v
   10-bit ADC          Compressor           Hilbert            Phase difference
   @ F_SAMP_TX         EQ, Pre-emph       Transform          to frequency
                                                                   |
                                                                   v
                                                       +-----------------------+
                                                       |   SI5351 Frequency    |
                                                       |   Synthesis           |
                                                       +-----------------------+
                                                               |
                                                               v
                                                       +-----------------------+
                                                       |   I2C to SI5351       |
                                                       +-----------------------+
                                                               |
                                                               v
                                                       +-----------------------+
                                                       |   PWM Output          |
                                                       |   (OCR1BL)           |
                                                       +-----------------------+
```

### 2.2 Microphone Input and ADC

**Location:** Line 2211-2213

```cpp
int16_t adc = ADC - 512;  // Center ADC range
int16_t df = ssb(adc >> MIC_ATTEN);  // Process with TX chain
```

**Parameters:**
- ADC Resolution: 10 bits
- DC Offset: 512 (centers the ADC range)
- MIC_ATTEN: Attenuation factor (default 0)

### 2.3 Voice Processing

Three processing stages prepare the audio for SSB modulation:

#### 2.3.1 Voice Compressor

**Location:** Lines 2005-2021

```cpp
inline int16_t voice_compressor(int16_t in) {
  int16_t abs_in = in < 0 ? -in : in;
  
  // Attack: ~1.5ms (faster response after v5.14)
  // Release: ~27ms
  if(abs_in > comp_envelope)
    comp_envelope = comp_envelope + ((abs_in - comp_envelope) >> 1);
  else
    comp_envelope = comp_envelope - ((comp_envelope - abs_in) >> 7);

  // Ratio: 2:1 (softer after v5.14)
  if(comp_envelope > comp_threshold) {
    int16_t gain = (comp_envelope - comp_threshold) / comp_ratio + comp_threshold;
    return (int16_t)((int32_t)in * gain / comp_envelope);
  }
  return in;
}
```

**Parameters (v5.14):**
| Parameter | Value | Purpose |
|-----------|-------|---------|
| `comp_enable` | 1 | Compressor enabled |
| `comp_ratio` | 2 | 2:1 ratio (softer) |
| `comp_threshold` | 128 | Activation threshold |
| `comp_envelope` | 0 | Tracking envelope |

#### 2.3.2 Microphone EQ

**Location:** Lines 2023-2034

Two-band parametric EQ for voice shaping.

```cpp
inline int16_t mic_eq(int16_t in) {
  if(eq_low == 0 && eq_high == 0)
    return in;
  // Low shelf + High shelf processing
  // ...
}
```

#### 2.3.3 Pre-emphasis

**Location:** Lines 2040-2044

```cpp
if(pre_emph > 0) {
  int16_t pre_in = in;
  in = in + ((pre_in - pre_z1) * pre_emph);
  pre_z1 = pre_in;
}
```

| Value | Effect |
|-------|--------|
| 0 | Off |
| 1 | 6dB/oct (default) |
| 2 | 12dB/oct |
| 3 | 18dB/oct |

### 2.4 Hilbert Transform for SSB

**Location:** Lines 2069-2087

Creates 90° phase-shifted signal for SSB generation.

```cpp
// I (in-phase) component
i = v[7] * 2;

// Q (quadrature) component with Hilbert transform
q = ((v[0] - v[14]) * 2 + 
     (v[2] - v[12]) * 8 + 
     (v[4] - v[10]) * 21 + 
     (v[6] - v[8]) * 16) / 64 + (v[6] - v[8]);
```

**Performance:** 40dB side-band rejection in 400-1900Hz range.

### 2.5 Phase to Frequency Conversion

**Location:** Lines 2105-2129

```cpp
int16_t phase = arctan3(q, i);      // Calculate phase
int16_t dp = phase - prev_phase;    // Phase difference = frequency
prev_phase = phase;

if(dp < 0)
  dp = dp + _UA;  // Make all phase differences positive
```

---

## 3. RX Signal Chain

RX uses QSD (Quadrature Sampling Detector) for direct RF sampling.

### 3.1 RX Block Diagram

```
RF INPUT (from QSD Detector)
       |
       v
   +---------+     +-------------+     +-------------+     +-------------+
   |   ADC   | --> |   CIC        | --> |   Decimation | --> |   Hilbert    |
   |  (ADC)  |     |  Decimation |     |   (8x)       |     |   Transform  |
   +---------+     +-------------+     +-------------+     +-------------+
   Line 3433          Line 3358-3418       Line 3228-3347      Line 2983-3009
       |                   |                    |                   |
       v                   v                    v                   v
   10-bit ADC         3rd-order CIC       Downsample          90-deg phase
   @ 62.5kSPS         R=4, M=2            62.5k -> 7.8kSPS    shift for SSB
                                                                   |
                                                                   v
                                                       +---------------------+
                                                       |   Demodulation      |
                                                       |   (Mode-specific)   |
                                                       +---------------------+
                                                               Line 3030-3100
                                                                   |
                                                   +-------+-------+-------+
                                                   |       |       |       |
                                                   v       v       v       v
                                              +-------+ +-------+ +-------+
                                              |  SSB  | |  AM   | |  FM   |
                                              |  demod| | demod | | demod |
                                              +-------+ +-------+ +-------+
                                              Line3096 Line3030 Line3074
                                                   |       |       |
                                                   v       v       v
                                              +-------+ +-------+ +-------+
                                              |  AGC  | |  AGC  | | Deemph|
                                              +-------+ +-------+ +-------+
                                              Line2712 Line2712 Line2141
                                                   |       |       |
                                                   v       v       v
                                              +-------+ +-------+ +-------+
                                              |  NR   | |  NR   | |  NR   |
                                              +-------+ +-------+ +-------+
                                              Line2881 Line2881 Line2881
                                                   |       |       |
                                                   v       v       v
                                              +-------+ +-------+ +-------+
                                              |  BPF  | |  BPF  | |  BPF  |
                                              +-------+ +-------+ +-------+
                                              Line2945 Line2945 Line2945
                                                   |
                                                   v
                                              +---------------------+
                                              |   PWM Audio Output  |
                                              |   (OCR1AL)          |
                                              +---------------------+
```

### 3.2 I/Q Sampling

**Location:** Lines 3423-3456

The QSD produces alternating I and Q samples:

```cpp
// I channel (even samples)
inline int16_t sdr_rx_common_i() {
  int16_t adc = ADC - 511;  // DC removal
  int16_t ac = (prev_adc + adc) >> 1;  // Average 2 samples
  prev_adc = adc;
  return ac;
}

// Q channel (odd samples)
inline int16_t sdr_rx_common_q() {
  return ADC - 511;  // DC removal
}
```

**Parameters:**
- Sample Rate: 62,500 SPS
- DC Offset: 511
- Sample Averaging: 2 samples for noise reduction

### 3.3 CIC Decimation

**Location:** Lines 3228-3418

3rd-order Cascaded Integrator-Comb filter:

```cpp
#define R 4  // Rate change factor
#define M_SR 1  // Decimation shift

void sdr_rx_00() {
  int16_t ac = sdr_rx_common_i();
  int16_t i_s1za0 = (ac + (i_s0za1 + i_s0zb0) * 3 + i_s0zb1) >> M_SR;
  int16_t ac2 = (i_s1za0 + (i_s1za1 + i_s1zb0) * 3 + i_s1zb1);
  process(ac2, q_ac2);
}
```

**Decimation Chain:**
| Stage | Input Rate | Output Rate | Factor |
|-------|------------|-------------|--------|
| CIC | 62,500 SPS | 7,812.5 SPS | 8x |

### 3.4 Hilbert Transform for Demodulation

**Location:** Lines 2983-3009

```cpp
// Hilbert transform for SSB/CW
qh = ((v[0] - q_ac2) + (v[2] - v[12]) * 4) / 64 + 
     ((v[4] - v[10]) + (v[6] - v[8])) / 8 +
     ((v[4] - v[10]) * 5 - (v[6] - v[8])) / 128 + 
     (v[6] - v[8]) / 2;
```

**Performance:** 43dB side-band rejection in 650-3400Hz range.

### 3.5 AGC Implementation

**Location:** Lines 2712-2740

```cpp
inline int16_t process_agc(int16_t in) {
  // Gain calculation
  if(centiGain >= 128)
    out = (centiGain >> 5) * in;
  else
    out = (centiGain >> 2) * (in >> 3);
  out >>= 2;
  
  // Fast attack, slow decay
  if(HI(abs(out)) > HI(1536))
    centiGain -= (centiGain >> 4);  // Fast attack
  else {
    if(--decayCount == 0) {
      if(small)
        centiGain += (centiGain >> 4);
      decayCount = (uint16_t)agc_decay * 100;
    }
  }
  return out;
}
```

**Parameters:**
| Parameter | Default | Purpose |
|-----------|---------|---------|
| `agc` | 1 | AGC mode (1=normal, 2=fast) |
| `agc_decay` | 8 | Decay time (~800ms) |

### 3.6 Audio Filtering

**Location:** `usdx_filter.h` Lines 46-222

```cpp
inline int16_t filt_var(int16_t za0) {
  // 300Hz high-pass
  za0 = ((30 * (za0 - zz2) + 25 * zz1) >> 5);
  
  // 4th order IIR bandpass (SSB)
  zb0 = ((za0 + 2*za1 + za2) >> 1) - ((13*zb1 + 11*zb2) >> 4);
  zc0 = ((zb0 + 2*zb1 + zb2) >> 1) - ((18*zc1 + 11*zc2) >> 4);
  
  return zc0;
}
```

**Filter Options:**
| Filter | Bandwidth | Use Case |
|--------|-----------|----------|
| 1 | 0-2900Hz | SSB Wide |
| 2 | 0-2400Hz | SSB Narrow |
| 3 | 0-1800Hz | SSB Very Narrow |
| 4 | 500-1000Hz | CW |
| 5 | 650-840Hz | CW Narrow |
| 6 | 650-750Hz | CW Very Narrow |
| 7 | 630-680Hz | CW Extreme |

---

## 4. DSP Functions

### 4.1 arctan3() - Phase Calculation

**Location:** Lines 1951-1967

```cpp
inline int16_t arctan3(int16_t y, int16_t x) {
  if (x == 0 && y == 0) return 0;
  
  int8_t c = 0;
  int16_t rx = x, ry = y;
  
  if (ry < 0) {
    if (rx < 0) { ry = -ry; rx = -rx; c = 2; }
    else { rx = -rx; ry = -ry; c = 3; }
  } else {
    if (rx < 0) { ry = -ry; rx = -rx; c = 1; }
  }
  
  int16_t r = (ry << 8) / (rx + (rx >> 4));
  int16_t ang = (r * 128) >> 8;
  if (ang > 90) ang = 90;
  
  static const int8_t atan_table[] = { /* 0-90 degrees */ };
  int16_t result = atan_table[ang];
  
  switch (c) {
    case 1: result = -result; break;
    case 2: result = -180 + result; break;
    case 3: result = 180 - result; break;
  }
  return result;
}
```

**Error:** ~0.8 degrees

### 4.2 magn() - Magnitude Calculation

**Location:** Lines 1969-1971

```cpp
#define magn(i, q) \
  (abs(i) > abs(q) ? abs(i) + (abs(q)/4) : abs(q) + (abs(i)/4))
```

**Error:** ~0.95dB vs true magnitude

### 4.3 slow_dsp() - Post-processing

**Location:** Lines 2970-3174

Main DSP processing function for RX:

```cpp
inline int16_t slow_dsp(int16_t i_ac2, int16_t q_ac2) {
  // Digital gain control
  q_ac2 >>= att2;
  
  // Hilbert transform (SSB/CW)
  if(mode != AM && mode != FM) {
    qh = /* Hilbert calculation */;
  }
  
  // Mode-specific demodulation
  if(mode == AM) {
    ac = magn(i, q);
  } else if(mode == FM) {
    int16_t z0 = _arctan3(q, i);
    ac = z0 - z1;
  } else {  // SSB, CW
    acm = -id - qh;
    ac = acm;
  }
  
  // AGC
  if(agc == 1)
    ac = process_agc(ac);
  
  // Noise reduction
  if(nr)
    ac = process_nr(ac);
  
  // Bandpass filter
  if(filt)
    ac = filt_var(ac);
  
  return ac;
}
```

### 4.4 process_nr() - Noise Reduction

**Location:** Lines 2881-2941

Two methods available:

**Method 1: Exponential Averaging (nr < 3)**
```cpp
inline int16_t process_nr(int16_t in) {
  static int16_t ea1;
  ea1 = EA(ea1, in, 1 << (nr - 1));
  return ea1;
}
```

**Method 2: FIR Low-pass (nr >= 3)**
```cpp
// FIR filter with window sizes 7-21 taps
// Cutoff frequencies: 2700Hz down to 500Hz
```

---

## 5. Mode-Specific Processing

### 5.1 SSB (USB/LSB)

**TX:** Lines 2036-2129
```
Audio --> Compressor --> EQ --> Pre-emphasis --> Hilbert --> I/Q --> arctan3 --> df --> SI5351
```

**RX Demodulation:** Lines 3096-3100
```cpp
else {  // USB, LSB, CW
  acm = -id - qh;  // SSB demodulation
  ac = acm;
}
```

### 5.2 CW

**TX Encoding:** Lines 2266-2284

Minsky circle algorithm for tone generation:

```cpp
inline void process_minsky() {
  int8_t alpha127 = tones[cw_tone] * 798 / _F_SAMP_TX;
  p_sin += alpha127 * n_cos / 127;
  n_cos -= alpha127 * p_sin / 127;
}
```

**RX Decoder:** Lines 2460-2530+

Threshold detection and Morse code decoding.

### 5.3 AM

**TX:** Lines 2286-2302

```cpp
void dsp_tx_am() {
  int16_t in = (ADC - 512) >> MIC_ATTEN;
  in = in << (drive - 4);
  in = max(0, min(255, (in + 32)));  // Base offset
  amp = in;
}
```

**RX Demodulation:** Lines 3030-3045
```cpp
acm = -i - q;  // S-Meter
ac = magn(i, q);  // Magnitude detection
```

### 5.4 FM

**TX:** Lines 2304-2321

```cpp
void dsp_tx_fm() {
  int16_t in = (ADC - 512) >> MIC_ATTEN;
  in = in << drive;
  int16_t df = in;
  si5351.freq_calc_fast(df);  // Direct frequency deviation
}
```

**RX Demodulation:** Lines 3074-3094
```cpp
else if(mode == FM) {
  int16_t z0 = _arctan3(q, i);
  ac = z0 - z1;  // Differentiator
  z1 = z0;
}
```

---

## 6. SI5351 Frequency Synthesis

### 6.1 Overview

The SI5351 generates the RF carrier and sampling clock.

**Key Functions:**
| Function | Lines | Purpose |
|----------|-------|---------|
| `freq_calc_fast()` | 1412, 2193 | Calculate frequency registers |
| `SendPLLRegisterBulk()` | 1435, 2168 | Send registers via I2C |
| `freq()` | 1559 | Set CLK0/CLK1/CLK2 |

### 6.2 TX Frequency Synthesis

**SSB/CW:**
```cpp
int16_t df = ssb(adc >> MIC_ATTEN);
si5351.freq_calc_fast(df);
```

**AM:**
```cpp
amp = in;  // Direct amplitude control
si5351.freq_calc_fast(0);
```

**FM:**
```cpp
int16_t df = in;
si5351.freq_calc_fast(df);
```

---

## 7. Signal Flow Summary

### 7.1 Complete TX Flow

```
MIC --> ADC --> ssb() --> Phase Calc --> freq_calc_fast() --> SI5351 --> PA --> ANT
              |           |
              |           v
              |        VOX (amplitude threshold)
              |
              v
        Compressor/EQ/Pre-emph
```

### 7.2 Complete RX Flow

```
ANT --> QSD --> ADC --> CIC Decimate --> Hilbert --> Demod --> AGC/NR/Filter --> PWM
                      |                  |
                      v                  v
                 I/Q Sample           Slow DSP (mode-specific)
```

---

## 8. File Structure

| File | Lines | Purpose |
|------|-------|---------|
| `usdx_plus_orange.ino` | 7250 | Main firmware |
| `usdx_settings.h` | 143 | Configuration |
| `usdx_filter.h` | 221 | DSP filters |

### Key Line References

| Component | Lines |
|-----------|-------|
| Voice Compressor | 2005-2021 |
| Mic EQ | 2023-2034 |
| Pre-emphasis | 2040-2044 |
| SSB Generation | 2036-2129 |
| Hilbert TX | 2069-2087 |
| arctan3() | 1951-1967 |
| dsp_tx() ISR | 2160-2234 |
| CIC Decimation | 3228-3418 |
| Hilbert RX | 2983-3009 |
| slow_dsp() | 2970-3174 |
| process_agc() | 2712-2740 |
| process_nr() | 2881-2941 |
| filt_var() | usdx_filter.h:46-222 |
| CW Decoder | 2460-2530+ |

---

## Appendix: Testbench Files

Testbench files are located in the `test/` folder:

| File | Description |
|------|-------------|
| `testbench_ssb_modulation.cpp` | Generic SSB modulation test |
| `testbench_exact_ssb.cpp` | Exact algorithm verification |
| `testbench_tx_7100_lsb.cpp` | TX simulation at 7.1MHz |

**Compilation:**
```bash
cd test
g++ -std=c++11 -O2 -o <name> <file>.cpp
./<name>
```

---

*Document generated for uSDX Plus Orange v5.14*
