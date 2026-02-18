# uSDX Plus Orange - Release Notes

**Version:** 5.00
**Base:** uSDX Legacy 1.02x
**Platform:** ATMEGA328P @ 20MHz
**Author:** EA7LJY - Julian

---

## v5.00 - Refactorization and DSP Improvements

### TX - Transmission

**Voice Compressor (ALC)**
- Ratio: 4:1, Threshold: 180, Attack: 2, Decay: 8
- Benefit: +6dB average talk power
- Variable: `comp_enable` (0=off, 1=on)

**Microphone EQ 2-band**
- Low shelf: 300Hz (-6dB to +6dB)
- High shelf: 2.5kHz (-6dB to +6dB)
- Variables: `eq_low`, `eq_high`

**Pre-emphasis**
- Options: 0=off, 1=6dB/oct, 2=12dB/oct
- Variable: `pre_emph` (default: 2)

### RX - Reception

**Variable RF Attenuator**
- Range: 0-20dB in 1dB steps
- Variable: `rf_atten`

**Configurable AGC Decay**
- Range: 100-800 (default: 400)
- Variable: `agc_decay`

**FM De-emphasis**
- Standard 75μs filter
- Variable: `deemph_fm` (0=off, 1=on)

### Filter Optimization

All IIR filters (SSB and CW) optimized with bit shifts:
- `/2` → `>>1`
- `/4` → `>>2`
- `/16` → `>>4`
- `/32` → `>>5`
- `/64` → `>>6`

Impact: ~3-4% CPU reduction

---

## Memory Usage

| Resource | Usage | Available |
|----------|-------|-----------|
| Flash | 30,652 bytes (95%) | 604 bytes |
| RAM | 1,474 bytes (71%) | 574 bytes |

## Compilation

```bash
arduino-cli compile -b arduino:avr:uno
```

## Status

- ✅ RX functional (SSB, CW, AM, FM)
- ✅ TX functional (SSB, CW, AM, FM)
- ✅ VOX working
- ✅ Filters working
- ✅ AGC working

---

**Author:** EA7LJY - Julian
**Date:** February 2026
