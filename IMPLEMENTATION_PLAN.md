# Plan de Refactorización: uSDX Plus Orange - Desde Cero

## Resumen del Proyecto

**Objetivo**: Reescribir completamente el firmware uSDX desde cero, manteniendo toda la funcionalidad legacy mientras se implementa una arquitectura modular y optimizada para ATmega328P.

**Proyecto Original**: `Ft8-menu/` (copiado como referencia legacy)
**Nueva Implementación**: `usdx_plus_orange/` (reemplazo completo)

---

## Estado del Código Legacy (Referencia)

| Métrica | Valor |
|---------|-------|
| Líneas de código | 6,360 (monolítico en `.ino`) |
| Flash Used | 32,278 bytes (105% - excede límite) |
| SRAM Used | 1,503 bytes (73% de 2KB) |
| Librerías | EEPROM, Wire, LiquidCrystal |

---

## Arquitectura Objetivo

```
usdx_plus_orange.ino          (150-200 líneas)
├── setup()
└── loop()

usdx_config.h                 (300+ líneas)
├── #includes
├── #defines de pines
├── #defines de variables globales
├── Enums unificados
└── Macros de utilidad

src/
├── hal/                      (Hardware Abstraction Layer)
│   ├── gpio.h/.cpp           (100 líneas)
│   ├── i2c.h/.cpp            (150 líneas)
│   └── adc.h/.cpp            (80 líneas)
│
├── drivers/                  (Drivers de dispositivos)
│   ├── si5351.h/.cpp         (250 líneas)
│   ├── display.h/.cpp        (400 líneas)
│   ├── lpf_switch.h/.cpp     (100 líneas)
│   └── ina219.h/.cpp         (100 líneas)
│
├── dsp/                      (Procesamiento Digital)
│   ├── dsp_core.h            (150 líneas - CONSTANTES + INLINE FUNCS)
│   ├── ssb.h/.cpp            (300+ líneas)
│   ├── agc.h/.cpp            (100 líneas)
│   ├── nr.h/.cpp             (150 líneas)
│   └── filters.h             (constantes)
│
├── radio/                    (Lógica de Radio)
│   ├── state.h/.cpp          (variables globales + singleton)
│   ├── bands.h               (configuración de bandas)
│   ├── vfo.h/.cpp            (150 líneas)
│   └── rx_tx.h/.cpp          (200 líneas)
│
├── ui/                       (Interfaz de Usuario)
│   ├── display.h/.cpp        (renderizado LCD/OLED)
│   ├── menu.h/.cpp           (300 líneas)
│   ├── encoder.h/.cpp        (100 líneas)
│   ├── smeter.h/.cpp         (100 líneas)
│   └── ft8_menu.h/.cpp       (50 líneas - simple)
│
├── cat/                      (CAT Interface)
│   └── cat_interface.h/.cpp  (300 líneas)
│
└── cw/                       (CW Functionality)
    ├── keyer.h/.cpp          (200 líneas)
    └── decoder.h/.cpp        (200 líneas)
```

---

## Enfoque de Optimización de Memoria

### Para ATmega328P (2KB SRAM, 32KB Flash)

**Variables Globales**: Acceso directo con `extern` (sin wrappers/clases)
- Las clases añaden overhead (~10-20 bytes)
- Acceso directo a memoria es más eficiente en AVR

**Funciones DSP Críticas**: `inline` en headers
- `ssb()`, `arctan3()`, `slow_dsp()`
- `process_agc()`, `process_nr()`
- Llamadas desde ISR - no puede haber overhead

**Strings**: PROGMEM por defecto
- Todas las etiquetas de menú en Flash
- Uso de `F()` macro para strings literales

**Configuración**: #ifdef unificados
- Eliminar duplicaciones de enums
- Una sola fuente de verdad (`usdx_config.h`)

---

## Plan de Implementación por Fases

### FASE 1: Foundation (Configuración Base)

**Objetivo**: Crear `usdx_config.h` unificado con toda la configuración

