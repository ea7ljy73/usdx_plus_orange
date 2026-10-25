# AGENTS.md - uSDX Plus Orange (firmware modular)

Guía para agentes que trabajen en este proyecto, el firmware modular
uSDX Plus Orange. El objetivo es **paridad funcional estricta** con
el firmware de referencia `usdx-legazy/` (verificado en hardware) ANTES de
reintroducir mejoras.

## Layout del proyecto (no confundir)

| Ruta | Qué es | Reglas |
|---|---|---|
| Raíz (`usdx_plus_orange.ino` + `.h`) | **Producción**: firmware definitivo v2.2.0, con CAT TS-480 ampliado | NO tocar sin tests en verde; ver `## Producción` |
| `usdx-lab/` | **Laboratorio**: copia + instrumentación (`eng_lab.h`), SIN CAT | Sketch autocontenido; ver `## Laboratorio` |
| `usdx-legazy/` | Referencia de solo lectura (fuente de verdad de paridad) | Nunca se modifica ni se elimina |
| `tests/` + `tools/` | Harnesses host y control de tamaño | Compartidos; el lab no los duplica |
| `doc/` | Documentos históricos (plan refactor, auditoría paridad); local, no se publica | Solo consulta; no guían el trabajo actual |

Eliminado y **no recrear**: `usdx_plus_orange_v2/`, `usdxWHITEBUTTONS/`,
`usdx_plus_orange_ft8_menu/`, `src/`, `top.png`, `twotone.png`
(la fusión a raíz ya está hecha).

## Mapa de ficheros

```text
usdx_plus_orange/                  <- RAIZ = producción (sketch Arduino)
├── usdx_plus_orange.ino           <- setup()/loop(), menú (35 entradas), UI
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
├── CAT.md                         <- referencia CAT TS-480: comandos, tabla EX, implementación, pruebas
├── HARDWARE_TEST_GUIDE.md         <- flasheo + checks hardware (producción)
├── GUIA_ESTACION_PC.md            <- fonía SSB desde el PC: CAT, audio, EasyEffects, SDR++ (sin probar)
├── RELEASE.md / RELEASE_ES.md     <- notas de release v2.x (EN/ES)
├── block.png / usdx.png           <- imágenes usadas por los README
├── tests/                         <- harnesses host (ver abajo)
├── tools/size_report.py           <- coste flash/RAM por build (+ size_log.csv)
├── usdx-legazy/                   <- referencia solo lectura (NO tocar; no se publica)
├── usdx-lab/                      <- laboratorio (ver ## Laboratorio)
└── doc/                           <- históricos solo consulta (local, no se publica)
```

```text
usdx-lab/                          <- SKETCH LAB autocontenido (NO producción)
├── usdx-lab.ino                   <- copia de producción SIN cat.h + Engineer
├── eng_lab.h                      <- monitor ADC/audio (pantallas LCD)
├── *.h                            <- copias de producción (ver README_LAB)
├── README_LAB.md                  <- qué es, build, regla de sincronía
├── GUIA_TEST_HW.md / .html        <- pruebas hardware paso a paso (+web)
└── uSDX_Plus_Orange_V2_Instrumentacion_DSP.md  <- teoría DSP (local, no se publica)
```

```text
tests/parity_tx/  gen+main TX (ssb)      tests/parity_rx/  gen+main RX (DSP)
tests/ab/         A/B IMD dos tonos      tests/ab_agc/     A/B AGC + regresión
tests/menu_sim/   menú real (prod/lab)   tests/fw_sim/     CAT, CW, UI, SI5351, TX
tests/avr_sim/    .elf real en simavr    tests/run_all.sh  todo lo anterior
tools/            size_report.py + size_log.csv
```

## Laboratorio (`usdx-lab/`)

- Sketch propio (`usdx-lab/usdx-lab.ino`): se compila desde su carpeta
  (`cd usdx-lab && arduino-cli compile --fqbn arduino:avr:uno .`).
- Es una **copia** de producción con `eng_lab.h` (monitor ADC/audio) y sin
  `cat.h` (la serie queda libre). Versión `v2.2.0-lab` y `F_VER_ID` propio:
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
- Las mejoras se reintroducen UNA A UNA, tras flag de menú y verificadas con
  el test de paridad (estado de cada una en `ROADMAP.md`).

## Producción

Este es el firmware definitivo de producción (`FIRMWARE_VERSION` en
`usdx_settings.h`, visible en el arranque). Reglas:

- El contrato de paridad sigue vigente: ningún cambio funcional entra sin
  su test host en verde (`parity_tx`, `parity_rx`, `ab_agc`).
- Presupuesto: flash al 99 % (~290 B libres tras el CAT ampliado), RAM al 53 %. Todo cambio
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
# Build actual: 31964B (99%) flash, 1097B (53%) RAM, 0 warnings (2026-10-25)
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
| RX NR ≥2 | Divergencia **intencional** (puerta relativa al suelo de ruido; medida en `tests/avr_sim/rx_mejoras.py`) |
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

