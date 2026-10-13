# AGENTS.md - uSDX Plus Orange (firmware modular)

Guía para agentes que trabajen en este proyecto, el firmware modular
uSDX Plus Orange. El objetivo es **paridad funcional estricta** con
el firmware de referencia `usdx-legazy/` (verificado en hardware) ANTES de
reintroducir mejoras.

## Layout del proyecto (no confundir)

| Ruta | Qué es | Reglas |
|---|---|---|
| Raíz (`usdx_plus_orange.ino` + `.h`) | **Producción**: firmware definitivo v2.0.0, con CAT | NO tocar sin tests en verde; ver `## Producción` |
| `usdx-lab/` | **Laboratorio**: copia + instrumentación (`eng_lab.h`), SIN CAT | Sketch autocontenido; ver `## Laboratorio` |
| `usdx-legazy/` | Referencia de solo lectura (fuente de verdad de paridad) | Nunca se modifica ni se elimina |
| `tests/` + `tools/` | Harnesses host y control de tamaño | Compartidos; el lab no los duplica |
| `doc/` | Documentos históricos (plan refactor, auditoría paridad) | Solo consulta; no guían el trabajo actual |

Eliminado y **no recrear**: `usdx_plus_orange_v2/`, `usdxWHITEBUTTONS/`,
`usdx_plus_orange_ft8_menu/`, `src/`, `top.png`, `twotone.png`
(la fusión a raíz ya está hecha).

## Mapa de ficheros

```text
usdx_plus_orange/                  <- RAIZ = producción (sketch Arduino)
├── usdx_plus_orange.ino           <- setup()/loop(), menú (34 entradas), UI
├── usdx_settings.h                <- SOLO config (WHITE_BUTTONS); FIRMWARE_VERSION
├── hw.h                           <- pines, ADC, timers, ISRs, switch_rxtx
├── rx.h                           <- CIC, Hilbert, demod, AGC, NR, slow_dsp, process
├── tx.h                           <- ssb(), dsp_tx_*_, LUT, VOX
├── cw.h                           <- keyer Iambic A/B, decoder, tonos
├── cat.h                           <- CAT TS-480 (solo producción)
├── vfo.h / menu.h / display.h     <- VFO A/B+RIT, menú declarativo, LCD+encoder
├── i2c.h / si5351.h / lpf.h       <- I2C bit-bang, SI5351, LPF REV3
├── perf.h                         <- medidor CPU (flag PERF_METER, coste 0 sin él)
├── usdx_filter.h                  <- filtros IIR SSB/CW
├── AGENTS.md / README.md / README_ES.md / ROADMAP.md
├── HARDWARE_TEST_GUIDE.md         <- flasheo + checks hardware (producción)
├── RELEASE.md / RELEASE_ES.md     <- notas de release históricas
├── block.png / usdx.png           <- imágenes usadas por los README
├── tests/                         <- harnesses host (ver abajo)
├── tools/size_report.py           <- coste flash/RAM por build (+ size_log.csv)
├── usdx-legazy/                   <- referencia solo lectura (NO tocar)
├── usdx-lab/                      <- laboratorio (ver ## Laboratorio)
└── doc/                           <- históricos solo consulta
```

```text
usdx-lab/                          <- SKETCH LAB autocontenido (NO producción)
├── usdx-lab.ino                   <- copia de producción SIN cat.h + Engineer
├── eng_lab.h                      <- monitor ADC/audio (pantallas LCD)
├── *.h                            <- copias de producción (ver README_LAB)
├── README_LAB.md                  <- qué es, build, regla de sincronía
├── GUIA_TEST_HW.md / .html        <- pruebas hardware paso a paso (+web)
└── uSDX_Plus_Orange_V2_Instrumentacion_DSP.md  <- teoría DSP (documento base)
```

```text
tests/parity_tx/  gen+main TX (ssb)      tests/parity_rx/  gen+main RX (DSP)
tests/ab/         A/B IMD dos tonos      tests/ab_agc/     A/B AGC + regresión
tools/            size_report.py + size_log.csv
```

## Laboratorio (`usdx-lab/`)

- Sketch propio (`usdx-lab/usdx-lab.ino`): se compila desde su carpeta
  (`cd usdx-lab && arduino-cli compile --fqbn arduino:avr:uno .`).
