# uSDX Plus Orange - Notas de Release

**Versión:** 6.00
**Base:** uSDX Legacy 1.02x / usdxWHITEBUTTONS v4.00d (GW8RDI)
**Plataforma:** ATMEGA328P @ 20MHz
**Autor:** EA7LJY - Julian

---

## v6.00 - AM/FM Desbloqueados y Soporte para Banda de 11m

**Memoria:** Pendiente

### Cambios Principales

#### Modos AM y FM Completamente Habilitados
- **Selección de modo desbloqueada** — se eliminó `SHOW_USB_LSB_CW_ONLY`; el botón de modo ahora cicla LSB→USB→CW→**FM**→**AM**→LSB (ciclo completo de 5 modos)
- **RX/TX de AM/FM existentes desbloqueados** — la demodulación (RX) y modulación (TX) de AM/FM ya estaban implementadas en el código pero bloqueadas; ahora accesibles desde el menú (1.2 Mode) y el botón derecho
- **VOX extendido** — transmisión operada por voz ahora funciona en AM y FM (antes solo LSB/USB)
- **Sin cambios en filtros, SI5351 ni DSP** — el `magn(i,q)` para AM y el `_arctan3(q,i)`+diferenciador para FM funcionan correctamente con la arquitectura SDR actual

#### Banda de 11m Añadida
- **Nueva banda**: 11m (27.0 MHz / banda CB) insertada entre 12m (24.9 MHz) y 10m (28.0 MHz)
- **Auto-detección**: umbrales de frecuencia divididos en 28 MHz — sintonizar 26-28 MHz selecciona 11m, 28-32 MHz selecciona 10m
- **LPF**: 11m y 10m comparten el mismo relé de LPF (IO1_3, f > 26 MHz) — sin cambios de hardware
- **Etiquetas de banda**: el menú ahora muestra: 80m, 60m, 40m, 30m, 20m, 17m, 15m, 12m, **11m**, **10m**
- **EEPROM**: se añadió BAND_DATA9 (banda 10 → 10m); I_PARAMS actualizado 5+5+9→5+5+10; N_ALL_PARAMS 67→68
- **VERSION incrementada** a "6.00" — fuerza reseteo de EEPROM en el primer arranque

#### Notas de Compatibilidad
- ⚠️ **Reseteo de EEPROM requerido** — el primer arranque tras flashear v6.00 reseteará todos los ajustes a valores de fábrica (mantener pulsado el botón del encoder durante el encendido si el reseteo automático no se activa)
- Todos los modos existentes (LSB, USB, CW) están **100% sin cambios** — ningún filtro, frecuencia o comportamiento DSP ha sido modificado
- El modo por defecto para bandas 1-4 sigue siendo LSB; bandas 5-10 por defecto USB
- 6m (50 MHz) permanece en el índice 11 (bandval=11, excluido del ciclo de bandas del menú)

**Memoria:** 30.760 bytes flash (95%), 1.342 bytes RAM (65%) — −394 bytes flash, −124 bytes RAM respecto a v5.17

### ROADMAP: 9 ítems implementados

Proyecto sistemático de mejora de calidad de código y DSP basado en ROADMAP.md.

#### Cambios

- **Corrección overflow AGC** — cast a `int32_t` evita desbordamiento de multiplicación `int16_t` en señales fuertes; antes causaba saltos de ganancia en niveles altos de audio
- **PROGMEM strings** — 12 tablas de etiquetas movidas de RAM a flash vía `pgm_read_ptr()`/`pgm_read_dword()` en `paramAction()`; ahorra ~134 bytes RAM
- **Código muerto eliminado** — clase BLIND, rama SIMPLE_RX, funciones TESTBENCH NCO, `process_nr_old()`, `Command_TX1()/TX2()` (duplicados), `ref_V` float (nunca leída); neto −212 líneas
- **Bit shifts** — `adc/4` → `adc>>2`, `(in+dc)/2` → `(in+dc)>>1`, clipper `/2` → `>>1`
- **CESSB clipper I/Q** — limitación de magnitud del vector I/Q cuando `_amp > 200` con compresión 4:1; ~2-3dB más potencia efectiva sin ensanchar ancho de banda
- **AGC hang timer** — contador `hang_cnt` (600 muestras ≈ 77ms @ 7812Hz) evita que el AGC suba ganancia en pausas entre palabras; resetea al detectar señal; elimina bombeo de ruido entre sílabas
- **LMS auto-notch adaptativo (2 taps)** — filtro notch adaptivo cancela heterodinos/birdies; **eliminado en v5.18 final** — interfería con la voz cuando NR estaba activo; el `process_nr()` estándar (EA/FIR) ofrece mejor reducción de ruido sin distorsionar la voz
- **Phase unwrapping** — reemplaza `if(dp < 0) dp = dp + _UA` por desenrollado de fase correcto; evita espurios espectrales
- **readSWR fixed-point** — `float` eliminado; VSWR calculado desde suma ADC con `uint32_t` fixed-point; potencia en milivatios; display de 3 dígitos (formato x.xx)