### Simulaciones host del firmware real (2026-10-13)

```bash
./tests/run_all.sh            # TODO: paridad TX/RX, A/B AGC, menú, fw_sim, sincronía
./tests/run_all.sh --build    # + compila producción y lab
```

| Test | Qué ejecuta | Qué garantiza |
|---|---|---|
| `tests/fw_sim/cat_sim` | `cat.h` real + la `MENU[]` real extraída del `.ino` (`gen_cat_menu.py`) | Respuestas TS-480 como legacy; **FA/MD/RIT nunca tocan SI5351/LPF en TX** (ni en las pausas de la voz, con `tx`=0); comandos ampliados AG/SM/GT/RA/NR/RL/PC/MG/KS/VX/PR/RT/FB/FR/FT/RU/RD/EX: escalas, rangos, `?;` sin efecto, persistencia en EEPROM, rechazo en TX de lo que toca RF |
| `tests/fw_sim/cw_sim` | `cw.h` real (prod y lab) | Keyer A/B (1:3, huecos, swap), mensajes CW, la paleta cancela el CQ; **decoder idéntico a legacy paso a paso** (682k pasos, con QRN y jitter) |
| `tests/fw_sim/ui_sim` | `display_tick()`/`on_rit()`/`on_vfosel()` extraídas del `.ino` | Texto CW con offset legacy sin pisar el medidor; con `smode` 0 (CAT) no escribe en la LCD; RIT normalizado; VFO A/B desde el menú sin pisar el otro VFO; invariantes de `setup()`/`loop()` (`rit=0`, `vox=0`, sin LCD ni sintonía en TX) |
| `tests/fw_sim/si5351_sim` | `si5351.h` real vs clase SI5351 de legacy, sobre un modelo de registros del chip | `freq()` y `freq_calc_fast` bit-exactos; mismo tráfico I2C que legacy (bulk completo); recuperación de una escritura perdida en 1 muestra |
| `tests/fw_sim/txamp_sim` | `ssb()` de `tx.h` vs la de legacy, **fase y amplitud** con `tx=1` y `MAX_DP` | Sin saturar: bit-exacto con legacy. Saturando (ALC de pico F3.8): IMD3 ≤ −24 dBc, PEP completa, ataque ≤ 30 ms, vuelta a paridad ≤ 8 s |
| `tests/fw_sim/rxtx_sim` | `switch_rxtx()` de `hw.h` vs la de legacy | 76 800 conmutaciones: misma secuencia de hardware y estado final; orden PA/receptor/oscilador, rampa anti-click, Practice, Semi-QSK |
| `tests/fw_sim/txtiming_sim` | `ssb()` + SI5351 reales con la temporización del I2C | Espectro de TX (IMD3/5, banda opuesta, adyacente, lejanos) no peor que con la temporización de legacy (≤ 1 dB) |
| `tests/menu_sim/run_menu.sh prod\|lab` | `menu.h` real | Motor de botones: rebotes, DC solo en MAIN, PT volumen solo en MAIN, wrap |
| `tests/check_sync.py` | — | Módulos compartidos idénticos en producción y lab; `rx.h` solo difiere en taps `eng_*` |

### Radio completa en simulador (`tests/avr_sim`, 2026-10-13)

```bash
./tests/avr_sim/run_avr_sim.sh [--legacy]   # o ./tests/run_all.sh --avr
```

Ejecuta el **`.elf` real** en un ATmega328P simulado (simavr, 20 MHz, ciclo a
ciclo) con SI5351 (I2C bit-bang decodificado), relés LPF, LCD, micro por ADC y
PTT. Mide cada interrupción `TIMER2_COMPA` (periodo, duración, anidamiento,
muestras perdidas, perfil por función) y reconstruye la emisión SSB para sacar
IMD3/IMD5, banda opuesta y espurios. Casos: dos tonos, peor caso de CPU (habla
sintética fuerte `--speech` + TX Comp 3 + low-cut, variables fijadas con
`--poke`) y lab; `--tone f a` (un tono) y la línea `MEDIDA` sirven para
barrer mejoras de TX; `--legacy`
pasa también `usdx-legazy` (sin DIAG) como referencia. simavr se descarga sin
root a `~/.cache/usdx-simavr` si no está instalado.

Con `--study` añade el conformador de envolvente del esquema (`usdx.png`:
KEY_OUT → 10k/10n → 10k/10n → puertas BS170) con el PWM real, y barre el
alineamiento amplitud/fase. De ahí sale **TX EnvPre** (F3.10, menú, 0 = legacy).

