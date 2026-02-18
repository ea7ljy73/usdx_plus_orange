# uSDX Plus Orange - Release Notes v5.00

## Refactorización y Mejoras

### TX - Transmisión

**Compresor de Voz (ALC)**
- Ratio: 4:1, Umbral: 180, Attack: 2, Decay: 8
- Beneficio: +6dB potencia media de voz
- Variable: `comp_enable` (0=off, 1=on)

**EQ Micrófono 2 bandas**
- Low shelf: 300Hz (-6dB a +6dB)
- High shelf: 2.5kHz (-6dB a +6dB)
- Variables: `eq_low`, `eq_high`

**Pre-emphasis**
- Opciones: 0=off, 1=6dB/oct, 2=12dB/oct
- Variable: `pre_emph` (defecto: 2)

### RX - Recepción

**Atenuador RF Variable**
- Rango: 0-20dB en pasos de 1dB
- Variable: `rf_atten`

**Decay AGC Configurable**
- Rango: 100-800 (defecto: 400)
- Variable: `agc_decay`

**De-emphasis FM**
- Filtro 75μs estándar
- Variable: `deemph_fm` (0=off, 1=on)

### Optimización de Filtros

Todos los filtros IIR (SSB y CW) optimizados con shifts:
- `/2` → `>>1`
- `/4` → `>>2`
- `/16` → `>>4`
- `/32` → `>>5`
- `/64` → `>>6`

Impacto: ~3-4% reducción CPU

---

## TX - Transmission

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

## Uso de Memoria / Memory Usage

| Recurso | Uso | Disponible |
|---------|-----|------------|
| Flash | 30,652 bytes (95%) | 604 bytes |
| RAM | 1,474 bytes (71%) | 574 bytes |

## Estado / Status

- ✅ RX funcional / functional (SSB, CW, AM, FM)
- ✅ TX funcional / functional (SSB, CW, AM, FM)
- ✅ VOX funcionando / working
- ✅ Filtros funcionando / filters working
- ✅ AGC funcionando / working

---

**Autor:** EA7LJY - Julian
**Fecha:** Febrero 2026
