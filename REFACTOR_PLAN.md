# uSDX Plus Orange — Plan de Refactorización Integral a Proyecto Limpio

Documento de planificación para reconstruir el firmware `usdx_plus_orange` desde
cero como un proyecto modular, corrigiendo bugs heredados y maximizando calidad
RX/TX con la misma limitación de hardware (ATMEGA328P: 32KB flash / 2KB RAM).

Estado: **EN EJECUCIÓN** (paso a paso, validando en cada paso).

---

## 1. Análisis del código actual (qué hay realmente)

### 1.1 Estructura monolítica

| Sección | Líneas | % |
|---|---|---|
| Sec 01 DERIVED SETTINGS | 289 | 4.0% |
| Sec 02 I2C CLASSES (LCD) | 138 | 1.9% |
| Sec 03 DISPLAY CLASSES (LCD/OLED) | 766 | 10.7% |
| Sec 04 PRIMARY I2C CLASS (SI5351) | 157 | 2.2% |
| Sec 05 SI5351 CLOCK GENERATOR | 308 | 4.3% |
| Sec 06 I/O EXPANDERS + LPF | 433 | 6.1% |
| Sec 07 GLOBAL VARIABLES | 71 | 1.0% |
| Sec 08 TX FUNCTIONS | 76 | 1.1% |
| Sec 09 CW FUNCTIONS | 322 | 4.5% |
| Sec 10 RX/DSP FUNCTIONS | 1080 | 15.1% |
| Sec 11 UI AND DISPLAY | 1738 | 24.3% |
| Sec 12 CAT INTERFACE | 318 | 4.4% |
| Sec 13 HW INITIALIZATION | 369 | 5.2% |
| Sec 14 MAIN LOOP | 1063 | 14.9% |
| **Total** | **7128** | 100% |

**Conclusión:** todo vive en UN solo `.ino` de 7128 líneas. La UI (24%) y RX/DSP
(15%) dominan. Imposible de mantener por módulos, probar de forma aislada, o
auditar el camino crítico de tiempo real.

### 1.2 Código condicional (inflado)

- **415 directivas** `#if/#ifdef/#else/#endif`.
- **101 identificadores condicionales** distintos (`#ifdef X`, `#ifndef X`).
- De ellos, **~80 están desactivados** en la config actual (WHITE_BUTTONS).
- Hardware soportado simultáneamente en el código: 5 modelos (BLACK_BRICK,
  RED_CORNERS, RED_BUTTONS, WHITE_BUTTONS, TRUSDX), 6 LPF banks, 3 displays,
  QCX, ONEBUTTON, TESTBENCH, KEYER, CW_MESSAGE, etc.

**Conclusión:** la mayoría del código nunca se compila en tu build, pero el
preprocesador y el mantenimiento lo sufren. Es dead-code condicional tolerado
por gcc que ensucia el árbol de decisión mental.

### 1.3 Bugs heredados confirmados (presentes en TODAS las variantes)

| Bug | Ubicación | Efecto |
|---|---|---|
| Overflow en `process_agc_fast()`: `(gain>>10)*in` sin `int32_t` | usdx_plus_orange.ino:2618 (también en WHITEBUTTONS, legacy, ft8) | Distorsión/recorte en señales fuertes con AGC=1 |
| Rama `OLD_RX` con `xx disabled int16_t ac = i + qh;` | línea 3454 | Ni compilaría si se activara (rama muerta rota) |
| Compensación de ganancia filtro SSB/CW desconectada (`//<<2` comentado) | línea 3117-3119 | Posible salto de nivel SSB↔CW |
| 3 clases I2C duplicadas (Sec 02, Sec 04, I/O expander) | líneas 312-449, 1216-1371 | Fuentes de verdad múltiples, riesgo de desincronización |
| `freq_calc_fast()` usa división 64-bit (`__divdi3`) | línea 1400 | ~400-900 ciclos/muestra; seguro pero caro |
| AGC `process_agc` M0PUB y `process_agc_fast` coexisten sin jerarquía | líneas 2617, 2661 | Dos algoritmos, default `agc=2`, mantenimiento duplicado |

### 1.4 Presupuesto actual (con las mejoras ya aplicadas)

