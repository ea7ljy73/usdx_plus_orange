# uSDX Plus Orange — Roadmap

Mejoras priorizadas para TX/RX, optimizaciones de código y corrección de bugs,
ordenadas por impacto y esfuerzo.

| #  | Mejora                          | Tipo     | Esfuerzo | Impacto | Estado |
|----|---------------------------------|----------|----------|---------|--------|
| 1  | Fix AGC overflow                | Bugfix   | 10 min   | ⭐⭐⭐   | ✅     |
| 2  | Tablas de strings a PROGMEM     | RAM      | 1h       | ⭐⭐⭐   | ✅     |
| 3  | Eliminar código muerto          | Flash    | 30 min   | ⭐⭐     | ✅     |
| 4  | Reemplazar divisiones por shift | CPU/RAM  | 15 min   | ⭐⭐     | ✅     |
| 5  | CESSB etapa 1 (clipper)         | TX       | 2-3h     | ⭐⭐⭐   | ✅     |
| 6  | AGC hang timer + noise floor    | RX       | 2-3h     | ⭐⭐⭐   | ✅     |
| 7  | LMS Auto-Notch                  | RX       | 1-2h     | ⭐⭐     | ✅     |
| 8  | Phase unwrapping                | TX       | 1h       | ⭐⭐     | ✅     |
| 9  | float a fixed-point             | CPU      | 1h       | ⭐       | ✅     |

## Extra optimizations (v5.18)

| #  | Mejora                          | Tipo     | Impacto | Estado |
|----|---------------------------------|----------|---------|--------|
| 10 | Eliminar float de smeter()      | Flash    | ⭐⭐⭐   | ✅     |
| 11 | AM-PM predistortion 64 entradas | TX       | ⭐⭐⭐   | ✅     |
| 12 | AGC decay rápido en CW          | RX       | ⭐⭐     | ✅     |
| 13 | cap_label a PROGMEM             | RAM      | ⭐⭐     | ✅     |
| 14 | DIAG fixed-point                | Flash    | ⭐⭐     | ✅     |
| 15 | FWD/SWR uint16_t + ref_V dead   | RAM      | ⭐       | ✅     |
| 16 | CESSB scope fix (const var)     | Bugfix   | ⭐       | ✅     |
| 17 | QUAD eliminado                  | TX       | ⭐⭐     | ✅     |
| 18 | Bloque #ifdef x eliminado       | Flash    | ⭐       | ✅     |

## Descripción detallada

### #1 Fix AGC overflow (crítico)
`process_agc()` línea ~2745: `out = (centiGain >> 5) * in` puede overflow `int16_t`
cuando `centiGain >= 128` y `in` es grande. Necesita cast a `int32_t`.

### #2 Tablas de strings a PROGMEM
Las tablas `mode_label[]`, `offon_label[]`, `filt_label[]`, `band_label[]`,
`stepsize_label[]`, `att_label[]`, `smode_label[]`, `swr_label[]`,
`keyer_mode_label[]`, `agc_label[]`, `stepsizes[]` están en RAM (~160+ bytes).
Mover a PROGMEM y usar `pgm_read_ptr()` + `pgm_read_byte()` en `paramAction()`.

### #3 Eliminar código muerto
- `process_nr_old()`, `process_nr_old2()` — no usados por ningún código
- `#ifdef BLIND` — BLIND nunca definido
- `#ifdef TESTBENCH` — NCO functions, TESTBENCH nunca definido
- `#ifdef SIMPLE_RX` — rama RX alternativa, nunca activada
- `#ifdef CLOCK` — reloj en display, CLOCK nunca definido
- `#ifdef VSS_METER` — voltímetro, nunca definido
- `#ifdef REMOVEFONT`, `#ifdef INVERSE` — nunca definidos
- `calibrate_iq()` — solo llamado si QCX definido (nunca)
- `Command_TX0/TX1/TX2` — 3 funciones casi idénticas
- `Command_AI()` / `Command_AI0()` — idénticas
- `Command_PS1()` — función vacía

### #4 Reemplazar /3 por shift en ssb()
En `ssb()` hay dos divisiones por 3: `(in - 2*z1)/3` y `(ac + 2*dc)/3`.
ATCOR: usar multiplicación por 21846 >> 16 (aprox. 1/3 en fixed-point).
También reemplazar `adc / 4` por `adc >> 2`.

### #5 CESSB etapa 1 (envelope clipper)
Controlled Envelope Single Sideband — etapa de recorte de envolvente en I/Q
para eliminar overshoots del SSB. Aumenta potencia efectiva ~2-3dB sin ensanchar
BW. Se implementa limitando la magnitud del vector I/Q antes de pasarlo al
modulador polar.

### #6 AGC con hang timer + noise floor
Mejora del AGC actual:
- Hang timer: la ganancia no sube durante pausas entre palabras SSB
- Noise floor adaptativo: la ganancia no se incrementa si la señal está al nivel
  del piso de ruido

### #7 LMS Auto-Notch
Filtro adaptivo de 2 taps para cancelar tonos estables (heterodinos, birdies).
Bajo coste computacional, muy efectivo en recepción SDR.

### #8 Phase unwrapping
Mejora el manejo de diferencias de fase grandes en el modulador polar para
reducir espurios cuando hay tonos de audio asimétricos.

### #9 float a fixed-point en readSWR/smeter
`readSWR()` y `smeter()` usan `float` para cálculos — extremadamente lento en
AVR (sin FPU). Convertir a fixed-point con enteros de 16/32 bits.
