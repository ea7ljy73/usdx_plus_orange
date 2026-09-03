# AUDITORÍA DE PARIDAD — v2 vs usdx-legazy (opción por opción, punto por punto)

Estado: EN REVISIÓN COMPLETA. Cada punto clasificado: ✅ ya hecho, 🔴 falta/incorrecto.

---

## A. MENÚ — opción por opción (31 parámetros)

La tabla v2 MENU[] está verificada (31/31 editan/clampan). Verificar CONTRA legacy:

| # | Parámetro v2 | Legacy id | OK? | Nota |
|---|---|---|---|---|
| 0 | Vol | VOLUME | ✅ | |
| 1 | Mode | MODE | ✅ | on_mode aplica filt/stepsize |
| 2 | FilterBW | FILTER | ✅ | |
| 3 | Band | BAND | ⚠️ | on_band cambia freq pero **no set_lpf** (añadido) |
| 4 | Tune Rate | STEP | ✅ | |
| 5 | VFO Mode | VFOSEL | ✅ | |
| 6 | RIT | RIT | ❌ | v2 usa `rit_on` (flag) pero **no implementa offset RIT real** (legacy rit) |
| 7 | AGC | AGC | ✅ | |
| 8 | NR | NR | ✅ | pero v2 force nr=0 en boot |
| 9 | ATT | ATT | ✅ | |
| 10 | ATT2 | ATT2 | ✅ | |
| 11 | S-Meter | SMETER | ⚠️ | v2 S aprox (no log10 del legacy) |
| 12 | AGC Dcy | AGC_DECAY | ✅ | |
| 13 | Noise Blk | NB | ✅ | |
| 14 | CW Decoder | CWDEC | ✅ | |
| 15 | Semi QSK | SEMIQSK | ⚠️ | `semi_qsk` en menú pero **no implementa timeout QSK** en loop |
| 16 | Practice | practice | ✅ | |
| 17 | VOX | VOX | ✅ | pero force off en boot |
| 18 | Noise Gate | VOXGAIN | ✅ | |
| 19 | TX Drive | DRIVE | ✅ | |
| 20 | TX Comp | COMP_EN | ⚠️ | `comp_enable` declarado pero ssb() ya no lo usa (paridad estricta) |
| 21 | TX Emph | PRE_EMPH | ⚠️ | igual, declarado no usado |
| 22 | TX Delay | TXDELAY | ⚠️ | usado para delay TX¿? |
| 23 | EQ Bass | EQ_BASS | ⚠️ | declarado no usado |
| 24 | EQ Treble | EQ_TREBLE | ⚠️ | declarado no usado |
| 25 | TX LoCut | TX_LOWCUT | ⚠️ | declarado no usado |
| 26 | PA bias min | PWM_MIN | ✅ | on_pwm |
| 27 | PA max | PWM_MAX | ✅ | on_pwm |
| 28 | Ref frq | SIFXTAL | ⚠️ | eslot 0 (no persiste) — legacy persiste |
| 29 | IQ phase | IQ_ADJ | ✅ | |
| 30 | Light | BACKL | ✅ | |

**Áreas de acción menú:**
- 🔴 RIT / Semi-QSK / VOX: implementar comportamiento real (no solo toggles).
- ⚠️ Params TX (comp/EQ/pre/lowcut): el ssb() está en paridad estricta (sin uso); el menú los expone pero no hacen nada = **inconsistente** (menú muestra Comp ON/OFF sin efecto). Decidir: quitar del menú o reintroducir uso.

---

## B. HARDWARE / PERIFÉRICOS que faltan

| Punto | Legacy | v2 | Estado |
|---|---|---|---|
| LPF band switching | `set_lpf` + IOExpander16 | lpf.h añadido + set_lpf en do_tune/on_band | ✅ nuevo |
| timer2_stop | sí | no | 🔴 falta (no hay función para detener ISR RX) |
| expander en init | `ioext.init()` (prev_lpf_io=0xff) | prev_lpf_io=0xff → set_lpatch llama init | ✅ |
| build_lut | función propia | `on_pwm()` | ⚠️ equivalent pero no idéntico |
| smeter() completo | dbm+log10+att2 | aprox `_absavg256>>10` | 🔴 S no calibrado |

---

## C. CAT — comandos que faltan en v2

El v2 tiene 16 comandos; el legacy 24. **Faltan (pendientes):**
- 🔴 `Command_AI0` (AI off)
- 🔴 `Command_PS1` (PS variant)
- 🔴 `Command_TX0/TX1/TX2` (TX states)
- 🔴 `Command_UA` (ATU/Ant)
- 🔴 `Command_UD` (up/down)
- 🔴 `Command_UK` (lock?)
- 🔴 `Command_VX` (VFO swap?)

Verificar cuáles usa software real (Hamlib/FLDigi) y añadir los necesarios.