### Correcciones de revisión de código

- **Scope CESSB** — `#define CESSB_THRESH 200` cambiado a `const uint16_t CESSB_THRESH = 200;` local (buena práctica: `#define` ignora scope C++)
- **QUAD eliminado** — los 3 bloques `#ifdef QUAD` eliminados junto con `quad_enabled`, `quad` y entrada de menú; QUAD desactivado por defecto y los comentarios del autor dicen "empeora la calidad de voz TX SSB"
- **Corrección acceso PROGMEM** — `lcd.print()` usaba `reinterpret_cast<const __FlashStringHelper*>` para imprimir etiquetas desde arrays PROGMEM. En Arduino AVR, los string literales están en RAM (`.rodata` copiado a RAM al arrancar), no en flash. El cast `__FlashStringHelper` hacía que `lcd.print()` leyese del espacio de direcciones flash, produciendo basura. Corregido a `(const char*)pgm_read_ptr(...)` — lee el puntero del array PROGMEM, luego imprime el string desde RAM.

### Eliminación de float (gran ahorro flash)

- **`smeter()` sin float** — `10*log10()` reemplazado por tabla de lookup PROGMEM de 128 bytes con normalización MSB; ahorra ~2-4KB eliminando la librería float completa
- **`cap_label` a PROGMEM** — última tabla de strings en RAM, ahorra ~18 bytes
- **DIAG fixed-point** — 6 mediciones de voltaje convertidas de `float` a enteros en milivoltios; ahorra ~300 bytes flash
- **`FWD`/`SWR` `uint16_t`** — variables del medidor SWR cambiadas de `float` a `uint16_t`; ahorra ~12 bytes RAM
- **`ref_V` eliminada** — `float ref_V = 5 * 1.15` declarada pero nunca leída

### Predistorsión AM-PM mejorada

- **Tabla expandida 16→64 entradas** — compensación 4× más fina del desplazamiento de fase del PA clase-E; indexada como `_amp >> 2` (antes `_amp >> 4`); coste: +48 bytes PROGMEM

### Decay AGC por modo

- **Modo CW** — `decayCount` automático de 200 muestras (~25ms) cuando `mode == CW`, vs 800 (~102ms) en SSB; coste flash cero, mejora significativa en CW

### Compresor de voz: Soft Knee + Make-up Gain

- **Soft knee** — zona de transición cuadrática (64 unidades bajo el umbral) suaviza el inicio de la compresión; elimina el "click" audible al cruzar el umbral
- **Make-up gain** — `comp_threshold >> 1` (~64) restaura automáticamente ~6dB del nivel perdido por la compresión; el audio comprimido iguala el volumen del original
- **`/ comp_ratio` eliminado** — ratio fijo 2:1, división reemplazada por `>> 1`; ahorra ciclos CPU y elimina la variable `comp_ratio` no usada (−2 bytes RAM)

### Noise Blanker (NB)

- **Eliminador de ruido impulsivo** — detecta pulsos cortos de alta amplitud (ruido de líneas, encendido, interferencia de electrodomésticos) y los reemplaza por la muestra anterior; colocado antes del AGC para evitar bombeo del AGC
- **Algoritmo**: rastrea nivel medio vía IIR lento (`>> 5`); dispara cuando abs(ac) > 3x el promedio; blanquea durante 8 muestras (~1ms); coste: +158 bytes flash, +6 bytes RAM

### Filtro TX Low-Cut (HPF)