Escenarios: emisión SSB (por defecto), `--scenario rx` (señal conocida en I/Q →
audio en OC1A: rechazo de banda lateral, AGC, respuesta, IMD, medidor S leído
del LCD) y `--scenario cw` (botón de modo real + llave: rampas, clics, sidetone).
El ADC es un modelo propio fiel al chip (el de simavr no vale para I/Q alternos).
`--scenario cat` (2026-10-25) manda tramas CAT por la **UART real** a 38400
baudios, byte a byte al ritmo de la línea (`--cat "CMD;=respuesta@var=valor|..."`,
`--cat-step`), y comprueba respuestas, RAM y que la ISR de RX no pierde muestras;
con `--ee-save`/`--ee-load` verifica que lo cambiado por CAT sobrevive al reinicio.
Casos en `run_avr_sim.sh`: guion completo (65 pasos), sondeo tipo Hamlib a 120 y
50 ms, y persistencia.

Encontró lo que ningún test host podía ver: medidor S con `__builtin_clz` de
16 bit (marcaba +60..+100 dB de más), `F_CPU` sin redefinir (timers a
78125/6010 Hz en vez de 62500/4808), ISR de TX al 98 % y desbordada con TX Comp.

Límite: son simulaciones de lógica. No miden RF ni audio reales (IMD, espurios,
tiempos I2C): eso sigue siendo la validación en hardware.

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
16. **VFO A/B en el menú** (2026-10-25): `on_vfosel()` volvía a conmutar tras
    escribir el menú `vfosel` y pisaba el otro VFO; ahora como legacy 5585.

### CAT ampliado (2026-10-25, divergencias intencionales)

Sobre el subconjunto legacy, comandos TS-480 estándar mapeados a `MENU[]`
(`CAT_MAP` en `cat.h`) + `EX` genérico (nº de menú de la LCD). Diferencias con
legacy: `AG0;` lee el volumen (legacy: eco), `RT0/1` y `VX0/1` actúan (legacy:
eco), `RC;` reaplica el SI5351, `IF` lleva offset/estado del RIT, TX/RX y VFO,
las escrituras no responden y se rechazan con `?;`. TX real por `OCR2A` (no por
`tx`, que es el latch de portadora). Con `smode` 0 la LCD no se refresca en
reposo (perdía bytes de CAT, legacy no refrescaba). Referencia completa
(comandos, tabla `EX`, implementación y pruebas): `CAT.md`. Quedaron fuera por flash: `KY`, `BU/BD`, `UP/DN`, `FW`.

> Build actual: 31964B (99%) flash, 1097B (53%) RAM, 0 warnings
> (ver log de medidas en ROADMAP.md). Pendiente: validación en hardware.

## Convenciones de código

- Estilo del proyecto: 2 espacios, K&R, `snake_case`,
  tipos `<stdint.h>`, `F("...")` para strings en flash, PROGMEM + `pgm_read_*`.
- ATMEGA328P: 32KB flash / 2KB RAM. Sin `malloc`. Preferir shifts a división.
- Strings de tabla de menú: **PROGMEM** + `pgm_read_ptr`.
- Sin `#ifdef` de hardware en el código: config centralizada en
  `usdx_settings.h` (modelo WHITE_BUTTONS por defecto).

## UX del menú (contrato, a sangre)

Solo el botón de menú entra al menú. Dentro, menú y dial hacen lo mismo:
en navegación el dial cambia de opción; una pulsación entra a edición;
girando se modifica; pulsando se deja de editar, graba y vuelve a la
opción (browser). **Solo menú saca a frecuencia** (en edición guarda y
vuelve a la opción; otra pulsación sale). El dial nunca sale. No cambiar
estas salidas sin pedirlo explícitamente (paridad legacy + UX validada
en banco; ver `tests/menu_sim/`).

## Estructura de módulos

| Módulo | Responsabilidad |
|---|---|
| `usdx_plus_orange.ino` | setup()/loop() + menú + UI fina |
| `usdx_settings.h` | SOLO configuración (modelo + features) |
| `hw.h` | Pins, ADC, timers, ISRs, switch_rxtx |
| `rx.h` | CIC, Hilbert, demod, AGC, NR, slow_dsp, process |
| `tx.h` | ssb(), dsp_tx_*_, LUT, VOX |
| `cw.h` | Keyer Iambic A/B, decoder, tonos |
| `cat.h` | CAT TS-480 (referencia en `CAT.md`) |
| `vfo.h` | VFO A/B, band memory (EEPROM) |
| `menu.h` | Menú declarativo (tabla PROGMEM + callbacks) |
| `display.h` | LCD HD44780 + encoder PCINT + CGRAM |
| `i2c.h` / `si5351.h` / `lpf.h` | I2C bit-bang, SI5351, LPF REV3 |
| `usdx_filter.h` | Filtros IIR SSB/CW |
| `tests/` | Harnesses host (`parity_tx`, `parity_rx`, `ab`, `ab_agc`, `menu_sim`, `fw_sim`) y radio completa en simavr (`avr_sim`); todo con `run_all.sh` |
| `tools/` | `size_report.py` + `size_log.csv` (flash/RAM por build) |