---

## D. Display / UI

| Punto | Legacy | v2 | Estado |
|---|---|---|---|
| Frecuencia línea 1 | ✅ | ✅ | |
| VFO indicator | CGRAM `\x06` | CGRAM `\x06/\x07` | ✅ 55b5785 |
| Cursor stepsize | ✅ | ✅ | |
| S-meter | dígitos dbm/S con log10 | aprox | 🔴 |
| `show_banner` | uSDX + logo | uSDX + logo CGRAM | ✅ final |
| `stepsize_change` | rotar stepsize con botón | no | 🔴 falta |
| CW decoder en línea 0 | muestra en RX CW | muestra en linea 0 | ✅ final |

---

## E. DSP / audio pendientes por verificar

| Punto | Estado |
|---|---|
| Audio RX suena | ✅ (verificado) |
| S-meter no calibrado | 🔴 |
| params TX (comp/EQ) sin efecto | 🔴 inconsistencia de menú |
| RIT offset sin efecto | 🔴 |
| semi_qsk timeout sin efecto | 🔴 |

---

## F2. CONFIRMACIONES (verificadas con hechos)

- RIT en v2: ✅ corregido - menu toggle sobre rit, do_tune ajusta offset,
  RX aplica freq_calc_fast(rit) (legacy 3849/5712)
- semi_qsk en v2: ✅ corregido - switch_rxtx setea semi_qsk_timeout y usa dummy;
  loop hace switch_rxtx(0) al expirar (legacy 3686/5292)
- Params TX (comp/eq/pre/lowcut): ✅ QUITADOS del menu (copia exacta legazy)
  - el menu v2 ahora tiene los 27 parametros de usdx-legazy
- CAT: faltan AI0/PS1/TX0/TX1/TX2/UA/UD/UK/VX (10). 
- smeter: ✅ calibrado (log10) + smode 1/2/3(S-bar)/4(wpm)
- stepsize_change: ✅ implementado (encoder push)
- timer2_stop/timer1_stop: ✅ implementados

## F. Mi priorización propuesta (orden de trabajo tras la auditoría)

1. RIT real + semi_qsk timeout (parámetros de menú que hoy no hacen nada)
2. Cuadrar menú: parámetros TX (comp/EQ) — o quitar los que no aplican, o reincorpores a ssb
3. Comandos CAT faltantes (los que usa Hamlib)
4. S-meter calibrado (smeter legacy)
5. Mostrar_ banner + stepsize_change

> NOTA: el objetivo es PARIDAD FUNCIONAL. No añadir features nuevos.

---

## Estado verificado hasta ahora (commits)
- ✅ ssb TX paridad exacta
- ✅ RX DSP equivalente
- ✅ Menú 31 params, transiciones BL/BR, display
- ✅ LPF band switching (05cddf1)
- ✅ Keyer Iambic A/B init (05cddf1)
- 🔴 Pendientes de esta auditoría listados arriba
---
## RESULTADOS DE TESTS DE PARIDAD (verificados con harness host, 2026-09-03)

Harnesses en `usdx_plus_orange_v2/tests/{parity_tx,parity_rx}` (ver AGENTS.md v2).

### TX `ssb()` — PARIDAD EXACTA
0/2000 mismatches. sum|df| 1694248 = 1694248, RMS 1919.1 = 1919.1, peak 4800 = 4800.

### RX DSP (cadena completa: CIC + Hilbert + demod + AGC + NR + filtros)
**TODOS los tests: 0 mismatches (100% paridad exacta).**

| Config | mismatch |
|---|---|
| USB/LSB/CW/AM/FM core (agc=0, filt=0) | 0.00% |
| att2=0, volume=16 | 0.00% |
| NR (nr=3, nr=8) | 0.00% |
| filt=1/2/3 (SSB 2900/2400/1800) | 0.00% |
| filt=4/7 (CW 600/18) | 0.00% |
| agc=1 (AGC ON) | 0.00% |

### Divergencias CORREGIDAS (2026-09-03) para lograr paridad total
1. ✅ AGC: `slow_dsp` agc==1 → `process_agc_fast` (legacy, sin FAST_AGC).
2. ✅ Filtros SSB 2/3: 2ª biquad `>>1`→`>>2`; CW: términos `>>6/>>2/>>4/>>5`,
   `return zc0>>3` (ganancia legacy).
3. ✅ `process_minsky`/`dsp_tx_cw`: tono CW 600Hz legacy (int8, `798/127`),
   OCR1AL `>> (16-volume)`.
4. ✅ `CW_OFFSET` runtime (`cw_offset = tones[cw_tone]`, legacy 5079); aplicado
   en TX CW y en RX vía `vfo_hw_apply` (fase IQ por modo).
