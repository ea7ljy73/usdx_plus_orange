# uSDX Plus Orange - Release Notes

**Version:** 5.15
**Base:** uSDX Legacy 1.02x / usdxWHITEBUTTONS v4.00d (GW8RDI)
**Platform:** ATMEGA328P @ 20MHz
**Author:** EA7LJY - Julian

---

## v5.15 - TX Menu Improvements

**Memory:** 31,150 bytes flash (96%), 1,465 bytes RAM (71%)

### New TX Menu Features

#### Microphone EQ (Bass/Treble)
- Added **EQ Bass** control to TX menu (range: -7 to +7)
- Added **EQ Treble** control to TX menu (range: -7 to +7)
- Location: `usdx_plus_orange.ino:5158-5163`
- Allows fine-tuning of microphone frequency response

#### TX Menu Reordering
- Menu items now appear in logical order:
  1. TX Drive (3.3)
  2. TX Delay (3.4)
  3. MOX (3.5)
  4. TX Comp (3.6)
  5. TX Emph (3.7)
  6. EQ Bass (3.8)
  7. EQ Treble (3.9)

### Notes

- EQ processing was already implemented in TX path (lines 1997-2033)
- Now accessible via menu for user adjustment
- Default values: Bass=0, Treble=0 (flat response)

---

## v5.13 - Configurable menu: AGC decay, TX compressor, TX pre-emphasis

**Memory:** 30710 bytes flash (95%), 1464 bytes RAM (71%)

### New menu parameters

Three new items accessible from the menu rotary encoder:

- **AGC Dcy** (row 1, col E) — `agc_decay`: AGC release time 1-16 (×100 samples). Default 8 (~800ms). Shorter values make AGC react faster to signal disappearance; longer values give smoother listening.
- **TX Comp** (row 3, col 6) — `comp_enable`: Enable/disable TX voice compressor (Off/ON). Default ON. Compressor adds ~6dB average talk power and reduces clipping.
- **TX Emph** (row 3, col 7) — `pre_emph`: TX microphone pre-emphasis 0-3 (0=off, 1=6dB/oct, 2=12dB/oct, 3=18dB/oct). Default 1 (6dB/oct, suitable for electret capsules).

### Technical details

- `agc_decay` type changed `uint16_t→uint8_t` (stored value ×100 = actual sample count; `decayCount = (uint16_t)agc_decay * 100`)
- EEPROM addresses: `AGC_DECAY`→0x1E, `COMP_EN`→0x36, `PRE_EMPH`→0x37
- `eeprom_version` bumped (VERSION "5.12"→"5.13"): EEPROM auto-reset on first boot; saved frequency resets to default (re-enter after flashing)
- N_PARAMS updated 47→50 to include new visible menu items

---

## v5.12 - Voice SSB Quality (TX compression + RX clarity)

**Memory:** 30594 bytes flash (94%), 1466 bytes RAM (71%)

### TX: Compressor improvements

**Decay time:** `>>4` → `>>7` (release 3ms → 27ms)
- Antes: el envelope de compresión se relajaba en 3ms → "pumping" audible entre sílabas
- Ahora: 27ms de release → compresión suave y transparente, similar a radios comerciales
- El ataque sigue siendo rápido (~3ms) para capturar picos de voz

**Overflow fix:** `in * gain` → `(int32_t)in * gain / comp_envelope`
- Antes: multiplicación int16×int16 podía desbordar en picos altos de micrófono → distorsión dura
- Ahora: aritmética 32 bits garantiza resultado correcto sin coste de flash extra

### RX: NR default desactivado para voz SSB

**NR default:** `nr=2` → `nr=0`
- El filtro EA con nr=2 corta a ~900Hz, atenuando significativamente las frecuencias de voz 1-3kHz
- Con nr=0, el filtro IIR de banda (filt_var) es el único limitador de ancho de banda → respuesta plana en SSB
- Los usuarios pueden activar NR desde el menú si desean reducción de ruido adicional
- CW no se ve afectado (ya forzaba nr=0 al cambiar de modo)

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

## Memory Usage (v5.14)

| Resource | Usage | Available |
|----------|-------|-----------|
| Flash | ~30,700 bytes (95%) | ~1556 bytes |
| RAM | ~1,464 bytes (71%) | 584 bytes |

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

## Operation

Currently, the following functions have been assigned to shortcut buttons (L=left, E=encoder, R=right) and menu-items:

| Menu Item           | Function                                     | Button |
| ------------------- | -------------------------------------------- | ------ |
| 1.1 Vol             | Audio level (0..16) & power-off/on (turn left) | **E +turn** |
| 1.2 Mode            | Modulation (LSB, USB, CW, AM, FM) | **R** |
| 1.3 FilterBW        | Audio passband (Full, 300..3000, 300..2400, 300..1800, 500, 200, 100, 50 Hz), this also controls the SSB TX BW. | **R double** |
| 1.4 Band            | Band-switch to pre-defined CW/FT8 freqs (80,60,40,30,20,17,15,10m) | **E double** |
| 1.5 Tune Rate       | Tuning step size 10M, 1M, 0.5M, 100k, 10k, 1k, 0.5k, 100, 10, 1 | **E or E long** |
| 1.6 VFO Mode        | Selects different VFO, or RX/TX split-VFO (A, B, Split) | **2x R long** |
| 1.7 RIT             | RX in transit (ON, OFF) | **R long** |
| 1.8 AGC             | Automatic Gain Control (OFF, Fast, Slow) | |
| 1.9 NR              | Noise-reduction level (0-8), load-pass & smooth | |
| 1.10 ATT            | Analog Attenuator (0, -13, -20, -33, -40, -53, -60, -73 dB) | |
| 1.11 ATT2           | Digital Attenuator in CIC-stage (0-16) in steps of 6dB | |
| 1.12 S-Meter        | Type of S-Meter (OFF, dBm, S, S-bar) | |
| 1.13 SWR Meter      | SWR Meter (OFF, ON) | |
| 1.14 AGC Dcy        | AGC decay time 1-16 (x100 samples), default 8 (~800ms) | |
| 2.1 CW Decoder      | Enable/disable CW Decoder (ON, OFF) | |
| 2.2 CW Tone         | CW Filter+Side-tone (600, 700) | |
| 2.3 CW Off          | CW Offset (300..2000 Hz) | |
| 2.4 Semi QSK        | On TX silents RX on CW sign and word spaces | |
| 2.5 Keyer Speed     | CW Keyer speed in Paris-WPM (1..60) | |
| 2.6 Keyer Mode      | Type of keyer (Iambic-A, -B, Straight) | |
| 2.7 Keyer Swap      | to swap keyer DIH, DAH inputs (ON, OFF) | |
| 2.8 Practice        | to disable TX for practice purposes (ON, OFF) | |
| 2.9 Tone Vol        | CW Side-tone volume (0..16) | |
| 3.1 VOX             | Voice Operated Xmit (ON, OFF) | |
| 3.2 Noise Gate      | Audio threshold for SSB TX and VOX (0-255) | |
| 3.3 TX Drive        | Transmit audio gain (0-8) in steps of 6dB, 8=constant amplitude for SSB | |
| 3.4 TX Delay        | Delays TX to allow PA relay to be fully switched on before TX (0-255 ms) | |
| 3.5 MOX             | Monitor on Xmit (audio unmuted during transmit) | |
| 3.6 TX Comp         | TX voice compressor (ON/OFF), adds ~6dB average talk power | |
| 3.7 TX Emph         | TX microphone pre-emphasis (0=off, 1=6dB/oct, 2=12dB/oct, 3=18dB/oct) | |
| 3.8 EQ Bass         | TX microphone bass EQ (-7 to +7) | |
| 3.9 EQ Treble       | TX microphone treble EQ (-7 to +7) | |
| 4.1 CQ Interval     | Idle time in seconds before new CQ Message is given (0-60) | |
| 4.2 CQ Msg          | CQ Message text, pressing left-button in menu will start sending | **L** |
| 8.1 PA bias min     | PA amplitude PWM level (0-255) for representing 0% RF output | |
| 8.2 PA max          | PA amplitude PWM level (0-255) for representing 100% RF output | |
| 8.3 Ref frq          | Actual si5351 crystal frequency, used for frequency-calibration | |
| 8.4 IQ phase        | RX I/Q phase offset in degrees (0..180 degrees) | |
| 10.1 Backlight      | Display backlight (ON, OFF) | |
| power-up             | Reset to factory settings | **E long** |
| main                | Tune frequency (20kHz..99MHz) | **turn** |
| main                | Quick menu | **L +turn** |
| main                | Menu enter | **L** |
| RIT                 | RIT back | **R** |
| menu                | Menu back | **R** |

---

**Disclaimer:** I am not responsible for any damage this firmware may cause to devices on which it can be applied. Use at your own risk.

**Author:** EA7LJY - Julian
**Date:** February 2026
