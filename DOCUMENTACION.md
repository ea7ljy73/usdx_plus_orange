# uSDX Plus Orange - Documentación del Firmware Modular

## 1. Introducción

### 1.1 Descripción del Proyecto

uSDX Plus Orange es una reescritura completa del firmware original de uSDX (aproximadamente 6,000 líneas de código en un único archivo) hacia una arquitectura modular y mantenible. El firmware está diseñado para el transceiver SDR dekits uSDX basado en el microcontrolador ATmega328P.

**Características principales:**
- Arquitectura modular con separación clara de responsabilidades
- Compatible con el hardware original de uSDX
- Soporte para crystals de 27MHz (frecuencia estándar)
- Procesamiento de señales DSP completo (SSB, CW, AM, FM)
- Interfaz CAT completa
- Decoder CW integrado
- Menú de configuración FT8

### 1.2 Historia del Proyecto

El código original de uSDX fue desarrollado por Guido PE1NNZ (QCX-SSB) y ha evolucionado a lo largo de los años. Esta versión modular fue creada para:
- Reducir el tamaño del código (el original excedía los 32KB de Flash)
- Mejorar la mantenibilidad del código
- Facilitar la incorporación de nuevas funcionalidades
- Separar las capas HAL (Hardware Abstraction Layer) del código de aplicación

---

## 2. Estructura del Proyecto

```
usdx_plus_orange/
├── usdx_plus_orange.ino          # Archivo principal (entry point)
├── usdx_config.h                 # Configuración unificada del sistema
├── ISP_PROGRAMMING.md            # Guía de programación ISP
├── REFACTORIZACION_STATUS.md     # Estado del refactoring
├── Ft8-menu/                     # Código legacy de referencia
│   ├── usdx_plus_orange.ino
│   └── usdx_settings.h
└── src/                          # Código fuente modular
    ├── hal/                      # Hardware Abstraction Layer
    │   ├── adc.h/cpp             # Control del ADC
    │   ├── gpio.h/cpp            # Control de pines GPIO
    │   ├── i2c.h/cpp             # Comunicación I2C
    │   ├── timer.h/cpp           # Configuración de timers
    │   └── timer_isr.cpp         # ISR del Timer2 (DSP)
    ├── drivers/                  # Controladores de dispositivos
    │   ├── si5351.h/cpp          # Driver del sintetizador SI5351
    │   └── lpf_switch.h/cpp      # Control de filtros LPF
    ├── dsp/                      # Procesamiento de señales
    │   ├── ssb.h/cpp             # Modulación/demodulación SSB
    │   ├── agc.h/cpp             # Control automático de ganancia
    │   ├── nr.h/cpp              # Reducción de ruido
    │   ├── filters.h/cpp         # Filtros de audio
    │   ├── slow_dsp.h/cpp        # Procesamiento lento DSP
    │   └── slow_dsp.cpp          # Funciones I/Q processing
    ├── state/                    # Gestión de estado
    │   ├── state.h               # Variables de estado globales
    │   ├── bands.h/cpp           # Configuración de bandas
    │   ├── vfo.h/cpp             # Control del VFO
    │   └── rx_tx.h/cpp           # Cambio RX/TX
    ├── ui/                       # Interfaz de usuario
    │   ├── display.h/cpp         # Control del display
    │   ├── menu.h/cpp            # Sistema de menús
    │   ├── encoder.h/cpp         # Control del encoder rotativo
    │   ├── smeter.h/cpp          # S-meter
    │   └── ft8_menu.h/cpp        # Menú específico FT8
    ├── cw/                       # CW Keyer y Decoder
    │   ├── keyer.h/cpp           # Keyer CW
    │   └── decoder.h/cpp         # Decoder CW
    └── cat/                      # Interfaz CAT
        └── cat_interface.h/cpp   # Protocolo CAT
```

---

## 3. Características Técnicas

### 3.1 Especificaciones del Hardware

| Parámetro | Valor |
|-----------|-------|
| Microcontrolador | ATmega328P |
| Frecuencia CPU | 27 MHz (cristal externo) |
| Flash disponible | 32 KB |
| RAM disponible | 2 KB |
| Cristal SI5351 | 27 MHz |
| Display | LCD 16x2 paralelo o OLED I2C |

### 3.2 Modos de Operación

| Modo | Descripción | Soporte |
|------|-------------|---------|
| LSB | Lower Sideband | ✅ Completo |
| USB | Upper Sideband | ✅ Completo |
| CW | Telegrafía | ✅ Completo |
| FM | Frecuencia Modulada | ✅ Básico |
| AM | Amplitud Modulada | ✅ Básico |

### 3.3 Parámetros DSP