5. ✅ `si5351.powerDown()` reg 24 = `0b00010000`.
6. ✅ `switch_rxtx` reescrito como port fiel de legacy: `_centiGain` backup/
   restore, `semi_qsk_timeout=0` en TX, `ADMUX=admux[2]` TX / `_init=1` RX,
   indicadores 'D'/'P'/'T'/'V'/'R', gating semi-QSK de relés, `_SERIAL` PC2,
   `interrupts()`, OCR2A+TIMSK2 al final.
7. ✅ `timer2_start`: TIMSK2 antes de OCR2A (orden legacy).
8. ✅ `analogSampleMic`: toggles RX con `vox_thresh>=32`; `analogSafeRead`:
   algoritmo activo legacy (prescaler 128 + settle).
9. ✅ VOX: solo LSB/USB + `delay(32)` al volver a RX.
10. ✅ CW decoder: portada etapa audio legacy (`_amp32`/EA/noise-blanker →
    `filteredstate` → `dec2()` OLD_CW + `printsym` CW_INTERMEDIATE).
11. ✅ CW_MESSAGE: `cw_tx`, `delayWithKeySense`, CQ Interval/CQ Message en menú.
12. ✅ CAT: `IF` frame 38 chars (falta `0000000`), `SETFreqA`/`SetMD` vía
    `vfo_apply` (fase), `RX;` semi_qsk_timeout, respuesta `?;`.
13. ✅ Menú: motor de botones completo de legacy (SC/DC/PL/PT), ciclo de modo
    LSB→USB→CW con backup/restore stepsize/filt, filtro por doble click, banda
    por BE|DC, RIT+VFO swap por BR|PL, volumen/powerDown por BE|PT; orden de
    parámetros = legacy (28, CQ Interval/Message, sin TX Delay); edición de
    string (CQ Message); wrap en edición; navegación con clamp (sin wrap).
14. ✅ Defaults: `drive=4`, `pwm_max=128`, `pwm_min=0`.
15. ✅ EEPROM: eslots únicos (sin colisión 15 ni 32), reset loop `MENU_COUNT`.
16. ✅ Polaridad de botón: `inv=0` (pulsado=HIGH, legacy 162) en setup y menú.
17. ✅ CAT baud 38400 (legacy, sin CAT_STREAMING).
18. ✅ `powerDown()` portado (sleep + wake por pin-change).
19. ✅ `do_tune`: clamp freq legacy (1..999999999), alineación bandval, RIT
    aplicado en tiempo real.

> Objetivo: PARIDAD FUNCIONAL. Test de paridad TX/RX al 100%. Pendiente de
> validación en hardware (2026-09-03).

---
## H. COMPARACIÓN VARIABLE A VARIABLE (legacy activo vs v2)

Config activa legacy: `KEYER, CAT, CW_DECODER, TX_ENABLE, KEY_CLICK, SEMI_QSK,
RIT_ENABLE, VOX_ENABLE, CW_MESSAGE, CW_INTERMEDIATE, REV3 LPF`, F_MCU=20M,
F_XTAL=27M. INACTIVOS: DIAG(note), PTX, NTX, TX_DELAY, FAST_AGC, MOX, OLED,
LCD_I2C, QCX, CAT_EXT, CAT_STREAMING, SWR_METER, VSS_METER, CLOCK, etc.

