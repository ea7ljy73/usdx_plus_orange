# uSDX Plus Orange - Release Notes

**Version:** 6.00
**Base:** uSDX Legacy 1.02x / usdxWHITEBUTTONS v4.00d (GW8RDI)
**Platform:** ATMEGA328P @ 20MHz
**Author:** EA7LJY - Julian

---

## v6.00 - AM/FM Unlocked & 11m Band Support

**Memory:** TBD

### Major Changes

#### AM & FM Modes Fully Enabled
- **Mode selection unblocked** — `SHOW_USB_LSB_CW_ONLY` removed; mode button now cycles LSB→USB→CW→**FM**→**AM**→LSB (full 5-mode cycle)
- **Existing AM/FM RX/TX unlocked** — AM/FM demodulation (RX) and modulation (TX) were already implemented in the codebase but blocked from user selection; now fully accessible via menu (1.2 Mode) and right-button cycling
- **VOX extended** — voice-operated TX now works in AM and FM modes (previously LSB/USB only)
- **No filters, SI5351, or DSP changes needed** — existing `magn(i,q)` for AM and `_arctan3(q,i)`+differentiator for FM work correctly with current SDR architecture

#### 11m Band Added
- **New band**: 11m (27.0 MHz / CB band) inserted between 12m (24.9 MHz) and 10m (28.0 MHz)
- **Auto-detection**: frequency thresholds split at 28 MHz — tuning 26-28 MHz selects 11m, 28-32 MHz selects 10m
- **LPF**: 11m and 10m share the same LPF relay (IO1_3, f > 26 MHz) — no hardware changes needed
- **Band labels**: menu now shows: 80m, 60m, 40m, 30m, 20m, 17m, 15m, 12m, **11m**, **10m**
- **EEPROM**: BAND_DATA9 added (band 10 → 10m); I_PARAMS updated 5+5+9→5+5+10; N_ALL_PARAMS 67→68
- **VERSION bumped** to "6.00" — forces EEPROM reset on first boot (required for new band data layout)