| Parámetro | Valor |
|-----------|-------|
| Frecuencia de muestreo RX | 192,307 Hz |
| Frecuencia de muestreo TX | 4,800 Hz |
| Frecuencia PWM (Audio) | variable según F_CPU |
| Decimación CIC | 4 |

---

## 4. Compilación

### 4.1 Requisitos

- Arduino IDE 1.8.x o superior
- Arduino AVR Boards 1.8.6
- Librerías necesarias:
  - LiquidCrystal (para LCD paralelo)
  - SSD1306Ascii (para OLED)

### 4.2 Compilar desde Arduino IDE

1. Abrir `usdx_plus_orange.ino`
2. Seleccionar placa: **Arduino Nano**
3. Seleccionar procesador: **ATmega328P**
4. Compilar: **Sketch → Verificar/Compilar** (Ctrl+R)

### 4.3 Compilar desde Línea de Comandos

```bash
# Usando arduino-cli
arduino-cli compile \
  --fqbn arduino:avr:nano:cpu=atmega328 \
  --build-property "compiler.cpp.extra_flags=\"-I/path/to/project\"" \
  usdx_plus_orange.ino
```

### 4.4 Resultado de Compilación

```
El Sketch usa 13340 bytes (43%) del espacio de almacenamiento de programa.
Las variables Globales usan 1012 bytes (49%) de la memoria dinámica.
```

---

## 5. Programación ISP

### 5.1 Conexiones ISP

Para programar el ATmega328P directamente en el circuito (sin bootloader), conectar un programador USBasp o ArduinoISP:

```
Programador → ATmega328P (uSDX)
----------------------------------------
VCC (5V)    → VCC
GND         → GND
SCK (D13)   → SCK
MISO (D12)  → MISO
MOSI (D11)  → MOSI
RESET (D10) → RESET
```

### 5.2 Fuses para 27MHz (sin bootloader)

| Fuse | Valor | Descripción |
|------|-------|-------------|
| Low | 0xFF | Full swing crystal, startup 258CK + 64ms |
| High | 0xD6 | No bootloader, SPI enabled |
| Extended | 0xFD | Brown-out disabled |

### 5.3 Programar con avrdude

```bash
# 1. Configurar fuses
avrdude -c usbasp -p m328p \
  -U lfuse:w:0xFF:m \
  -U hfuse:w:0xD6:m \
  -U efuse:w:0xFD:m

# 2. Programar firmware
avrdude -c usbasp -p m328p \
  -U flash:w:usdx_plus_orange.ino.hex:i
```

### 5.4 Programar desde Arduino IDE

1. Conectar el programador ISP
2. Seleccionar: **Herramientas → Programador → USBasp**
3. Seleccionar: **Herramientas → Placa → Arduino Nano**
4. Seleccionar: **Herramientas → Procesador → ATmega328P**
5. Grabar bootloader (configura los fuses): **Herramientas → Grabar bootloader**
6. Subir firmware: **Sketch → Subir usando programador** (Ctrl+Shift+U)

---

## 6. Arquitectura de Módulos

### 6.1 Capa HAL (Hardware Abstraction Layer)

La capa HAL abstrae el hardware específico del microcontrolador, permitiendo portabilidad:

#### GPIO (`src/hal/gpio.h/cpp`)
```cpp
void gpio_mode(uint8_t pin, uint8_t mode);
void gpio_write(uint8_t pin, uint8_t value);
uint8_t gpio_read(uint8_t pin);
```

#### ADC (`src/hal/adc.h/cpp`)
```cpp
void adc_start(uint8_t adcpin, bool ref1v1, uint32_t fs);
void adc_stop(void);
uint16_t adc_read(void);
```

#### Timer (`src/hal/timer.h/cpp`)
```cpp
void timer1_start(uint32_t fs);
void timer1_stop();
void timer2_start(uint32_t fs);
void timer2_stop();
```

### 6.2 Capa DSP

El procesamiento de señales se divide en:

#### SSB (`src/dsp/ssb.h/cpp`)
- Modulación SSB
- Demodulación SSB con CIC filter
- Funciones `dsp_tx()`, `dsp_tx_cw()`, `dsp_tx_am()`, `dsp_tx_fm()`
- Funciones `sdr_rx_00()` a `sdr_rx_07()` (cadena CIC)

#### AGC (`src/dsp/agc.h/cpp`)
- Control automático de ganancia
- Modos: OFF, Fast, Slow

#### NR (`src/dsp/nr.h/cpp`)
- Reducción de ruido digital

#### Filters (`src/dsp/filters.h/cpp`)
- Filtros de audio variables

### 6.3 Capa de Estado

Gestiona el estado global del sistema:

#### VFO (`src/state/vfo.h/cpp`)
```cpp
void vfo_tune(int8_t direction);
void vfo_step_size(uint8_t step);
void rit_increment(int8_t delta);
```

#### RX/TX (`src/state/rx_tx.h/cpp`)
```cpp
void start_rx();
void switch_rxtx(uint8_t tx_enable);
void tx_enable();
void rx_enable();
```

---

## 7. Configuración

### 7.1 Parámetros Principales (`usdx_config.h`)

```cpp
#define F_MCU 27000000      // Frecuencia del CPU
#define F_XTAL 27000000     // Frecuencia del cristal
#define VERSION "1.03x"     // Versión del firmware
```

### 7.2 Definición de Pines

| Pin | Función | Puerto |
|-----|---------|--------|
| 0 | LCD D4 | PD0 |
| 1 | LCD D5 | PD1 |
| 2 | LCD D6 | PD2 |
| 3 | LCD D7 | PD3 |
| 4 | LCD EN | PD4 |
| 6 | ROT_A | PD6 |
| 7 | ROT_B | PD7 |
| 8 | RX | PB0 |
| 9 | SIDETONE | PB1 |
| 10 | KEY_OUT | PB2 |
| 14 (A0) | AUDIO1 | PC0 |
| 15 (A1) | AUDIO2 | PC1 |
| 16 (A2) | DVM | PC2 |
| 17 (A3) | BUTTONS | PC3 |
| 18 (A4) | SDA | PC4 |
| 19 (A5) | SCL | PC5 |

### 7.3 Bandas de Frecuencia

| Banda | Frecuencia (kHz) |
|-------|------------------|
| 160m | 1840 |
| 80m | 3573 |
| 60m | 5357 |
| 40m | 7074 |
| 30m | 10136 |
| 20m | 14074 |
| 17m | 18100 |
| 15m | 21074 |
| 12m | 24915 |
| 10m | 28074 |
| 6m | 50313 |

---

## 8. Uso del Sistema

### 8.1 Control del Encoder

- **Rotación**: Cambia frecuencia o valor del menú
- **Pulsación corta**: Entra/sale del menú
- **Pulsación larga**: Funciones especiales según el modo

### 8.2 Menú Principal

| Item | Función |
|------|---------|
| VOL | Volumen |
| MODE | Modo (LSB/USB/CW/FM/AM) |
| FILT | Ancho de filtro |
| BAND | Selección de banda |
| STEP | Paso de sintonía |
| RIT | Offset RIT |
| AGC | Modo AGC |
| NR | Reducción de ruido |
| ATT | Atenuador |
| DRIVE | Potencia de transmisión |
| CWxxx | Configuración CW |

### 8.3 Modo FT8

El firmware incluye un modo específico para FT8:
- Cambio automático de frecuencia por banda
- Configuración optimizada para FT8
- Decodificador CW activo en modo CW

---

## 9. Solución de Problemas

### 9.1 El display no muestra nada

1. Verificar conexiones del LCD
2. Verificar contraste del LCD
3. Comprobar voltaje de alimentación (5V)

### 9.2 No hay audio en recepción

1. Verificar conexión del altavoz
2. Comprobar volumen (`VOL` en menú)
3. Verificar configuración del SI5351

### 9.3 No transmite

1. Verificar conexión del MIC
2. Comprobar configuración de modo (no funciona en FM/AM básico)
3. Verificar alimentación del PA

### 9.4 Frecuencia incorrecta

1. Verificar configuración de F_XTAL (27MHz)
2. Comprobar cristales del SI5351
3. Verificar calibración IQ

### 9.5 El micro no responde tras programación ISP

1. Verificar conexiones ISP
2. Comprobar voltaje (5V)
3. Verificar que el cristal funcione
4. Intentar programar con velocidad reducida: `-B 5`

---

## 10. Historial de Cambios

### v1.0 (Modular)
- Versión inicial con arquitectura modular
- Tamaño de código: ~13.3 KB
- División en módulos HAL, DSP, State, UI, CW, CAT

### v1.03x (Legacy original)
- Código monolítico original
- Tamaño: ~28 KB (excedía límites)

---

## 11. Referencias

- [Repositorio original uSDX](https://github.com/pe1nnz/ amateur-radio-projects)
- [Documentación SI5351](https://www.silabs.com/documents/public/data-sheets/Si5351-B.pdf)
- [Hoja de datos ATmega328P](http://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-7810-Automotive-Microcontrollers-ATmega328P_Datasheet.pdf)

---

## 12. Licencia

Este proyecto está basado en el código original de Guido PE1NNZ (QCX-SSB) y mantiene la licencia MIT.

---

**Autor:** EA7LJY  
**Versión:** 1.0 (Modular)  
**Fecha:** Enero 2026
