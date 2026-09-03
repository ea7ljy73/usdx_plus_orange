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