- **Low-cut estilo Yaesu/Icom** — HPF IIR de primer orden en audio de micrófono, elimina frecuencias sub-audio antes de la modulación SSB; reduce potencia desperdiciada e IMD por ruido de respiración/manipulación
- **Implementación**: HPF = señal − LPF, donde LPF tiene alpha = 1/2^k (k = 3, 2, 1 para 100, 200, 400Hz); colocado después del EQ de micrófono, antes del pre-énfasis
- **Coste**: +112 bytes flash, +7 bytes RAM

### Referencia Completa de Menú

| # | Item | Función | Notas |
|---|------|---------|-------|
| 1.1 | Vol | Nivel de audio (0..16) y apagado | |
| 1.2 | Mode | Modulación (LSB, USB, CW, AM, FM) | |
| 1.3 | FilterBW | Pasabanda de audio / BW TX | |
| 1.4 | Band | Cambio de banda a frecs predefinidas | |
| 1.5 | Tune Rate | Paso de sintonía | |
| 1.6 | VFO Mode | VFO A/B, Split | |
| 1.7 | RIT | RX en tránsito | |
| 1.8 | AGC | Control Automático de Ganancia (OFF/ON) | |
| 1.9 | NR | Reducción de ruido (0-8), LMS notch si ON | |
| 1.10 | ATT | Atenuador analógico (0..-73dB) | |
| 1.11 | ATT2 | Atenuador digital (0-16 x6dB) | |
| 1.12 | S-Meter | Tipo S-meter (OFF, dBm, S, S-bar) | |
| 1.13 | SWR Meter | Medidor SWR/Potencia | *si SWR_METER* |
| 1.14 | AGC Dcy | Tiempo decay AGC 1-16 (×100 muestras) | |
| 1.15 | **Noise Blk** | **Noise Blanker ON/OFF** | **NUEVO v5.18** |
| 2.1 | CW Decoder | Activar/desactivar decodificador CW | *si CW_DECODER* |
| 2.2 | CW Tone | Filtro CW + tono lateral | *si FILTER_700HZ* |
| 2.3 | CW Off | Offset CW | *si QCX* |
| 2.4 | Semi QSK | Semi-QSK en CW | *si SEMI_QSK* |
| 2.5 | Keyer Speed | Velocidad manipulador CW (1-60 WPM) | *si KEYER* |
| 2.6 | Keyer Mode | Iambic-A/B, Straight | *si KEYER* |
| 2.7 | Keyer Swap | Intercambiar DIT/DAH | *si KEYER* |
| 2.8 | Practice | Deshabilitar TX para práctica | |
| 2.9 | Tone Vol | Volumen tono lateral CW | *si CW_DECODER* |
| 3.1 | VOX | Transmisión Operada por Voz | *si VOX_ENABLE* |
| 3.2 | Noise Gate | Umbral de audio para VOX | |
| 3.3 | TX Drive | Ganancia de audio TX (0-8) | |
| 3.4 | TX Delay | Retardo relé TX | *si TX_DELAY* |
| 3.5 | MOX | Monitor en TX | *si MOX_ENABLE* |
| 3.6 | TX Comp | Compresor de voz TX ON/OFF | |
| 3.7 | TX Emph | Pre-énfasis de micrófono | |
| 3.8 | EQ Bass | EQ de graves TX (-7..+7) | |
| 3.9 | EQ Treble | EQ de agudos TX (-7..+7) | |
| 3.10 | **TX LoCut** | **Filtro paso alto TX (Off/100/200/400Hz)** | **NUEVO v5.18** |
| 4.1 | CQ Interval | Intervalo mensaje CQ | *si CW_MESSAGE* |
| 4.2 | CQ Msg | Texto mensaje CQ | *si CW_MESSAGE* |
| 8.1 | PA bias min | Amplitud PA para 0% RF | |
| 8.2 | PA max | Amplitud PA para 100% RF | |
| 8.3 | Ref frq | Calibración cristal Si5351 | |
| 8.4 | IQ phase | Offset fase I/Q RX | |
| 8.5 | IQ Cal | Calibración I/Q RX | *si IQ_CALIBRATION* |
| 8.6 | CAT115K | Velocidad CAT 115200 (vs 38400) | *si CAT* |
| 9.1 | Sample rate | Mostrar tasa de muestreo | *invisible* |
| 9.2 | CPU load | Mostrar carga CPU % | *invisible* |
| 9.3 | ParamA | Parámetro interno A | *invisible* |
| 9.4 | ParamB | Parámetro interno B | *invisible* |
| 9.5 | ParamC | Parámetro interno C | *invisible* |
| 10.1 | Light | Iluminación pantalla ON/OFF | |

