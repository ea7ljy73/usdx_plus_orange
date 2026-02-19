# uSDX Plus Orange - Notas de Release

**Versión:** 5.13
**Base:** uSDX Legacy 1.02x / usdxWHITEBUTTONS v4.00d (GW8RDI)
**Plataforma:** ATMEGA328P @ 20MHz
**Autor:** EA7LJY - Julian

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

## Uso de Memoria (v5.13)

| Recurso | Uso | Disponible |
|---------|-----|------------|
| Flash | 30.710 bytes (95%) | 1546 bytes |
| RAM | 1.464 bytes (71%) | 584 bytes |

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

## Operación

Actualmente, las siguientes funciones han sido asignadas a botones de acceso directo (L=izquierdo, E=encoder, R=derecho) y elementos de menú:

| Elemento Menú       | Función                                     | Botón |
| ------------------- | -------------------------------------------- | ------ |
| 1.1 Vol             | Nivel de audio (0..16) & apagado/encendido (girar izquierda) | **E +turn** |
| 1.2 Mode            | Modulación (LSB, USB, CW, AM, FM) | **R** |
| 1.3 FilterBW        | Pasabanda de audio (Full, 300..3000, 300..2400, 300..1800, 500, 200, 100, 50 Hz), también controla el BW TX SSB. | **R double** |
| 1.4 Band            | Cambio de banda a frec predefinidas CW/FT8 (80,60,40,30,20,17,15,12,10,6m) | **E double** |
| 1.5 Tune Rate       | Tamaño de paso de sintonía 10M, 1M, 0.5M, 100k, 10k, 1k, 0.5k, 100, 10, 1 | **E or E long** |
| 1.6 VFO Mode        | Selecciona VFO diferente, o VFO split RX/TX (A, B, Split) | **2x R long** |
| 1.7 RIT             | RX en tránsito (ON, OFF) | **R long** |
| 1.8 AGC             | Control Automático de Ganancia (OFF, Fast, Slow) | |
| 1.9 NR              | Nivel de reducción de ruido (0-8), load-pass & smooth | |
| 1.10 ATT            | Atenuador Analógico (0, -13, -20, -33, -40, -53, -60, -73 dB) | |
| 1.11 ATT2           | Atenuador Digital en etapa CIC (0-16) en pasos de 6dB | |
| 1.12 S-Meter        | Tipo de S-Meter (OFF, dBm, S, S-bar) | |
| 1.13 SWR Meter      | Medidor SWR (OFF, ON) | |
| 1.14 AGC Dcy        | Tiempo decay AGC 1-16 (x100 muestras), defecto 8 (~800ms) | |
| 2.1 CW Decoder      | Habilitar/deshabilitar decodificador CW (ON, OFF) | |
| 2.2 CW Tone         | Filtro CW+Tono lateral (600, 700) | |
| 2.3 CW Off          | Offset CW (300..2000 Hz) | |
| 2.4 Semi QSK        | En TX silencia RX en espacios de signo y palabra CW | |
| 2.5 Keyer Speed     | Velocidad del manipulador CW en Paris-WPM (1..60) | |
| 2.6 Keyer Mode      | Tipo de manipulador (Iambic-A, -B, Straight) | |
| 2.7 Keyer Swap      | Intercambiar entradas DIH, DAH del manipulador (ON, OFF) | |
| 2.8 Practice        | Deshabilitar TX para práctica (ON, OFF) | |
| 2.9 Tone Vol        | Volumen tono lateral CW (0..16) | |
| 3.1 VOX             | Transmisión Operada por Voz (ON, OFF) | |
| 3.2 Noise Gate      | Umbral de audio para TX SSB y VOX (0-255) | |
| 3.3 TX Drive        | Ganancia de audio de transmisión (0-8) en pasos de 6dB, 8=amplitud constante para SSB | |
| 3.4 TX Delay        | Retrasa TX para permitir que el relé PA conmute completamente antes de TX (0-255 ms) | |
| 3.5 MOX             | Monitor en transmisión (audio sin silenciar durante TX) | |
| 3.6 TX Comp         | Compresor de voz TX (ON/OFF), añade ~6dB de potencia media | |
| 3.7 TX Emph         | Pre-énfasis micrófono TX (0=off, 1=6dB/oct, 2=12dB/oct, 3=18dB/oct) | |
| 4.1 CQ Interval     | Tiempo de inactividad en segundos antes de nuevo Mensaje CQ (0-60) | |
| 4.2 CQ Msg          | Texto del mensaje CQ, pulsar botón izquierdo en menú iniciará envío | **L** |
| 8.1 PA bias min     | Nivel PWM de amplitud PA (0-255) para representar 0% de salida RF | |
| 8.2 PA max          | Nivel PWM de amplitud PA (0-255) para representar 100% de salida RF | |
| 8.3 Ref frq          | Frecuencia real del cristal si5351, usada para calibración de frecuencia | |
| 8.4 IQ phase        | Offset de fase I/Q RX en grados (0..180 grados) | |
| 10.1 Backlight      | Iluminación de pantalla (ON, OFF) | |
| power-up             | Restablecer a ajustes de fábrica | **E long** |
| main                | Sintonizar frecuencia (20kHz..99MHz) | **turn** |
| main                | Menú rápido | **L +turn** |
| main                | Entrar en menú | **L** |
| RIT                 | RIT atrás | **R** |
| menu                | Menú atrás | **R** |

---

**Descargo de responsabilidad:** No me responsabilizo de los daños que este firmware pueda causar en los dispositivos en los que se pueda aplicar. Úsalo bajo tu propia responsabilidad.

**Autor:** EA7LJY - Julian
**Fecha:** Febrero 2026
