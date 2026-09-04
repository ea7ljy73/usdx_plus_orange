# AGENTS.md - uSDX Plus Orange v2 (refactor modular)

Guía para agentes que trabajen en `usdx_plus_orange_v2/`, el refactor modular
del firmware uSDX Plus Orange. El objetivo es **paridad funcional estricta** con
el firmware de referencia `usdx-legazy/` (verificado en hardware) ANTES de
reintroducir mejoras.

## Contrato de paridad

- `usdx-legazy/` es la **única fuente de verdad**. NO se modifica.
- El v2 debe producir la **misma salida** que legacy para la configuración
  activa (WHITE_BUTTONS). Toda divergencia debe estar documentada y justificada.
- Las mejoras de "gama alta" (CESSB, compresor, EQ, AGC hang+noise floor) están
  **diferidas**: reintroducir UNA A UNA, verificadas con el test de paridad.

## Build

```bash
cd usdx_plus_orange_v2
arduino-cli compile --fqbn arduino:avr:uno .
# Build actual: 26938B (83%) flash, 1162B (56%) RAM, 0 warnings
```

## Tests de paridad (host, sin hardware)

Los harnesses generan dos TU (legacy y v2), alimentan la misma señal y comparan
salida muestra a muestra.

### TX (ssb df)

```bash
cd usdx_plus_orange_v2/tests/parity_tx
python3 gen_parity_tx.py            # extrae ssb() de usdx-legazy.ino y tx.h
gcc -O0 -o parity_tx main_test_tx.c tx_legacy.c tx_v2.c -lm
./parity_tx                         # esperado: 0 mismatches, "TX PARIDAD EXACTA"
```

### RX (cadena DSP completa: CIC + Hilbert + demod + AGC + NR + filtro)

```bash
cd usdx_plus_orange_v2/tests/parity_rx
python3 gen_parity_rx.py            # extrae process_agc_fast/agc/nr, slow_dsp,
                                    # process, sdr_rx_00..07, common_i/q, filt_var
gcc -O0 -o parity_rx main_test_rx.c rx_legacy.c rx_v2.c -lm
./parity_rx                         # cada test corre en un fork (estado DSP aislado)
```

### Resultado verificado (2026-09-03)

| Test | Resultado |
|---|---|
| TX `ssb()` | **PARIDAD EXACTA** (0/2000 mismatches, RMS/peak idénticos) |
| RX core USB/LSB/CW/AM/FM (agc=0, filt=0) | **0 mismatches** (CIC+Hilbert+demod idénticos) |
| RX att2 / volume / NR | **0 mismatches** |
| RX filt=1/2/3 (SSB) | **0 mismatches** |
| RX filt=4..7 (CW) | **0 mismatches** |
| RX agc=1 (AGC ON) | **0 mismatches** |

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

> Pendiente: validación en hardware. Build: 26944B (83%) flash, 1603B (78%) RAM,
> 0 warnings.

## Convenciones de código

- Mismo estilo que la raíz (AGENTS.md): 2 espacios, K&R, `snake_case`,
  tipos `<stdint.h>`, `F("...")` para strings en flash, PROGMEM + `pgm_read_*`.
- ATMEGA328P: 32KB flash / 2KB RAM. Sin `malloc`. Preferir shifts a división.
- Strings de tabla de menú: **PROGMEM** + `pgm_read_ptr` (ver AGENTS.md raíz,
  sección PROGMEM).
- Sin `#ifdef` de hardware en el código: config centralizada en
  `usdx_settings.h` (modelo WHITE_BUTTONS por defecto).

## Estructura de módulos v2

| Módulo | Responsabilidad |
|---|---|
| `usdx_plus_orange_v2.ino` | setup()/loop() + menú + UI fina |
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