### Resumen

| Métrica | v5.17 | v5.18 | Δ |
|---------|-------|-------|---|
| Flash | 31.154 (96%) | 30.760 (95%) | **−394 bytes** |
| RAM | 1.466 (71%) | 1.342 (65%) | **−124 bytes** |
| RAM libre | 582 | 706 | **+124 bytes** |

---

**Memoria:** 31.154 bytes flash (96%), 1.466 bytes RAM (71%) — −16 bytes respecto a v5.16

### TX: Mejoras del compresor y el ecualizador

Construido sobre la base limpia de v5.16 (todos los parámetros reseteados vía EEPROM) para añadir mejoras reales.

#### Cambios

- **Incremento de VERSION** "5.16" → "5.17": fuerza reset del EEPROM en el primer arranque, cargando el nuevo valor por defecto `comp_enable=1`
- **`comp_enable = 1`** (activado por defecto): activa el compresor de voz; evita hard-clipping en entradas fuertes; el bug de EEPROM de v5.16 está resuelto mediante el incremento de VERSION, por lo que activar el compresor es ahora seguro
- **Release del compresor `>> 5` → `>> 8`**: el TC cambia de 6.7ms a 53ms (~200ms hasta llegar al 1%); elimina el pumping entre sílabas en habla conversacional (sílabas del español: 50–200ms); el ataque rápido (TC ≈ 0.30ms) no se modifica — asimetría clásica de limitadores de broadcast
- **Reescritura de `mic_eq()`**: corregidos dos bugs:
  1. `eq_high` era un LPF (fc ≈ 760Hz) — el boost de "Agudos" amplificaba en realidad 0-760Hz (graves/medios). Sustituido por HPF: `hi = in - eq_high_iir`, de modo que el control de agudos actúa ahora sobre frecuencias realmente altas (>760Hz, presencia/aire)
  2. `low_gain = 4 + (eq_low << 2)` invertía la fase cuando `eq_low < -1` (ej., eq=-7 → ganancia=-24). Nueva fórmula: `(eq_low_iir * eq_low) >> 3` es lineal, sin inversión de fase
  3. Frecuencia de corte del LPF de graves ajustada: `>> 3` (191Hz) → `>> 4` (75Hz) — separación más limpia entre graves y medios

#### Notas

- `pre_emph` permanece en 0 (desactivado); sigue accesible desde el menú 3.7 para experimentación del usuario
- El modo CW no se ve afectado: `dsp_tx_cw()` no usa `voice_compressor()` ni `mic_eq()`
- El VOX no se ve afectado: el umbral VOX depende de `_amp`, calculado antes del compresor

---

## v5.16 - Baseline TX Legacy + Corrección de Menú

**Memoria:** 31.170 bytes flash (96%), 1.465 bytes RAM (71%)

### TX: Reversión a la cadena DSP del legacy

Se restaura la cadena de audio TX para que coincida exactamente con `usdx-legazy`, como punto de partida antes de futuras mejoras.

**Causa raíz del "siseo al comienzo de cada palabra":** El EEPROM de v5.13 tenía `comp_enable=1` y `pre_emph=1` guardados. Las versiones v5.14/v5.15 cambiaron los valores por defecto a 0 pero no incrementaron la VERSION, por lo que los valores antiguos del EEPROM seguían cargándose al arrancar. El filtro de pre-énfasis (`in + (in - pre_z1) * pre_emph`) es un diferenciador de primer orden que amplifica los transientes de inicio — produciendo exactamente sibilancia al comienzo de cada palabra.

#### Cambios

- **Incremento de VERSION** "5.15" → "5.16": fuerza reset del EEPROM en el primer arranque, limpiando los valores obsoletos `comp_enable=1` y `pre_emph=1`
- **Coeficiente Hilbert revertido** (path `!MORE_MIC_GAIN`): `*16` → `*15` — coincide exactamente con el legacy
- **Orden de `dsp_tx()` revertido** (ambos paths MULTI_ADC y single ADC): `SendPLLRegisterBulk()` se ejecuta ahora **antes** de `OCR1BL = amp`, restaurando la alineación amplitud-fase del legacy (~30-50µs)

