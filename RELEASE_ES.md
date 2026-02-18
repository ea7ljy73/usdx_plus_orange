# uSDX Plus Orange - Notas de Release

**Versión:** 5.00
**Base:** uSDX Legacy 1.02x
**Plataforma:** ATMEGA328P @ 20MHz
**Autor:** EA7LJY - Julian

---

## v5.00 - Refactorización y Mejoras DSP

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
- Filtro estándar 75μs
- Variable: `deemph_fm` (0=off, 1=on)

### Optimización de Filtros

Todos los filtros IIR (SSB y CW) optimizados con shifts de bits:
- `/2` → `>>1`
- `/4` → `>>2`
- `/16` → `>>4`
- `/32` → `>>5`
- `/64` → `>>6`

Impacto: ~3-4% reducción CPU

---

## Uso de Memoria

| Recurso | Uso | Disponible |
|---------|-----|------------|
| Flash | 30,652 bytes (95%) | 604 bytes |
| RAM | 1,474 bytes (71%) | 574 bytes |

## Compilación

```bash
arduino-cli compile -b arduino:avr:uno
```

## Estado

- ✅ RX funcional (SSB, CW, AM, FM)
- ✅ TX funcional (SSB, CW, AM, FM)
- ✅ VOX funcionando
- ✅ Filtros funcionando
- ✅ AGC funcionando

---

**Autor:** EA7LJY - Julian
**Fecha:** Febrero 2026