- Es una **copia** de producción con `eng_lab.h` (monitor ADC/audio) y sin
  `cat.h` (la serie queda libre). Versión `v2.0.0-lab` y `F_VER_ID` propio:
  no comparte EEPROM con producción.
- Regla de sincronía: al cambiar producción, copiar a mano los `.h`
  afectados (salvo `usdx_settings.h`: mantener versión/magia lab).
- El lab **nunca** es release ni radio de uso: solo banco con carga de 50 Ω.
- Detalle: `usdx-lab/README_LAB.md`. Pruebas hardware paso a paso:
  `usdx-lab/GUIA_TEST_HW.md` (+ versión web `GUIA_TEST_HW.html` con
  campos y autoverificación).

## Contrato de paridad

- `usdx-legazy/` es la **única fuente de verdad**. NO se modifica.
- La carpeta `usdx-legazy/` es firmware de referencia de **solo lectura**:
  se conserva para consulta, nunca se modifica ni se elimina.
- El firmware debe producir la **misma salida** que legacy para la configuración
  activa (WHITE_BUTTONS). Toda divergencia debe estar documentada y justificada.
- Las mejoras de "gama alta" (CESSB, compresor, EQ, AGC hang+noise floor) están
  **diferidas**: reintroducir UNA A UNA, verificadas con el test de paridad.

## Producción

Este es el firmware definitivo de producción (`FIRMWARE_VERSION` en
`usdx_settings.h`, visible en el arranque). Reglas:

- El contrato de paridad sigue vigente: ningún cambio funcional entra sin
  su test host en verde (`parity_tx`, `parity_rx`, `ab_agc`).
- Presupuesto: flash al 95 % (~1,4 KB libres), RAM al 58 %. Todo cambio
  declara su coste (`python3 tools/size_report.py --note "..."`).
- Cambios de layout/semántica EEPROM → subir `F_VER_ID` y avisar de reset
  a fábrica (arranque con encoder pulsado).
- Mejoras solo tras flag de menú (apagables), UNA A UNA, con A/B medido;
  sin mejora medible → rollback (ver ROADMAP.md).
- Puerta de release pendiente: validación en hardware según
  `HARDWARE_TEST_GUIDE.md` (hasta entonces, no dar por cerrado ningún cambio
  de comportamiento RF/audio).

## Build

```bash
arduino-cli compile --fqbn arduino:avr:uno .
# Build actual: 30804B (95%) flash, 1189B (58%) RAM, 0 warnings (2026-09-14)
```

## Tests de paridad (host, sin hardware)

Los harnesses generan dos TU (legacy y firmware actual, sufijos `_legacy`/`_v2`
en el código generado), alimentan la misma señal y comparan
salida muestra a muestra.

### TX (ssb df)

```bash
cd tests/parity_tx
python3 gen_parity_tx.py            # extrae ssb() de usdx-legazy.ino y tx.h
gcc -O0 -o parity_tx main_test_tx.c tx_legacy.c tx_v2.c -lm
./parity_tx                         # esperado: 0 mismatches, "TX PARIDAD EXACTA"
```

### RX (cadena DSP completa: CIC + Hilbert + demod + AGC + NR + filtro)

```bash
cd tests/parity_rx
python3 gen_parity_rx.py            # extrae process_agc_fast/agc/nr, slow_dsp,
                                    # process, sdr_rx_00..07, common_i/q, filt_var
gcc -O0 -o parity_rx main_test_rx.c rx_legacy.c rx_v2.c -lm
./parity_rx                         # cada test corre en un fork (estado DSP aislado)
```

### Resultado verificado (2026-09-03; reverificado desde la raíz el 2026-09-15)

| Test | Resultado |
|---|---|
| TX `ssb()` | **PARIDAD EXACTA** (avg\|diff\|=0, RMS/peak idénticos) |
| RX core USB/LSB/CW/AM/FM (agc=0, filt=0) | **0 mismatches** (CIC+Hilbert+demod idénticos) |
| RX att2 / volume / NR 0..1 | **0 mismatches** |
| RX NR ≥2 | Divergencia **intencional** (NR de 2 polos, ver ROADMAP F4) |
| RX filt=1/2/3 (SSB) | **0 mismatches** |
| RX filt=4..7 (CW) | **0 mismatches** |
| RX agc=1 (AGC ON) | **0 mismatches** |
| M0PUB body legacy vs actual (F4.18, funcion suelta) | **0 mismatches** (2852 pasos) |