#### Notas

- `comp_enable`, `pre_emph`, `eq_low/high` permanecen en el menú (disponibles para experimentación)
- Todos los parámetros de procesamiento TX quedan a 0/desactivado tras el reset del EEPROM
- La cadena RX no se modifica

### Menú: Corrección de posiciones basura tras 10.1

- **`N_PARAMS` corregido**: 65 → 47 — `BACKL` (0xA1, "10.1 Light") es siempre el último parámetro visible; los valores por encima de 47 son parámetros internos invisibles (FREQA/FREQB/etc.) que se trataban incorrectamente como entradas válidas del menú
- **`I_PARAMS` corregido** (KEEP_BAND_DATA): `5+9` → `5+5+9=19` — añade los 5 parámetros SR-PARAM_C que faltaban; `N_ALL_PARAMS` alcanza correctamente BAND_DATA8=66
- **`I_PARAMS` corregido** (sin KEEP_BAND_DATA): `5` → `10` — `N_ALL_PARAMS` alcanza correctamente PARAM_C=57

---

## v5.15 - Mejoras del Menú TX

**Memoria:** 31,150 bytes flash (96%), 1,465 bytes RAM (71%)

### Nuevas Características del Menú TX

#### Ecualizador de Micrófono (Graves/Agudos)
- Agregado control **EQ Bass** al menú TX (rango: -7 a +7)
- Agregado control **EQ Treble** al menú TX (rango: -7 a +7)
- Ubicación: `usdx_plus_orange.ino:5158-5163`
- Permite ajustar la respuesta de frecuencia del micrófono

#### Reordenamiento del Menú TX
- Los elementos del menú ahora aparecen en orden lógico:
  1. TX Drive (3.3)
  2. TX Delay (3.4)
  3. MOX (3.5)
  4. TX Comp (3.6)
  5. TX Emph (3.7)
  6. EQ Bass (3.8)
  7. EQ Treble (3.9)

### Notas

- El procesamiento de EQ ya estaba implementado en el camino TX (líneas 1997-2033)
- Ahora accesible vía menú para ajuste del usuario
- Valores por defecto: Bass=0, Treble=0 (respuesta plana)

---

## v5.13 - Menú configurable: decay AGC, compresor TX, pre-énfasis TX

**Memoria:** 30710 bytes flash (95%), 1464 bytes RAM (71%)

### Nuevos parámetros de menú

Tres nuevos ítems accesibles desde el encoder del menú:

- **AGC Dcy** (fila 1, col E) — `agc_decay`: Tiempo de release del AGC 1-16 (×100 muestras). Defecto 8 (~800ms). Valores bajos hacen que el AGC reaccione más rápido a la desaparición de señal; valores altos dan una escucha más suave.
- **TX Comp** (fila 3, col 6) — `comp_enable`: Activar/desactivar el compresor de voz TX (Off/ON). Defecto ON. El compresor añade ~6dB de potencia media y reduce la saturación.
- **TX Emph** (fila 3, col 7) — `pre_emph`: Pre-énfasis de micrófono TX 0-3 (0=off, 1=6dB/oct, 2=12dB/oct, 3=18dB/oct). Defecto 1 (6dB/oct, adecuado para cápsulas electret).

### Detalles técnicos

- Tipo de `agc_decay` cambiado `uint16_t→uint8_t` (valor almacenado ×100 = número real de muestras; `decayCount = (uint16_t)agc_decay * 100`)
- Direcciones EEPROM: `AGC_DECAY`→0x1E, `COMP_EN`→0x36, `PRE_EMPH`→0x37
- `eeprom_version` incrementada (VERSION "5.12"→"5.13"): EEPROM se resetea automáticamente en el primer arranque; la frecuencia guardada vuelve al defecto (hay que reintroducirla tras flashear)
- N_PARAMS actualizado 47→50 para incluir los nuevos ítems visibles del menú

---

## v5.12 - Calidad SSB en voz (compresión TX + claridad RX)

**Memoria:** 30594 bytes flash (94%), 1466 bytes RAM (71%)

### TX: Mejoras del compresor

