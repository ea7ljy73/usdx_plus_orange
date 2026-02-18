# Notas de Reorganización - uSDX Plus Orange

## Dependencias Críticas de Orden

### 1. Includes y Forward Declarations
```cpp
// Orden actual (debe mantenerse o moverse junto):
#include <avr/sleep.h>    // Necesario antes de clases que lo usen
#include <avr/wdt.h>

// Forward declarations necesarias:
void sdr_rx_03(); void sdr_rx_07();  // Líneas 2576
typedef void(*func_t)(void);           // Línea 2449
```

### 2. Clases y sus Dependencias

| Clase | Línea | Depende de | Usada por |
|-------|-------|------------|----------|
| `I2C_` | 391 | Ninguno | LCD, OLED |
| `LCD` | 475 | I2C_ (opcional), Print | Display template |
| `OLEDDevice` | 851 | Wire (Arduino) | Display template |
| `Display<T>` | 968 | Template | lcd global |
| `Blind` | 984 | Print | Dummy display |
| `Encoder` | 1029 | Ninguno | No usada |
| `I2C` | 1057 | Ninguno | SI5351 |
| `SI5351` | 1184 | I2C | setup(), loop() |
| `IOExpander16` | 1406 | Ninguno | set_lpf() |

### 3. Variables Globales Críticas

| Variable | Línea | Depende de | Usada por |
|----------|-------|------------|----------|
| `I2C_ Wire` | 469 | I2C_ | LCD, OLED |
| `SI5351 si5351` | 1199 | SI5351 | setup(), loop() |
| `func_t func_ptr` | 2449 | - | ISRs |
| `IOExpander16 ioext` | 1441 | IOExpander16 | set_lpf() |

### 4. Orden de Inicialización (setup())

```
setup()
├── si5351.powerDown()         // Primero: apagar clock
├── wdt_enable()
├── encoder_setup()           // Configurar encoder
├── initPins()               // Configurar pines
├── lcd.begin()              // Inicializar display
├── show_banner()            // Mostrar banner
├── build_lut()              // Construir LUT
├── calibrate_iq()           // Calibrar IQ
└── start_rx()               // Iniciar RX
```

### 5. ISR Functions (no pueden moverse)

```
ISR(PCINT2_vect)           // Encoder - línea 1013
ISR(TIMER2_COMPA_vect)     // DSP - línea 2975 aproximadamente
```

### 6. Reglas para Reorganización

**NO MOVER:**
- ISR handlers
- Forward declarations
- Funciones llamadas desde ISRs

**PUEDEN MOVER CON CUIDADO:**
- Clases (mantener juntas con sus dependencias)
- Funciones de alto nivel (setup, loop, funciones de menú)

**MEJOR NO TOCAR DE MOMENTO:**
- Variables globales y sus inicializaciones
- Macros y defines de configuración

### 7. Plan de Reorganización Propuesto

```
=== SECCIÓN 1: INCLUDES ===
- #include <avr/sleep.h>
- #include <avr/wdt.h>
- Arduino.h (si existe)

=== SECCIÓN 2: CONFIGURACIÓN ===
- usdx_settings.h
- Derived settings
- Hardware model detection

=== SECCIÓN 3: TIPOS Y FORWARD DECLS ===
- typedefs
- Forward declarations
- struct definitions

=== SECCIÓN 4: CLASES I2C ===
- I2C_ class
- I2C class

=== SECCIÓN 5: CLASES DISPLAY ===
- LCD class
- OLEDDevice class
- Display template
- Blind class

=== SECCIÓN 6: HARDWARE DRIVERS ===
- SI5351 class
- IOExpander16 class

=== SECCIÓN 7: VARIABLES GLOBALES ===
- I2C_ Wire
- SI5351 si5351
- IOExpander16 ioext
- func_ptr y relacionadas

=== SECCIÓN 8: DSP FUNCTIONS ===
- ssb()
- slow_dsp()
- Filtros IIR/FIR

=== SECCIÓN 9: TX FUNCTIONS ===
- dsp_tx()
- dsp_tx_cw()
- dsp_tx_am()
- dsp_tx_fm()

=== SECCIÓN 10: CW FUNCTIONS ===
- cw_tx()
- cw_decode()
- update_PaddleLatch()

=== SECCIÓN 11: DISPLAY FUNCTIONS ===
- show_banner()
- printmenuid()
- paramAction()
- smeter()

=== SECCIÓN 12: CAT FUNCTIONS ===
- analyseCATcmd()
- Command_*()

=== SECCIÓN 13: HARDWARE FUNCTIONS ===
- initPins()
- switch_rxtx()
- set_lpf()
- adc_start/stop

=== SECCIÓN 14: ISR HANDLERS ===
- PCINT2_vect
- TIMER2_COMPA_vect

=== SECCIÓN 15: MAIN ===
- setup()
- loop()
```

### 8. Verificación Post-Reorganización

После reorganización, verificar:
```bash
arduino-cli compile --fqbn arduino:avr:uno
```

Compilar debe succeeder sin cambios de funcionalidad.

## Estado Actual

- **Archivo original:** 5855 líneas
- **Después de FASE 1:** 5709 líneas (-146)
- **Flash:** 30144 bytes (93%)
- **RAM:** 1453 bytes (70%)