### A/B AGC (F4.16/F4.17, divergencias intencionales, con asserts)

```bash
cd tests/ab_agc
./run_ab_agc.sh   # cuerpo REAL de process_agc_fast; exit 0 = PASS, 1 = FAIL
```

Valida con asserts: T1 precarga x8 audible en muestra 0 y t90 menor;
T2 rec=4 varianza soplo <1/4, overshoot menor y sin clipping.

## Divergencias CORREGIDAS (2026-09-03) — paridad total alcanzada

1. **AGC**: `slow_dsp` agc==1 → `process_agc_fast` (legacy sin FAST_AGC).
2. **Filtros SSB/CW**: ganancia revertida a legacy (`>>2`, `>>6`, `zc0>>3`).
3. **`process_minsky`/`dsp_tx_cw`**: tono CW 600Hz legacy (int8, coef `798/127`).
4. **CW offset**: runtime `cw_offset = tones[cw_tone]` (TX CW + RX fase IQ).
5. **`si5351.powerDown()`** reg 24 = `0b00010000`.
6. **`switch_rxtx`**: port fiel de legacy (`_centiGain`, semi_qsk, ADMUX mic,
   `_init=1`, indicadores LCD, gating relés, `_SERIAL`, `interrupts()`).
7. **CW decoder**: etapa audio legacy (`_amp32`/EA/noise-blanker) + `dec2`
   OLD_CW + `printsym` CW_INTERMEDIATE.
8. **CW_MESSAGE**: `cw_tx`, `delayWithKeySense`, CQ Interval/CQ Message.
9. **CAT**: IF 38 chars, SETFreqA/SetMD con fase, `RX;`, `?;`, baud 38400.
10. **Menú**: motor botones completo (SC/DC/PL/PT), ciclo modo LSB→USB→CW,
    28 params en orden legacy, edición de string, wrap, clamp de navegación.
11. **Defaults**: drive=4, pwm_max=128, pwm_min=0.
12. **EEPROM**: eslots únicos, reset loop `MENU_COUNT`.
13. **Polaridad botón**: `inv=0` (pulsado=HIGH, legacy 162).
14. **`powerDown()`** portado; **`analogSafeRead`/`analogSampleMic`/VOX** legacy.
15. **`do_tune`**: clamp legacy, alineación bandval, RIT en tiempo real.

> Build actual: 30804B (95%) flash, 1189B (58%) RAM, 0 warnings
> (ver log de medidas en ROADMAP.md). Pendiente: validación en hardware.

## Convenciones de código

- Estilo del proyecto: 2 espacios, K&R, `snake_case`,
  tipos `<stdint.h>`, `F("...")` para strings en flash, PROGMEM + `pgm_read_*`.
- ATMEGA328P: 32KB flash / 2KB RAM. Sin `malloc`. Preferir shifts a división.
- Strings de tabla de menú: **PROGMEM** + `pgm_read_ptr`.
- Sin `#ifdef` de hardware en el código: config centralizada en
  `usdx_settings.h` (modelo WHITE_BUTTONS por defecto).

## Estructura de módulos

| Módulo | Responsabilidad |
|---|---|
| `usdx_plus_orange.ino` | setup()/loop() + menú + UI fina |
| `usdx_settings.h` | SOLO configuración (modelo + features) |
| `hw.h` | Pins, ADC, timers, ISRs, switch_rxtx |
| `rx.h` | CIC, Hilbert, demod, AGC, NR, slow_dsp, process |
| `tx.h` | ssb(), dsp_tx_*_, LUT, VOX |
| `cw.h` | Keyer Iambic A/B, decoder, tonos |
| `cat.h` | CAT TS-480 |
| `vfo.h` | VFO A/B, band memory (EEPROM) |
| `menu.h` | Menú declarativo (tabla PROGMEM + callbacks) |
| `display.h` | LCD HD44780 + encoder PCINT + CGRAM |
| `i2c.h` / `si5351.h` / `lpf.h` | I2C bit-bang, SI5351, LPF REV3 |
| `usdx_filter.h` | Filtros IIR SSB/CW |
| `tests/` | Harnesses host: `parity_tx`, `parity_rx`, `ab`, `ab_agc` |
| `tools/` | `size_report.py` + `size_log.csv` (flash/RAM por build) |