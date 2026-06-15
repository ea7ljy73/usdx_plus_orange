# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

uSDX Plus Orange **v5.13** is a firmware for ATMEGA328P-based SDR transceivers. It implements SSB/CW/AM/FM transmission and reception with DSP features including filters, AGC, noise reduction, and VOX.

**Platform:** ATMEGA328P @ 20MHz (Arduino Uno compatible)
**Base:** usdxWHITEBUTTONS v4.00d by GW8RDI (extends uSDX Legacy 1.02x by PE1NNZ)
**Author:** EA7LJY - Julian
**Branch:** `dev` (main development branch)
**License:** MIT

## Build Commands

### Arduino CLI (Recommended)

```bash
# Compile for Arduino Uno
arduino-cli compile -b arduino:avr:uno

# Compile and check memory usage
arduino-cli compile -b arduino:avr:uno 2>&1 | grep -E "bytes.*usado|bytes.*used|Sketch uses|Global"

# Compile with hex file generation
arduino-cli compile -b arduino:avr:uno -e

# Upload (adjust port as needed)
arduino-cli upload -b arduino:avr:uno -p /dev/ttyUSB0
```

### Firmware Upload via ISP

```bash
avrdude -c avrisp -b 19200 -P /dev/ttyACM0 -p m328p -U flash:w:firmware.hex:i
```

**Important:** Do not modify fuse settings during ISP programming. Default fuses: E=FD H=D6 L=FF

## File Structure

```
usdx_plus_orange/
├── usdx_plus_orange.ino     # Main firmware (~7200 lines, monolithic)
├── usdx_settings.h          # Compile-time configuration — EDIT THIS
├── usdx_filter.h            # DSP IIR biquad filter implementations
├── RELEASE.md               # Version changelog
├── README.md                # Project documentation
│
├── usdxWHITEBUTTONS/        # REFERENCE ONLY — DO NOT MODIFY, DO NOT COPY FROM
│   └── usdxWHITEBUTTONS.ino # Original GW8RDI firmware (base for this project)
│
└── usdx-legazy/             # REFERENCE ONLY — DO NOT MODIFY, DO NOT COPY FROM
    ├── usdx.ino             # PE1NNZ legacy uSDX firmware
    ├── usdx_settings.h
    └── usdx_filter.h
```

### Reference Folder Rules

**CRITICAL:** Never modify or copy code from reference folders.

- `usdxWHITEBUTTONS/` — The direct base of this project. Consult only to understand original GW8RDI behavior when a feature needs investigation.
- `usdx-legazy/` — The PE1NNZ legacy firmware. Consult only for algorithm reference.
- All DSP improvements and fixes go into `usdx_plus_orange.ino` and `usdx_settings.h`.

## Critical Constraints

### Memory Budget (HARD LIMITS)

| Resource | Total | Used (v5.13) | % | Safety limit |
|----------|-------|-------------|---|-------------|
| Flash    | 32256 bytes | 30710 bytes | 95% | **31000 bytes (96%)** |
| RAM      | 2048 bytes  | 1464 bytes  | 71% | 1700 bytes |
| EEPROM   | 1024 bytes  | ~512 bytes  | ~50% | — |

**Rule:** Always compile and check memory after every change. STOP if flash > 31000 bytes.

```bash
arduino-cli compile -b arduino:avr:uno 2>&1 | tail -3
```

### Performance Requirements

- **TX DSP update rate:** 4800 Hz (Timer2 ISR)
- **RX ADC sampling:** 62.5 kHz oversampled, decimated to ~7.8 kHz
- **I2C speed:** ~800 kbit/s for SI5351 (88 µs per bulk register write)
- **No floating-point:** Use integer arithmetic and bit shifts only

## Architecture

### DSP Chain

**RX Path:**
```
ADC 62.5kHz → CIC decimator (N=3, R=4, 7.8kHz) → Hilbert 15-tap → Demodulator
→ process_agc() → noise_reduction() → filt_var() IIR → PWM audio out
```

**TX Path (SSB):**
```
Mic ADC 4800Hz → voice_compressor() → mic_eq() → pre_emph → ssb() Hilbert 15-tap
→ arctan3(q,i) [phase] + magn(i,q) [amplitude]
→ SI5351 freq_calc_fast() [I2C 88µs] + OCR1BL=lut[amp] [PWM]
```

**TX Path (CW):**
```
Timer2 ISR → dsp_tx_cw() → KEY_CLICK ramp (31 steps) → process_minsky() sidetone
→ SI5351 SendPLLRegisterBulk()
```

### Key DSP Algorithms

**Hilbert Transform (TX, ~line 2069/2084):**
- 15-tap FIR, 90° phase shift
- `q = ((v[0]-v[14])*2 + (v[2]-v[12])*8 + (v[4]-v[10])*21 + (v[6]-v[8])*16) / 128 + (v[6]-v[8])/2`
- **v5.10 fix:** coeff unified to 16 in both branches (was 15 in !MORE_MIC_GAIN path)

**Voice Compressor (TX, ~line 2005):**
- Envelope follower: attack `>>2`, decay `>>7` (v5.12: ~27ms release, was >>4=3ms → pumping)
- Gain reduction when envelope > threshold
- **v5.10 defaults:** enabled=1, ratio=3, threshold=128

**AGC (~line 2720):**
- `process_agc()`: centiGain/128 representation, 0.25x–255x range (~60dB)
- `process_agc_fast()`: gain/1024 representation, fast attack only
- **v5.13:** `agc_decay = 8` (stored as uint8_t 1-16; actual samples = value × 100; default 8 → 800ms)