| Variable | Legacy | v2 | Estado |
|---|---|---|---|
| mode | USB | USB | ✅ |
| volume | 12 | 12 | ✅ |
| agc | 1 | 1 | ✅ |
| nr | 0 | 0 | ✅ |
| att | 0 | 0 | ✅ |
| att2 | 2 | 2 | ✅ |
| filt | 0 | 0 | ✅ |
| bandval | 3 | 3 | ✅ |
| stepsize | STEP_1k=5 | 5 | ✅ |
| vfosel | VFOA=0 | 0 | ✅ |
| rit | 0 | 0 | ✅ |
| smode | 1 | 1 | ✅ |
| cwdec | 1 | 1 | ✅ |
| semi_qsk | 0 | 0 | ✅ |
| keyer_speed | 25 | 25 | ✅ |
| keyer_mode | 2 (SINGLE) | 2 | ✅ |
| keyer_swap | 0 | 0 | ✅ |
| practice | 0 | 0 | ✅ |
| vox | 0 | 0 | ✅ |
| vox_thresh | (1<<2)=4 | 4 | ✅ |
| drive | 2 (setup→4) | 4 (setup→4) | ✅ |
| txdelay | 0 | 0 | ✅ |
| pwm_min | 0 | 0 | ✅ |
| pwm_max | 128 (no-QCX) | 128 | ✅ |
| rx_ph_q | 90 | 90 | ✅ |
| backlight | 8 | 8 | ✅ |
| freq | 14000000 | 14000000 | ✅ (corregido) |
| vfo[] | {7074000,14074000} | {7074000,14074000} | ✅ |
| vfomode[] | {USB,USB} | {USB,USB} | ✅ |
| tx | 0 | 0 | ✅ |
| quad | 0 | 0 | ✅ |
| cw_tone | 1 | 1 | ✅ |
| p_sin | 0 (int8) | 0 (int8) | ✅ |
| n_cos | 448/4 (int8) | 448/4 (int8) | ✅ |
| cw_offset | tones[cw_tone] | tones[cw_tone] | ✅ |
| tones[] | {700,600,700}·F_MCU/2e7 | igual | ✅ |
| lut[256] | sí | sí | ✅ |
| gain | 1024 | 1024 | ✅ |
| centiGain | 128 | 128 | ✅ |
| decayCount | DECAY_FACTOR=400 | 400 | ✅ |
| _init | 0 | 0 | ✅ |
| _centiGain | 0 | 0 | ✅ (corregido) |
| i, q, ocomb, qh | 0 | 0 | ✅ |
| absavg256/_absavg256 | 0 | 0 | ✅ |
| amp32/_amp32 | 0 | 0 | ✅ |
| avg | 256 | 256 | ✅ |
| sym | 0 (uninit) | 0 | ✅ (corregido) |
| filteredstate/before | LOW | LOW | ✅ |
| realstate/before | LOW | LOW | ✅ |
| nbtime | 16 | 16 | ✅ |
| wpm | 25 | 25 | ✅ |
| inv | 0 | 0 | ✅ |
| cat_active | 0 | 0 | ✅ |
| cw_msg[1][48] | CW_MSG1 | CW_MSG1 | ✅ |
| cw_msg_interval | 5 | 5 | ✅ |
| cw_msg_event/id | 0 | 0 | ✅ |
| band[] (FT8) | 1840000…50313000 | PROGMEM igual | ✅ |
| stepsizes[10] | 10M…1 | PROGMEM igual | ✅ |
| prev_stepsize | {STEP_1k,STEP_500} | {5,6} | ✅ |
| prev_filt | {0,4} | {0,4} | ✅ |
| PTX | INACTIVO | INACTIVO | ✅ (corregido) |
| TX_DELAY | INACTIVO | INACTIVO | ✅ (corregido) |
| DIAG | ACTIVO | NO PORTADO | ⚠️ divergencia documentada (solo diag de arranque) |
| change flag | presente | vía vfo_apply directo | ✅ equivalente |
| menumode/menu | presente | menu.state | ✅ equivalente |
| rxend_event | presente | no (CAT display lockout) | ⚠️ menor (estético CAT) |
---
## G. CHECKLIST DE DEFAULTS (uno a uno, legazy vs v2)

| Variab | legazy | v2 ANTES | v2 AHORA | Estado |
|---|---|---|---|---|
| volume | 12 | 12 | 12 | ✅ |
| mode | USB(1) | USB | USB | ✅ |
| filt | 0 | 0 | 0 | ✅ |
| bandval | 3 | 3 | 3 | ✅ |
| stepsize | STEP_1k(5) | 3 | 5 | ✅ 60c5905 |
| vfosel | VFOA(0) | 0 | 0 | ✅ |
| rit | 0 | 0 | 0 | ✅ |
| agc | 1 (sin FAST_AGC) | 2 | 1 | ✅ |
| nr | 0 | 2 | 0 | ✅ |
| att | 0 | 0 | 0 | ✅ |
| att2 | 2 | 2 | 2 | ✅ |
| smode | 1 | 1 | 1 | ✅ |
| cwdec | 1 | 1 | 1 | ✅ |
| semi_qsk | 0 | 0 | 0 | ✅ |
| keyer_speed | 25 | 25 | 25 | ✅ |
| keyer_mode | SINGLE(2) | 2 | 2 | ✅ |
| keyer_swap | 0 | 0 | 0 | ✅ |
| practice | 0 | 0 | 0 | ✅ |
| vox | 0 | 0 | 0 | ✅ |
| vox_thresh | 4 | 0(uninit) | 4 | ✅ |
| drive | 2 | 2 | 2 | ✅ |
| txdelay | 0 | 1 | 0 | ✅ |
| pwm_min | 0 | 115 | 0 | ✅ |
| pwm_max | 255 | 220 | 255 | ✅ |
| rx_ph_q | 90 | 90 | 90 | ✅ |
| backlight | 8 | 1 | 8 | ✅ |
| vfo[] | {7074000,14074000} | - | añadido | ✅ |
| vfomode[] | {USB,USB} | - | añadido | ✅ |
| freq inicial | 14000000 | 0||bandval | bandval recall | ✅ via vfo_recall |

### Ajustes de comportamiento (paridad)
- on_mode: prev_stepsize[]/prev_filt[] por modo (SSB/CW) ✅
- on_vfosel: VFO A/B toggle real (guarda/carga freq+mode) ✅