**Entregables**:
- [ ] `usdx_config.h` con:
  - [ ] Definiciones de pines (LCD, encoder, keyer, etc.)
  - [ ] Enums unificados (`action_t`, `params_t`, `vfo_t`)
  - [ ] Macros unificadas (`_N`, `EA`, etc.)
  - [ ] Configuración de hardware (F_MCU, etc.)
  - [ ] Flags de features (#ifdef de cada config)

**Tareas**:
1. Consolidar todos los `#define` de pines del `.ino`
2. Unificar enums duplicados (`ParamAction` + `action_t` → uno)
3. Unificar `ParamId` + `params_t` → uno
4. Documentar cada flag de compilación

**Criterio de Éxito**: Compila sin warnings, tamaño similar o menor

---

### FASE 2: HAL (Hardware Abstraction Layer)

**Objetivo**: Abstraer el acceso al hardware

**Entregables**:
- [ ] `hal/gpio.h/.cpp` - digitalRead/Write, pinMode
- [ ] `hal/i2c.h/.cpp` - Wire abstraction
- [ ] `hal/adc.h/.cpp` - ADC sampling

**Tareas**:
1. Implementar `_digitalRead()` con inversión configurable
2. Wrapper para Wire con timeouts
3. Funciones de ADC con prescaler configurable

**Criterio de Éxito**: Hardware funciona igual que legacy

---

### FASE 3: Drivers Básicos

**Objetivo**: Drivers de dispositivos esenciales

**Entregables**:
- [ ] `drivers/si5351.h/.cpp` - Control del sintetizador
- [ ] `drivers/lpf_switch.h/.cpp` - Selección de filtros
- [ ] `drivers/display.h/.cpp` - LCD/OLED unificado

**Tareas**:
1. Portar clase SI5351 existente
2. Implementar selección de LPF según banda
3. Unificar LCD 16x2 y OLED SSD1306/SH1106

**Criterio de Éxito**: Frecuencia correcta, display muestra caracteres

---

### FASE 4: DSP Core

**Objetivo**: Procesamiento de señal optimizado

**Entregables**:
- [ ] `dsp/dsp_core.h` - Constantes + inline funcs
- [ ] `dsp/ssb.h/.cpp` - Modulación/demodulación SSB
- [ ] `dsp/agc.h/.cpp` - Control automático de ganancia
- [ ] `dsp/nr.h/.cpp` - Reducción de ruido
- [ ] `dsp/filters.h` - Tablas de filtros

**Tareas**:
1. Mantener `ssb()`, `arctan3()`, `magn()` como inline
2. Externalizar `dsp_tx()`, `dsp_tx_cw()`, `dsp_tx_am()`, `dsp_tx_fm()`
3. Externalizar `sdr_rx()`, `process()`
4. Portar AGC y NR existentes

**Criterio de Éxito**: Audio SSB igual que original

---

### FASE 5: Radio State

**Objetivo**: Estado del radio encapsulado

**Entregables**:
- [ ] `radio/state.h/.cpp` - Variables globales (singleton o extern)
- [ ] `radio/bands.h` - Configuración de bandas
- [ ] `radio/vfo.h/.cpp` - Gestión de VFOs
- [ ] `radio/rx_tx.h/.cpp` - Conmutación RX/TX

**Tareas**:
1. Definir variables globales una sola vez
2. Implementar cambio de banda con LPF automático
3. Implementar `switch_rxtx()`

**Criterio de Éxito**: Cambio de banda y TX/RX funcionan

---

### FASE 6: UI (Interfaz de Usuario)

**Objetivo**: Menú y display

**Entregables**:
- [ ] `ui/display.h/.cpp` - Renderizado de pantalla
- [ ] `ui/menu.h/.cpp` - Sistema de menús
- [ ] `ui/encoder.h/.cpp` - Control del encoder
- [ ] `ui/smeter.h/.cpp` - Medidor S
- [ ] `ui/ft8_menu.h/.cpp` - Menú FT8 (simple)

**Tareas**:
1. Implementar labels en PROGMEM
2. Sistema de navegación de menús
3. Renderizado LCD/OLED con #ifdef
4. Contador de encoder

**Criterio de Éxito**: Menú navegable, encoder responde

---

### FASE 7: CAT Interface

**Objetivo**: Control por ordenador

**Entregables**:
- [ ] `cat/cat_interface.h/.cpp` - Todos los comandos CAT

**Tareas**:
1. Portar todos los `Command_*()` functions
2. Implementar parsing de comandos
3. Emulación Kenwood/Yaesu/Icom

**Criterio de Éxito**: CAT responde igual que original

---

### FASE 8: CW Functionality

**Objetivo**: CW keyer y decoder

**Entregables**:
- [ ] `cw/keyer.h/.cpp` - Keyer interno
- [ ] `cw/decoder.h/.cpp` - Decodificador CW

**Tareas**:
1. Portar funciones de keyer
2. Portar decoder CW
3. Mensajes CW programables

**Criterio de Éxito**: CW TX/RX funciona

---

### FASE 9: EEPROM y Finalización

**Objetivo**: Persistencia y test final

**Entregables**:
- [ ] `eeprom_manager.h/.cpp` - Gestión de EEPROM
- [ ] `usdx_plus_orange.ino` - setup() y loop()
- [ ] Documentación de tests

**Tareas**:
1. Implementar save/load de estado
2. Integrar todo en `.ino`
3. Tests de verificación completos

**Criterio de Éxito**: Compila < 32KB, funciona igual que original

---

## Estimación de Tamaño

| Módulo | Flash Est. | SRAM Est. |
|--------|-----------|-----------|
| HAL | 1 KB | 50 bytes |
| Drivers | 4 KB | 200 bytes |
| DSP | 6 KB | 300 bytes |
| Radio | 3 KB | 400 bytes |
| UI | 5 KB | 500 bytes |
| CAT | 2 KB | 200 bytes |
| CW | 3 KB | 300 bytes |
| **TOTAL** | **~24 KB** | **~1950 bytes** |

**Objetivo**: 24 KB vs 32 KB legacy → 8 KB de margen

---

## Configuraciones Soportadas

### Hardware
- LCD 16x2 paralelo
- OLED SSD1306 (I2C)
- OLED SH1106 (I2C)

### Features (cada uno con #ifdef)
- `KEYER` - CW keyer interno
- `CW_DECODER` - Decodificador CW
- `SEMI_QSK` - Semi-QSK breaking
- `RIT_ENABLE` - RIT
- `SWR_METER` - Medidor SWR
- `VSS_METER` - Medidor voltaje
- `CLOCK` - Reloj

### Frecuencias de CPU
- 20 MHz (standard)
- >16 MHz (overclock)

---

## Tests de Verificación

### Tests de Compilación
| # | Test | Criterio |
|---|------|----------|
| T1 | Compila configuración A (LCD+SI5351+KEYER+CWDEC) | Sin errores |
| T2 | Compila configuración B (OLED+SI5351) | Sin errores |
| T3 | Tamaño Flash | < 32,000 bytes |

### Tests Funcionales
| # | Función | Test |
|---|---------|------|
| F1 | Recepción LSB | Audio correcto |
| F2 | Recepción USB | Audio correcto |
| F3 | Transmisión LSB | Frecuencia correcta |
| F4 | Transmisión USB | Frecuencia correcta |
| F5 | CW TX | Tono correcto |
| F6 | CW RX | Decodificación |
| F7 | CAT commands | Respuesta igual |
| F8 | S-meter | Lectura calibrada |
| F9 | EEPROM | Persistencia |
| F10 | Cambio de banda | Frecuencia correcta |

---

## Dependencias

```
arduino:avr@1.8.6
EEPROM (built-in)
Wire (built-in)
LiquidCrystal (para LCD)
```

---

## Riesgos y Mitigaciones

| Riesgo | Probabilidad | Impacto | Mitigación |
|--------|--------------|---------|------------|
| Código excede Flash | Media | Alto | Optimizaciones agresivas, eliminar código muerto |
| ISR demasiado lenta | Baja | Alto | Mantener funciones críticas inline |
| Regresiones de funcionalidad | Media | Alto | Tests comparativos con original |
| Complejidad de #ifdef | Alta | Medio | Documentar cada configuración |

---

## Próximos Pasos

1. [ ] Eliminar código actual de `usdx_plus_orange.ino` (mantener solo includes básicos)
2. [ ] Crear `usdx_config.h` con configuración unificada (FASE 1)
3. [ ] Ejecutar FASE 2: HAL
4. [ ] Iterar por cada fase
5. [ ] Tests finales

---

## Notas del Analisis Previo

### Duplicaciones Encontradas en Legacy
- `enum action_t` y `enum ParamAction` - unificar
- `enum params_t` y `enum ParamId` - unificar
- `_N(x)` macro definida múltiples veces - centralizar

### Funciones DSP Críticas (deben permanecer inline)
- `ssb()`
- `arctan3()`
- `slow_dsp()`
- `process_agc()`
- `process_nr()`

### Funciones que pueden externalizarse
- `dsp_tx()`, `dsp_tx_cw()`, `dsp_tx_am()`, `dsp_tx_fm()`
- `sdr_rx()`, `process()`
- Todos los `Command_*()` de CAT
- Funciones de menú

---

**Plan generado el**: 2026-01-07
**Basado en análisis de**: `usdx_plus_orange.ino` (6,360 líneas)

---

## Instructivo para Iniciar

Para comenzar la refactorización desde cero:

1. **Hacer backup** del código actual si es necesario
2. **Ejecutar FASE 1**: Crear `usdx_config.h` con toda la configuración unificada
3. **Verificar** que el archivo de configuración compila correctamente
4. **Continuar** con FASE 2 en adelante

**Esperando confirmación para comenzar con FASE 1...**
