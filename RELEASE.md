# uSDX Plus Orange - Release Notes

**Version:** 5.11
**Base:** uSDX Legacy 1.02x / usdxWHITEBUTTONS v4.00d (GW8RDI)
**Platform:** ATMEGA328P @ 20MHz
**Author:** EA7LJY - Julian

---

## v5.11 - TX Modulation and RX Filter Fixes

**Memory:** 30594 bytes flash (94%), 1465 bytes RAM (71%)

### TX Improvement: Amplitude-Phase Alignment

`OCR1BL` (amplitud PWM a PA) se envía ahora **antes** de `SendPLLRegisterBulk()`,
en ambos paths (MULTI_ADC y single ADC).

- Antes: fase se actualizaba ~88µs antes que la amplitud (error de alineación)
- Ahora: amplitud se actualiza ~140µs antes de que el PLL se estabilice
- Efecto: menor distorsión de envolvente en SSB, señal más limpia en banda lateral

Esto restaura el comportamiento original del diseño (estaba en comentarios desde el código base de GW8RDI).

### RX Improvement: Ganancia uniforme en filtros SSB

Filtros SSB 2 (0-2400Hz) y 3 (0-1800Hz): segunda sección biquad `>>2` → `>>1`
para igualar la ganancia del filtro 1 (0-2900Hz).

- Antes: cambiar entre filtros SSB 1↔2 o 1↔3 producía un salto de ~6dB
- Ahora: los tres filtros SSB tienen ganancia consistente, sin saltos de volumen

---

## v5.10 - RX/TX DSP Quality Improvements

**Memory:** 30602 bytes flash (94%), 1465 bytes RAM (71%)

### Bug Fixes

- **FM Demod:** `zi` inicializado a 0 (antes = valor arbitrario → click al entrar en FM)
- **AM DC removal:** Reemplazado `float * 0.9999f` por aritmética entera `as_last - (as_last >> 10)` (alpha ≈ 0.999, ahorra ~128 bytes flash y ciclos CPU)

### TX Improvements

- **Voice Compressor activado por defecto** (`comp_enable = 1`): mejor potencia media en SSB, menos IMD
- **Ratio reducido:** 4:1 → 3:1 (menos distorsión armónica, voz más natural)
- **Threshold ajustado:** 180 → 128 (activación más temprana, mayor rango de compresión útil)
- **Pre-énfasis reducido:** 12dB/oct → 6dB/oct (`pre_emph = 1`), menos sibilancia con cápsulas electret
- **KEY_CLICK activado:** rampa de amplitud CW al ON/OFF para eliminar clicks audibles
- **Hilbert TX unificado:** coeficiente `v[6]-v[8]` corregido 15→16 (matches rama MORE_MIC_GAIN, mejora rechazo de banda lateral en TX SSB)

### RX Improvements

- **AGC decay:** 400 → 800 (~800ms), más natural y cómodo para escucha SSB prolongada

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