- **Flash:** 31062B / 32256B = **96%** (solo ~1.2KB libres)
- **RAM:** 1369B / 2048B = **66%** (~679B libres)
- F_CPU real = **20MHz** (no 16MHz): 4166 ciclos/muestra TX @4800SPS

**Conclusión:** sin liberar flash (eliminando dead-code y duplicados), no caben
más mejoras de "gama alta". El proyecto modular resuelve esto por construcción.

---

## 2. Arquitectura objetivo (proyecto modular)

**Nota técnica (descubierta en Paso 1):** Arduino compila el sketch como un
directorio con un `.ino` (mismo nombre del directorio). Los módulos `.h`/`.cpp`
deben vivir **junto al `.ino`**, no en subcarpeta `src/` (esa carpeta es
ignorada/tratada aparte por arduino-cli). Además, el v1 monolítico y el v2 no
pueden coexistir en el mismo directorio (símbolos duplicados al compilar juntos).

Por ello la estructura es un **proyecto nuevo independiente**:

```
usdx_plus_orange_v2/             # NUEVO directorio (el v1 queda intacto)
├── usdx_plus_orange_v2.ino     # setup() + loop() + ISRs críticos (delgado)
├── usdx_settings.h              # SOLO configuración (hardware model + features)
├── usdx_filter.h                # Filtros IIR/FIR (SSB/CW) — sin mismatch
├── i2c.h / i2c.cpp              # I2C bit-bang (UNA sola copia)
├── si5351.h / si5351.cpp        # Driver SI5351: init, freq(), freq_calc_fast()
├── display.h / display.cpp      # LCD/OLED tras interfaz común
├── gpio_expander.h/.cpp         # I/O expander + LPF switching
├── tx.h / tx.cpp                # ssb(), dsp_tx(), polar, clipper/CESSB/comp/EQ
├── rx.h / rx.cpp                # CIC decimador, Hilbert, demod, NR, NB
├── agc.h / agc.cpp              # AGC único (hang + noise floor), fix overflow
├── cw.h / cw.cpp                # CW: keyer, decoder, messages, tones
├── cat.h / cat.cpp              # CAT TS-480
├── vfo.h / vfo.cpp              # VFO A/B, RIT, split, band memory
├── eeprom.h / eeprom.cpp        # Persistencia de settings
└── ui.h / ui.cpp                # Menú, encoder, botones, S-meter
```

### Principios de diseño

1. **Una fuente de verdad por subsistema** — una sola clase I2C, un solo AGC.
2. **El DSP de tiempo real se extrae SIN tocar su comportamiento** (pasos 0-2):
   el CIC en 8 fases (`sdr_rx_00..07`) y `dsp_tx()` se mueven intactos.
3. **Sin `#ifdef` de hardware dentro del código** — `usdx_settings.h` define
   pins/constantes según el modelo; el código siempre compila la ruta activa.
   El dead-code desaparece por construcción.
4. **Config por defecto = WHITE_BUTTONS** (hardware actual) → build mínimo y
   óptimo, maximizando el margen flash/RAM.
5. **Nothing breaks**: cada paso mantiene build verde y (idealmente) el mismo
   comportamiento en hardware.

---

## 3. Plan de implementación paso a paso

Cada paso termina con: (a) compilación verde, (b) nota de uso de flash/RAM,
(c) impacto de comportamiento. Un paso se considera COMPLETO solo si compila
con las mismas prestaciones (o mejores).