**Tiempo de decay:** `>>4` → `>>7` (release 3ms → 27ms)
- Antes: el envelope de compresión se relajaba en 3ms → "pumping" audible entre sílabas
- Ahora: 27ms de release → compresión suave y transparente, similar a radios comerciales
- El ataque sigue siendo rápido (~3ms) para capturar picos de voz

**Corrección de desbordamiento:** `in * gain` → `(int32_t)in * gain / comp_envelope`
- Antes: multiplicación int16×int16 podía desbordar en picos altos de micrófono → distorsión dura
- Ahora: aritmética 32 bits garantiza resultado correcto sin coste extra de flash

### RX: NR desactivado por defecto para voz SSB

**NR por defecto:** `nr=2` → `nr=0`
- El filtro EA con nr=2 corta a ~900Hz, atenuando significativamente las frecuencias de voz 1-3kHz
- Con nr=0, el filtro IIR de banda (filt_var) es el único limitador de ancho de banda → respuesta plana en SSB
- Los usuarios pueden activar NR desde el menú si desean reducción de ruido adicional
- CW no se ve afectado (ya forzaba nr=0 al cambiar de modo)

---

## v5.11 - Correcciones de modulación TX y filtros RX

**Memoria:** 30594 bytes flash (94%), 1465 bytes RAM (71%)

### Mejora TX: Alineación amplitud-fase

`OCR1BL` (amplitud PWM a la PA) se envía ahora **antes** de `SendPLLRegisterBulk()`,
en ambos paths (MULTI_ADC y single ADC).

- Antes: la fase se actualizaba ~88µs antes que la amplitud (error de alineación)
- Ahora: la amplitud se actualiza ~140µs antes de que el PLL se estabilice
- Efecto: menor distorsión de envolvente en SSB, señal más limpia en banda lateral

Esto restaura el comportamiento original del diseño (estaba comentado desde el código base de GW8RDI).

### Mejora RX: Ganancia uniforme en filtros SSB

Filtros SSB 2 (0-2400Hz) y 3 (0-1800Hz): segunda sección biquad `>>2` → `>>1`
para igualar la ganancia del filtro 1 (0-2900Hz).

- Antes: cambiar entre filtros SSB 1↔2 o 1↔3 producía un salto de ~6dB
- Ahora: los tres filtros SSB tienen ganancia consistente, sin saltos de volumen

---

## v5.10 - Mejoras DSP en RX y TX

**Memoria:** 30602 bytes flash (94%), 1465 bytes RAM (71%)

### Correcciones de bugs

- **Demodulador FM:** `zi` inicializado a 0 (antes = valor arbitrario → click al entrar en modo FM)
- **Eliminación DC en AM:** Reemplazado `float * 0.9999f` por aritmética entera `as_last - (as_last >> 10)` (alpha ≈ 0.999, ahorra ~128 bytes flash y ciclos CPU)

### Mejoras TX

- **Compresor de voz activado por defecto** (`comp_enable = 1`): mejor potencia media en SSB, menos IMD
- **Ratio reducido:** 4:1 → 3:1 (menos distorsión armónica, voz más natural)
- **Umbral ajustado:** 180 → 128 (activación más temprana, mayor rango de compresión útil)
- **Pre-énfasis reducido:** 12dB/oct → 6dB/oct (`pre_emph = 1`), menos sibilancia con cápsulas electret
- **KEY_CLICK activado:** rampa de amplitud en CW al ON/OFF para eliminar clicks audibles
- **Hilbert TX unificado:** coeficiente `v[6]-v[8]` corregido 15→16 (coincide con rama MORE_MIC_GAIN, mejora rechazo de banda lateral en TX SSB)

### Mejoras RX

- **Decay AGC:** 400 → 800 (~800ms), más natural y cómodo para escucha SSB prolongada

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

## Uso de Memoria (v5.16)

| Recurso | Uso | Disponible |
|---------|-----|------------|
| Flash | 31.170 bytes (96%) | ~1086 bytes |
| RAM | 1.465 bytes (71%) | 583 bytes |

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

---

**Descargo de responsabilidad:** No me responsabilizo de los daños que este firmware pueda causar en los dispositivos en los que se pueda aplicar. Úsalo bajo tu propia responsabilidad.

**Autor:** EA7LJY - Julian
**Fecha:** Junio 2026