**FM Demodulator (~line 3082):**
- Product differentiator: `ac = (ac + i) * zi; zi = i`
- **v5.10 fix:** `zi` initialized to 0 (was runtime `=i`, caused click on mode entry)

**AM DC Removal (~line 3054):**
- IIR HP: `as = ac + as_last - (as_last >> 10)` (alpha ≈ 0.999)
- **v5.10 fix:** replaced `(float)as_last * 0.9999f` with integer shift (saves ~128 bytes)

**IIR Filters (usdx_filter.h):**
- `filt_var()` with 7 modes (SSB: 1-3, CW: 4-7)
- SSB Mode 1: 0-2900Hz / Mode 2: 0-2400Hz / Mode 3: 0-1800Hz
- CW Mode 4: ±250Hz / Mode 5: ±100Hz / Mode 6: ±50Hz / Mode 7: ±18Hz

**KEY_CLICK (~line 2267):**
- 31-step raised-cosine ramp on CW TX/RX transitions
- Stored in PROGMEM: `ramp[]`
- **v5.10:** enabled by default via `#define KEY_CLICK 1`

### SI5351 Phase Modulation

- `freq_calc_fast(df)`: computes PLL registers from frequency shift `df`
- `SendPLLRegisterBulk()`: sends 4 bytes over I2C (88 µs)
- Update rate: 4800 Hz (every 208 µs); I2C latency creates ~30-50 µs phase-amplitude misalignment

### TX Amplitude Control

- `magn(i,q)` → `_amp`: Chebyshev approximation of √(i²+q²), error < 0.95 dB
- `_amp << drive` → clipped to 255 → `lut[_amp]` → `OCR1BL` (PWM)
- `build_lut()`: linear mapping `lut[i] = i * (pwm_max - pwm_min) / 255 + pwm_min`
- WHITE_BUTTONS: `pwm_min = 0`, `pwm_max = 160`

## Configuration System (usdx_settings.h)

Edit `usdx_settings.h` for all compile-time configuration. Never put `#define` switches directly in the `.ino`.

### Current Active Configuration (v5.13)

```
Hardware model:    WHITE_BUTTONS
SI5351 address:    0x60
LPF bank:          LPF_SWITCHING_DL2MAN_USDX_REV3 (8-band latching)
CAT:               enabled (115200 baud, CAT_FAST)
CW:                KEY_CLICK enabled / KEYER disabled (memory) / CW_DECODER enabled
VOX:               VOX_ENABLE
RIT:               RIT_ENABLE
Semi-QSK:          SEMI_QSK
TX:                TX_ENABLE
```

### Feature Cost Reference

| Feature | Flash cost | RAM cost |
|---------|-----------|---------|
| DIAG | +1308 bytes | — |
| CAT | +4150 bytes | — |
| KEYER | +500 bytes | +20 bytes |
| CW_DECODER | +1468 bytes | — |
| KEY_CLICK | +128 bytes | — |
| NR_FIR | +120 bytes | +52 bytes |
| RIT_ENABLE | +200 bytes | — |

## Development Guidelines

### Code Style

- **Memory-first:** bit shifts over division (`>> 3` not `/ 8`)
- **PROGMEM:** constants in flash (`const uint8_t arr[] PROGMEM = {...}`)
- **Fixed-width types:** `int8_t`, `uint16_t`, `int32_t` — no plain `int` in DSP
- **Inline DSP:** `inline` on frequently-called functions
- **No dynamic allocation:** no `malloc()`/`new`, use static buffers
- **No float in ISR:** float operations are slow on AVR; use integer arithmetic

### Workflow

1. Read relevant code section before any change
2. Make one targeted change at a time
3. Compile and check memory after EACH change
4. Stop if flash > 31000 bytes
5. Test on hardware before committing

### Conditional Compilation

All optional features use `#ifdef` guards. When modifying:
- Never break non-guarded code paths
- Test that the build compiles both with and without a feature
- Only add `#define` switches to `usdx_settings.h`

## Current Known Issues (post v5.13)

These are identified but not yet fixed — require careful memory analysis before attempting:

1. **Drive boost hard clip** (line ~2102): `_amp << drive` with hard clip at 255. With compressor active, mitigated but can still clip on very loud input.

2. **MIC_ATTEN always 0** (line 2151): no configurable microphone attenuation.

## Testing Protocol

### After Every Change

```bash
arduino-cli compile -b arduino:avr:uno 2>&1 | tail -3
# Verify: flash < 31000 bytes, RAM < 1700 bytes
```

### Functional Tests (on hardware)

1. **RX SSB:** S-meter stable, no AGC pumping, audio clear
2. **RX CW:** CW filters 4-7 working, decoder active
3. **RX FM:** Enter FM mode — no click or transient
4. **TX SSB:** Voice clear, no distortion, compressor active
5. **TX CW:** No click on key ON/OFF (KEY_CLICK ramp)
6. **Menu:** All items navigate correctly, EEPROM save/restore
7. **CAT:** TS-480 commands respond at 115200 baud
8. **LPF bank:** Band switching activates correct filter relay

## References

- **Original uSDX (PE1NNZ):** https://github.com/threeme3/usdx
- **uSDX Forum:** https://groups.io/g/ucx
- **SI5351 Application Note:** Silicon Labs AN619
- **ATMEGA328P Datasheet:** Microchip ATmega328P-DS40002061A

## Contact

**Author:** EA7LJY - Julian
**Email:** ea7ljy73@gmail.com