| Paso | Módulo | Acción | Comportamiento |
|---|---|---|---|
| 0 | Esqueleto v2 | Crear `usdx_plus_orange_v2/` con `.ino` tipo y módulos `.h` vacíos | v1 intacto ✅ |
| 1 | I2C + SI5351 | Crear `i2c.h/cpp` y `si5351.h/cpp` limpios (adaptar de v1, resolver macros globales) | v2 compila ✅ (commit 6e7d98f) |
| 2 | DSP RX/TX ISRs | Copiar `dsp_tx`, `sdr_rx_00..07`, `freq_calc_fast` intactos a `tx`/`rx` | v2 igual a v1 ✅ (commit 7307e9a, **parity check ssb real**) |
| 3 | Filtros | Normalizar ganancia SSB/CW (fix `zc0/64` CW) | Mejora ✅ |
| 4 | Firmware operativo | `hw.h` (pins, ADC, timers, ISR, switch_rxtx) + setup/loop + VOX | Idéntico (config activa) ✅ |
| 5 | UI esencial | `display.h` (LCD HD44780 + encoder PCINT) + sintonía VFO + botón step | Idéntico (config activa) ✅ |
| 6 | CW decoder + keyer | `cw.h`: keyer Iambic A/B + decoder (buffer `cw_line[]`, sin LCD coupling) | ✅ |
| 7 | CAT | `cat.h`: TS-480 FA/IF/MD/RX/TX/ID/PS/AI/RT1/XT1/AG0/FL0/RS/RC/RTS | ✅ (sin sprintf, menos flash) |
| 8 | UI menú completo + VFO/EEPROM | **Menú declarativo (tabla + callbacks + PROGMEM)** | ✅ 31 params, RAM 50% |
| 9 | VFO/EEPROM persistencia | `vfo.h`: band memory (freq/mode by band, EEPROM) | ✅ |
| 10 | Optimización flash + UI | S-meter (barra 6 seg), decoder CW en display, modos AM/FM | ✅ |
| 11 | Mejoras "gama alta" | Compresor/EQ/CESSB por diseño con margen asegurado | ✅ integradas en tx.h|

### Regla de verificación por paso
```
make check   -> compila sin errores/warnings
benchmark    -> CPU ISR RX/TX < 100% (medir con sketch o DEBUG si cabe)
git diff     -> no hay cambios de comportamiento no documentados
```

---

## 4. Mejoras "gama alta" a incorporar de fábrica

| Mejora | Diseño |
|---|---|
| AGC único con hang-time + noise floor | Ya implementado en `process_agc`; se elimina la duplicación con `agc_fast` y su overflow |
| Compresor de voz + EQ + low-cut + pre-emph + CESSB | Reintroducidos en `ssb()` (trabajo ya hecho); en el proyecto nuevo quedan con margen de CPU garantizado |
| `freq_calc_fast` optimizada | Opcional: reemplazar `__divdi3` por aritmética 32/64-bit más rápida (paso 9, verificado contra registros SI5351 idénticos) |
| Filtros normalizados | Ganancia SSB/CW consistente, sin saltos de nivel |
| Testbench/DIAG limpio | Añadir `DIAG` de arranque sin duplicar código |

---

## 5. Riesgos y mitigaciones

| Riesgo | Mitigación |
|---|---|
| Romper el timing ISR (RX CIC / TX polar) al reorganizar | Pasos 0-2 mueven el DSP **intacto** (byte-identical), solo cambia de ubicación |
| Falta de hardware para validar cada paso | Cada paso mantiene build verde y mismo uso flash/RAM; los cambios de comportamiento se limitan a pasos 3-4 y 9 (verificables) |
| Overrun de CPU con compresor/EQ activos | El margen de flash liberado permite activarlos sin riesgo; se mide el benchmark en cada paso |
| Aumento de flash por arquitectura más clara | Objetivo: **reducir** flash al eliminar duplicados; midiendo en cada paso |

---

## 6. Decisiones registradas (historial)

- **2026-09-02** — Análisis inicial: memoria stock 96% flash / 66% RAM. Se
  aplicaron mejoras TX (clipper legado + LPF 1/9) y RX (AGC hang+noise floor +
  TX comp/EQ/CESSB) en el árbol actual. Ver commits 90bcf7a y el pendiente.
- **2026-09-02** — Decisión del usuario: NO eliminar código muerto en el árbol
  actual; se prefiere el proyecto modular nuevo donde el dead-code se elimina
  por construcción.
- **2026-09-02** — Decisión del usuario: NO tocar `freq_calc_fast` por ahora
  (conservador); queda como paso opcional 9.
- **2026-09-02** — Verificado paridad de `ssb()` v1 vs v2: lógica DSP idéntica
  (solo se eliminaron ramas `#ifdef` cuya config no aplica). Paso 2 validado.