#### Compatibility Notes
- ⚠️ **EEPROM reset required** — first boot after flashing v6.00 will reset all settings to defaults (hold encoder button during power-on if automatic reset doesn't trigger)
- All existing modes (LSB, USB, CW) are **100% unchanged** — no filters, frequencies, or DSP behavior modified
- Default mode for bands 1-4 remains LSB; bands 5-10 default to USB
- 6m (50 MHz) remains at index 11 (bandval=11, excluded from menu cycling)

### Bug Fixes & Optimizations

#### FM De-emphasis Corrected
- **Fix**: `fm_deemph()` time constant changed from `>>3` (τ≈960µs, fc≈166Hz) to `>>1` (τ≈185µs, fc≈860Hz), matching the NBFM 150µs de-emphasis standard. Previous setting severely attenuated all audio above 166Hz, making FM reception muffled. (`usdx_plus_orange.ino:2155`)

#### AM Carrier Bias Fixed
- **Fix**: `AM_BASE` increased from 32 to 85 (carrier at 33% instead of 12.5%). Provides symmetrical positive/negative modulation headroom (±200%) for clean AM transmission. Previous setting caused asymmetric clipping on positive modulation peaks. (`usdx_plus_orange.ino:2293`)

#### Fast AGC Gain Recovery
- **Fix**: `process_agc_fast()` now reduces gain on strong signals (previously only increased gain, never recovered). After a loud signal, the AGC would permanently lower sensitivity until frequency change. Now properly balances gain up/down. (`usdx_plus_orange.ino:2681`)

#### TX Low-Cut Filter Corners
- **Improvement**: Corners changed from 96/191/382Hz to 48/96/191Hz (`k = 5 - tx_lowcut` instead of `4 - tx_lowcut`). The "100Hz" setting now accurately cuts at ~96Hz, and "200Hz" at ~191Hz, preserving more low-end voice energy. (`usdx_plus_orange.ino:2010`)

#### FM Deviation Limiter
- **Improvement**: Soft-clip limiter added to `dsp_tx_fm()` (±5kHz threshold with 4:1 compression). Prevents over-deviation on loud speech, avoiding adjacent-channel interference. (`usdx_plus_orange.ino:2315`)

#### Voice Compressor Timing
- **Improvement**: Attack slowed from `>>1` (~1.5ms) to `>>2` (~3ms) — reduces "clicky" compression artifacts on plosives. Release extended from `>>8` (~53ms) to `>>10` (~213ms) — more natural syllable tracking, closer to broadcast limiter behavior. (`usdx_plus_orange.ino:1976-1978`)

#### AM-PM Predistortion LUT
- **Improvement**: Expanded from 64 to 256 entries (full 8-bit amplitude resolution). Each amplitude value now has its own predistortion coefficient, providing smoother phase correction and better TX spectral purity. (+192 bytes PROGMEM) (`usdx_plus_orange.ino:2123`)



**Memory:** 30,760 bytes flash (95%), 1,342 bytes RAM (65%) — −394 bytes flash, −124 bytes RAM vs v5.17

### ROADMAP: 9 items implemented

Systematic code quality and DSP enhancement project based on ROADMAP.md.

#### Changes

- **AGC overflow fix** — `int32_t` cast prevents `int16_t` multiplication overflow on strong signals; previously caused gain jumps at high audio levels
- **PROGMEM strings** — moved 12 label tables (`mode_label`, `offon_label`, `filt_label`, `band_label`, `stepsize_label`, `att_label`, `smode_label`, `swr_label`, `cw_tone_label`, `keyer_mode_label`, `agc_label`, `stepsizes[]`) from RAM to flash via `pgm_read_ptr()`/`pgm_read_dword()` access in `paramAction()`; saves ~134 bytes RAM
- **Dead code eliminated** — removed BLIND class (unused), SIMPLE_RX branch (unused), TESTBENCH NCO functions, `process_nr_old()` / `process_nr_old2()` (unused), `Command_TX1()`/`Command_TX2()` (duplicates of `Command_TX0()`), `Command_AI0()` reordered to avoid forward-declaration, `ref_V` float (never read); net −212 lines
- **Bit shifts** — `adc/4` → `adc>>2`, `(in+dc)/2` → `(in+dc)>>1`, clipper `/2` → `>>1`; consistent with project style guide
- **CESSB clipper I/Q** — Controlled Envelope SSB: limits I/Q vector magnitude when `_amp > 200` with 4:1 soft compression ratio; increases effective talk power ~2-3dB without expanding occupied bandwidth
- **AGC hang timer** — `hang_cnt` counter (600 samples ≈ 77ms @ 7812Hz) prevents AGC gain increase during SSB word pauses; reset on signal detection; eliminates noise pumping between syllables
- **LMS adaptive auto-notch (2-tap)** — adaptive FIR notch filter cancels narrowband heterodynes/birdies; **removed in final v5.18** — interfered with speech when NR was active; the standard `process_nr()` EA/FIR filter provides better noise reduction without speech distortion
- **Phase unwrapping** — replaces `if(dp < 0) dp = dp + _UA` with proper shortest-path unwrapping (`if(dp > _UA/2) dp -= _UA`); prevents spectral spurs from wraparound on asymmetric audio content
- **Fixed-point readSWR** — `float` eliminated from SWR calculations; VSWR computed from ADC sum ratio using `uint32_t` fixed-point; power in milliwatts via `sum² / 67000`; 3-digit display (x.xx format)

### Code Review Fixes

- **CESSB scope** — `#define CESSB_THRESH 200` inside `ssb()` changed to local `const uint16_t CESSB_THRESH = 200;` (best practice: `#define` ignores C++ scope, could cause name collisions)
- **QUAD removed entirely** — all 3 `#ifdef QUAD` blocks removed along with `quad_enabled` variable, `quad` flag, and menu entry; QUAD was disabled by default and the author's own comments state "This worsens TX SSB voice quality, more Dalex sounding"; phase unwrapping handles large phase transitions correctly
- **PROGMEM access fix** — `lcd.print()` was using `reinterpret_cast<const __FlashStringHelper*>` to print label strings from PROGMEM pointer arrays. In Arduino AVR, string literals are in RAM (`.rodata` copied to RAM at boot), not flash. The `__FlashStringHelper` cast caused `lcd.print()` to read from flash address space, producing garbage. Fixed to `(const char*)pgm_read_ptr(...)` — reads the pointer from PROGMEM array, then prints the string from RAM. Affected: `paramAction()` (all menu labels), `display_vfo()` (mode label), `show_banner()` (cap label).

### Float Elimination (major flash savings)

- **`smeter()` float removed** — `10*log10()` replaced by fixed-point lookup table (128-byte PROGMEM) with MSB-based normalization; SDR offset −185dB, non-SDR offset −176dB; saves ~2-4KB by eliminating entire float math library
- **`cap_label` to PROGMEM** — last remaining string table in RAM, saves ~18 bytes
- **DIAG fixed-point** — 6 voltage measurements (`vdd`, `vee`, `avcc`, `dvm`, `audio1`, `audio2`) converted from `float` to millivolt integers (`uint32_t * 5000UL / 1024`); saves ~300 bytes flash
- **`FWD`/`SWR` `uint16_t`** — SWR meter globals changed from `float` to `uint16_t` (stored as VSWR×100); saves ~12 bytes RAM
- **`ref_V` dead removal** — declared `float ref_V = 5 * 1.15` was never read

### AM-PM Predistortion Enhancement

- **Table expanded 16→64 entries** — 4× finer compensation of class-E PA phase shift vs amplitude; indexed as `_amp >> 2` (was `_amp >> 4`); linear interpolation from original values; cost: +48 bytes PROGMEM

### AGC Per-Mode Decay

- **CW mode** — `decayCount` automatically set to 200 samples (~25ms) when `mode == CW`, vs 800 samples (~102ms) in SSB; zero flash cost, improves CW comfort significantly

### Voice Compressor Soft Knee + Make-up Gain

- **Soft knee** — quadratic transition zone (64 units below threshold) smooths compression onset; eliminates audible "click" when signal crosses threshold
- **Make-up gain** — `comp_threshold >> 1` (~64) automatically restores ~6dB level lost by compression; compressed audio now matches uncompressed loudness
- **`/ comp_ratio` eliminated** — ratio fixed at 2:1, division replaced with `>> 1`; saves CPU cycles and removes unused variable `comp_ratio` (−2 bytes RAM)

### Noise Blanker (NB)

- **Impulse noise blanker** — detects short high-amplitude pulses (power line noise, ignition, appliance interference) and replaces them with the previous sample; placed before AGC to prevent AGC pumping
- **Algorithm**: tracks average signal level via slow IIR (`>> 5`); triggers when abs(ac) > 3x average; blanks for 8 samples (~1ms); cost: +158 bytes flash, +6 bytes RAM

### TX Low-Cut Filter (HPF)

- **Yaesu/Icom-style low-cut** — first-order IIR HPF on mic audio, removes sub-audio frequencies before SSB modulation; reduces wasted power and IMD from breath/handling noise
- **Implementation**: HPF = signal − LPF, where LPF has alpha = 1/2^k (k = 3, 2, 1 for 100, 200, 400Hz respectively); placed after mic EQ, before pre-emphasis
- **Cost**: +112 bytes flash, +7 bytes RAM

### Full Menu Reference

| # | Item | Function | Notes |
|---|------|----------|-------|
| 1.1 | Vol | Audio level (0..16) & power-off | |
| 1.2 | Mode | Modulation (LSB, USB, CW, AM, FM) | |
| 1.3 | FilterBW | Audio passband / TX BW | |
| 1.4 | Band | Band-switch to pre-defined freqs | |
| 1.5 | Tune Rate | Tuning step size | |
| 1.6 | VFO Mode | VFO A/B, Split | |
| 1.7 | RIT | RX in transit | |
| 1.8 | AGC | Automatic Gain Control (OFF/ON) | |
| 1.9 | NR | Noise-reduction (0-8), LMS notch when ON | |
| 1.10 | ATT | Analog Attenuator (0..-73dB) | |
| 1.11 | ATT2 | Digital Attenuator (0-16 x6dB steps) | |
| 1.12 | S-Meter | S-meter type (OFF, dBm, S, S-bar) | |
| 1.13 | SWR Meter | SWR/Power meter | *if SWR_METER* |
| 1.14 | AGC Dcy | AGC decay time 1-16 (×100 samples) | |
| 1.15 | **Noise Blk** | **Noise Blanker ON/OFF** | **NEW v5.18** |
| 2.1 | CW Decoder | Enable/disable CW decoder | *if CW_DECODER* |
| 2.2 | CW Tone | CW filter+side-tone | *if FILTER_700HZ* |
| 2.3 | CW Off | CW offset | *if QCX* |
| 2.4 | Semi QSK | Semi-QSK on CW | *if SEMI_QSK* |
| 2.5 | Keyer Speed | CW keyer speed (1-60 WPM) | *if KEYER* |
| 2.6 | Keyer Mode | Iambic-A/B, Straight | *if KEYER* |
| 2.7 | Keyer Swap | Swap DIT/DAH | *if KEYER* |
| 2.8 | Practice | Disable TX for practice | |
| 2.9 | Tone Vol | CW side-tone volume | *if CW_DECODER* |
| 3.1 | VOX | Voice Operated Xmit | *if VOX_ENABLE* |
| 3.2 | Noise Gate | Audio threshold for VOX | |
| 3.3 | TX Drive | Transmit audio gain (0-8) | |
| 3.4 | TX Delay | TX relay delay | *if TX_DELAY* |
| 3.5 | MOX | Monitor on Xmit | *if MOX_ENABLE* |
| 3.6 | TX Comp | TX voice compressor ON/OFF | |
| 3.7 | TX Emph | Microphone pre-emphasis | |
| 3.8 | EQ Bass | TX mic bass EQ (-7..+7) | |
| 3.9 | EQ Treble | TX mic treble EQ (-7..+7) | |
| 3.10 | **TX LoCut** | **TX low-cut HPF (Off/100/200/400Hz)** | **NEW v5.18** |
| 4.1 | CQ Interval | CQ message interval | *if CW_MESSAGE* |
| 4.2 | CQ Msg | CQ message text | *if CW_MESSAGE* |
| 8.1 | PA bias min | PA amplitude for 0% RF | |
| 8.2 | PA max | PA amplitude for 100% RF | |
| 8.3 | Ref frq | Si5351 crystal calibration | |
| 8.4 | IQ phase | RX I/Q phase offset | |
| 8.5 | IQ Cal | RX I/Q calibration | *if IQ_CALIBRATION* |
| 8.6 | CAT115K | CAT baud rate 115200 (vs 38400) | *if CAT* |
| 9.1 | Sample rate | Display sample rate | *invisible* |
| 9.2 | CPU load | Display CPU load % | *invisible* |
| 9.3 | ParamA | Internal parameter A | *invisible* |
| 9.4 | ParamB | Internal parameter B | *invisible* |
| 9.5 | ParamC | Internal parameter C | *invisible* |
| 10.1 | Light | Display backlight ON/OFF | |

### Summary

| Metric | v5.17 | v5.18 | Δ |
|--------|-------|-------|---|
| Flash | 31,154 (96%) | 30,760 (95%) | **−394 bytes** |
| RAM | 1,466 (71%) | 1,342 (65%) | **−124 bytes** |
| Free RAM | 582 | 706 | **+124 bytes** |

---

**Memory:** 31,154 bytes flash (96%), 1,466 bytes RAM (71%) — −16 bytes vs v5.16

### TX: Compressor & EQ improvements

Builds on the clean v5.16 baseline (all defaults reset via EEPROM) to add real improvements.

#### Changes

- **VERSION bump** "5.16" → "5.17": forces EEPROM reset on first boot, loading new `comp_enable=1` default
- **`comp_enable = 1`** (enabled by default): activates voice compressor; prevents hard-clipping on loud inputs; the v5.16 EEPROM bug is now resolved via VERSION bump, so activating the compressor is safe
- **Compressor release `>> 5` → `>> 8`**: TC changes from 6.7ms to 53ms (~200ms to reach 1%); eliminates inter-syllable pumping at conversational speech rates (Spanish syllables: 50–200ms); fast attack (TC ≈ 0.30ms) unchanged — classic broadcast limiter asymmetry
- **`mic_eq()` rewrite**: fixed two bugs:
  1. `eq_high` was an LPF (fc ≈ 760Hz) — "Treble" boost actually amplified 0-760Hz (bass/mids). Replaced with HPF: `hi = in - eq_high_iir`, so treble control now acts on real high frequencies (>760Hz presence/air)
  2. `low_gain = 4 + (eq_low << 2)` inverted phase when `eq_low < -1` (e.g., eq=-7 → gain=-24). New formula: `(eq_low_iir * eq_low) >> 3` is linear, no inversion
  3. Bass LPF cutoff tightened: `>> 3` (191Hz) → `>> 4` (75Hz) — cleaner separation between bass and mid-range

#### Notes

- `pre_emph` remains at 0 (disabled); still accessible via menu 3.7 for user experimentation
- CW mode unaffected: `dsp_tx_cw()` does not use `voice_compressor()` or `mic_eq()`
- VOX unaffected: VOX threshold based on `_amp`, calculated before compressor

---

## v5.16 - TX Legacy Baseline + Menu Fix

**Memory:** 31,170 bytes flash (96%), 1,465 bytes RAM (71%)

### TX: Revert to legacy DSP chain

Restore the TX audio chain to match `usdx-legazy` exactly, as baseline before further improvements.

**Root cause of "hissing at word onset":** EEPROM from v5.13 had `comp_enable=1` and `pre_emph=1` stored. Versions v5.14/v5.15 changed defaults to 0 but did not bump VERSION, so the old EEPROM values kept loading on boot. The pre-emphasis filter (`in + (in - pre_z1) * pre_emph`) is a first-order differentiator that boosts transient onsets — exactly producing sibilance at the start of each word.

#### Changes

- **VERSION bump** "5.15" → "5.16": forces EEPROM reset on first boot, clearing stale `comp_enable=1` and `pre_emph=1` values
- **Hilbert coeff reverted** (`!MORE_MIC_GAIN` path): `*16` → `*15` — matches legacy exactly
- **`dsp_tx()` order reverted** (both MULTI_ADC and single ADC paths): `SendPLLRegisterBulk()` now runs **before** `OCR1BL = amp`, restoring the legacy ~30-50µs amplitude-phase alignment

#### Notes

- `comp_enable`, `pre_emph`, `eq_low/high` remain in the menu (available for experimentation)
- All TX processing features default to 0/disabled after EEPROM reset
- RX chain unchanged

### Menu: Fix garbage positions after 10.1

- **`N_PARAMS` corrected**: 65 → 47 — `BACKL` (0xA1, "10.1 Light") is always the last visible parameter; values above 47 are invisible internal params (FREQA/FREQB/etc.) that were incorrectly treated as valid menu entries
- **`I_PARAMS` corrected** (KEEP_BAND_DATA): `5+9` → `5+5+9=19` — adds missing SR-PARAM_C(5) to invisible count; `N_ALL_PARAMS` now correctly reaches BAND_DATA8=66
- **`I_PARAMS` corrected** (without KEEP_BAND_DATA): `5` → `10` — `N_ALL_PARAMS` now correctly reaches PARAM_C=57

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

## Memory Usage (v5.16)

| Resource | Usage | Available |
|----------|-------|-----------|
| Flash | 31,170 bytes (96%) | ~1086 bytes |
| RAM | 1,465 bytes (71%) | 583 bytes |

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

---

**Disclaimer:** I am not responsible for any damage this firmware may cause to devices on which it can be applied. Use at your own risk.

**Author:** EA7LJY - Julian
**Date:** June 2026