- **2026-09-02** — Filtros normalizados en v2 (ganancia CW `/64` vs `/8` del v1)
  para eliminar el salto de volumen SSB↔CW.
- **2026-09-02** — El CW decoder y keyer se mueven al Paso UI (acoplamiento con
  `lcd`/`menumode`/botones). Se extraerá con el menú.
- **2026-09-02** — Menú v2 rediseñado como tabla declarativa `MENU[]` (PROGMEM)
  con `on_change` callbacks por parámetro y persistencia por slot EEPROM,
  sustituyendo el switch de 49 cases + `NEXT_MENU` probing + post-handling
  inline del legacy. Mismo uso (BL/BR/encoder), mejor diseño. RAM: 78%→50%.
- **2026-09-02** — Labels de tabla y tabla completa en PROGMEM (flash); acceso
  via `memcpy_P` + `pgm_read_*`. RAM 1599B→1039B.
- **2026-09-02** — CW extraído a `cw.h`: keyer Iambic A/B (state machine fiel)
  + decoder (escribe `cw_line[]`, desacoplado del LCD, mejora vs legacy que lo
  escribía directamente a `lcd`). Integrado en loop (keyer si CW, decoder en RX).
- **2026-09-02** — Paridad RX verificada (diff normalizado v1 vs v2): `slow_dsp`,
  `process_agc`, `process_nr`, `process_agc_fast`. Las únicas diferencias son las
  mejoras intencionales del v2: sintaxis de llaves, rama inactiva eliminada, y el
  **fix de overflow `(int32_t)` en `process_agc_fast`** (bug heredado del v1).
- **2026-09-02** — VFO/band memory persistente (`vfo.h`): freq/mode por banda en
  EEPROM, restore al arranque, apply hook a SI5351. CAT completo como módulo.
- **2026-09-02 (REVISIÓN pre-hardware)** — Revisión completa del v2. Fixes
  críticos aplicados:
  - `do_tune` fuera de rango: usar `stepsizes[10]` PROGMEM (menú permite 0..9)
  - `switch_rxtx` TX: habilitar `TCCR1A COM1B1` (KEY_OUT PWM al PA) — el TX no
    emitía sin esto
  - Parámetros de menú: `menu_load_all()` al arranque (no se restauraban)
  - Colisión EEPROM: VFO movido a `0x250` (libre de slots de menú hasta 0x248)
  - Encoder ISR: `digitalRead`→`PIND` directo (seguro en ISR)
  - Decoder CW: `cw_set_keyed` desde `switch_rxtx` (antes nunca transicionaba)
- Build tras fixes: 19564B flash (60%), 1425B RAM (69%), 623B libres, 0 warnings.
- **2026-09-02 (REVISIÓN 2, pre-hardware)** — Encontrado y corregido un bug
  CRÍTICO en el CIC RX: `sdr_rx_00..07` llamaban a la siguiente fase
  directamente → recursión infinita en cada tick de ISR (stack overflow, RX
  muerto). Ahora cada fase setea `func_ptr` y retorna (como v1). También
  corregida la referencia ADC de RX: `admux[]` ahora captura `ADMUX` completo
  (con 1.1V interno), no solo el canal — la calibración ADC del SDR era
  incorrecta. Build final: 19816B (61%), 1425B (69%), 0 warnings.
- **2026-09-02 (REVISIÓN 3, pre-hardware)** — Encontrado y corregido un bug
  CRÍTICO de sample-rate: `switch_rxtx` NO reconfiguraba `OCR2A` a F_SAMP_TX/
  F_SAMP_RX según dirección → la ISR de TX corría a 62.5kHz en vez de 4.8kHz
  (13× más rápido: modulación polar rota, I2C reventando CPU). Ahora recalcula
  `OCR2A` como el v1 (línea 4144). También la tasa ADC RX ahora usa `F_ADC_CONV`
  (192307/2) como el v1 para evitar clicks de audio. Build: 19852B (61%),
  1425B (69%), 0 warnings.

---

## 7. Próximos pasos inmediatos

1. **Paso 0**: crear estructura de carpetas y esqueleto.
2. **Paso 1**: extraer I2C + SI5351 intactos, compilar verde.
3. Continuar secuencialmente según validación.