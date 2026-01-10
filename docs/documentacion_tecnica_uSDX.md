# Documentación Técnica del Proyecto uSDX Plus Orange

## 1. Descripción General del Proyecto

### 1.1 ¿Qué es uSDX?

**uSDX** (micro Software Defined Transceiver) es un transceiver QRP SSB/CW experimental desarrollado por **Guido PE1NNZ**. El proyecto implementa un transceiver de radioaficionado completamente funcional utilizando técnicas SDR (Software Defined Radio) en un microcontrolador ATMEGA328P.

**Características principales:**
- Transmisión SSB completamente digital y basada en software
- Receptor SDR con procesamiento DSP
- Modo Clase-E para transmisión eficiente
- Aproximadamente 5W PEP de salida SSB
- Multibanda (160m-10m)
- Modos: USB, LSB, CW, AM, FM

### 1.2 Estructura del Proyecto

```
/home/jgalera/develop/arduino/usdx/usdx_plus_orange/
|
|-- usdx_plus_orange.ino        (236 KB, 5,848 líneas) - Versión 1.03x
|-- usdx_settings.h             (Configuración principal)
|-- usdx_filter.h               (Filtros DSP)
|-- README.md                   (Documentación original)
|-- prompt.txt
|-- .gitignore
|-- .git/
|-- .idea/
|-- docs/                       (Documentación)
|-- usdx-lagazy/                (Versión legacy de referencia)
|   |-- usdx.ino                (Versión 1.02x)
|   |-- usdx_settings.h
|   |-- usdx_filter.h
|   |-- README.md
|   |-- ina219.h
|   |-- imágenes (top.png, block.png, usdx.png, twotone.png)
```

### 1.3 Versiones del Firmware

| Aspecto | usdx-lagazy (v1.02x) | usdx_plus_orange (v1.03x) |
|---------|---------------------|---------------------------|
| **Versión firmware** | 1.02x | 1.03x |
| **Tamaño archivo** | 236 KB | 236 KB |
| **Líneas de código** | ~5,800 | ~5,848 |
| **Indicativo** | PE1NNZ (original) | EA7LJY (personalizado) |
| **Nombre operador** | UNKNOWN | JULIAN |

---

## 2. Arquitectura del Hardware

### 2.1 Componentes Principales

El hardware de uSDX se basa en una arquitectura simple con solo 4 CI principales:

| Componente | Función |
|------------|---------|
| **ATMEGA328P** | Microcontrolador principal, procesamiento DSP, control |
| **SI5351** | Generador de reloj, sintetizador de frecuencia, modulación |
| **Transistores MOSFET PA** | Etapa de potencia de transmisión (Class-E) |
| **HD44780 LCD** | Display de usuario (1602 o 2004) |

### 2.2 Diagrama de Bloques del Sistema

```
                    +------------------+
                    |   MICROFONO      |
                    | (Electret + PTT) |
                    +--------+---------+
                             |
                    +--------v---------+
                    |   ATMEGA328P     |
                    |   (ADC - Audio)  |
                    +--------+---------+
                             |
              +--------------v---------------+
              |         PROCESO SSB          |
              |  - Hilbert Transform         |
              |  - Phase/Amplitude Extract   |
              |  - Phase Modulation -> SI5351|
              |  - PWM Envelope -> PA        |
              +---+------------------+-------+
                  |                  |
    +-------------v---+    +--------v---------+
    |    SI5351       |    |   PA Class-E     |
    | - CLK0/CLK1     |    |   (MOSFETs)      |
    | - Generador RF  |    +--------+---------+
    +--------+--------+             |
             |                      |
    +--------v--------+    +--------v---------+
    |  TAYLOE QSD     |    |   LOW PASS       |
    |  (Quadrature    |    |   FILTER         |
    |   Detector)     |    +--------+---------+
    +--------+--------+             |
             |                      |
    +--------v--------+    +--------v---------+
    |   ATMEGA328P    |    |    ANTENA        |
    |   (ADC - I/Q)   |    +------------------+
    |   - DSP RX      |
    |   - Filters     |
    |   - AGC/NR      |
    +---+------------+
        |
   +----v-----+
   | DISPLAY  |
   | LCD/OLED |
   +----------+
```

### 2.3 Definición de Pines (ATMEGA328P)

```c
#define LCD_D4   0  // PD0
#define LCD_D5   1  // PD1
#define LCD_D6   2  // PD2
#define LCD_D7   3  // PD3
#define LCD_EN   4  // PD4
#define FREQCNT  5  // PD5
#define ROT_A    6  // PD6
#define ROT_B    7  // PD7
#define RX       8  // PB0
#define SIDETONE 9  // PB1
#define KEY_OUT 10  // PB2
#define SIG_OUT 11  // PB3
#define DAH     12  // PB4
#define DIT     13  // PB5
#define AUDIO1  14  // PC0/A0 (ADC0 - I/Q input)
#define AUDIO2  15  // PC1/A1 (ADC1 - I/Q input)
#define DVM     16  // PC2/A2 (ADC2 - Mic input)
#define BUTTONS 17  // PC3/A3 (ADC3 - Buttons)
#define LCD_RS  18  // PC4 (SDA I2C)
#define SCL     19  // PC5 (SCL I2C)
```

### 2.4 Parámetros Técnicos Clave

| Parámetro | Valor |
|-----------|-------|
| Frecuencia cristal MCU | 20 MHz (F_MCU = 20000000) |
| Frecuencia cristal SI5351 | 27 MHz (F_XTAL = 27000000) |
| Frecuencia muestreo RX | 62.5 kHz (F_SAMP_RX = 62500) |
| Frecuencia muestreo TX | 19.531 Hz (F_SAMP_TX = 78125) |
| Frecuencia PWM | 32 kHz |
| Resolución ADC | 10 bits |
| Salida SSB | ~5W PEP @ 13.8V |
| Ancho banda SSB | 2400 Hz |
| Rango dinámico RX | ~72 dB teórico |

---

## 3. Arquitectura del Firmware

### 3.1 Estructura del Código (usdx_plus_orange.ino)

El archivo principal `usdx_plus_orange.ino` tiene aproximadamente 5,848 líneas organizadas en las siguientes secciones:

```
Líneas 1-200:     Includes y definiciones de versión
Líneas 14-125:    Definiciones de pines
Líneas 180-300:   Keyer CW (Iambic A/B)
Líneas 234-311:   Clase I2C secundaria (Wire)
Líneas 318-526:   Clase LCD personalizada
Líneas 662-924:   Fuentes para display
Líneas 967-1200:  Clase OLEDDevice
Líneas 1125-1231: ISR del encoder rotatorio (PCINT2_vect)
Líneas 1236-1366: Clase I2C principal
Líneas 1376-1641: Clase SI5351
Líneas 1700-2400: Funciones CW decoder
Líneas 2500-2800: Algoritmos AGC y Noise Reduction
Líneas 2840-3100: Funciones DSP de recepción (CIC filter, Hilbert)
Líneas 3300-3600: ISR de transmisión (TIMER2_COMPA_vect)
Líneas 3700-4000: Funciones de control TX/RX
Líneas 4000-4300: Funciones de menú y configuración
Líneas 4300-4800: Funciones EEPROM
Líneas 4836-5100: Función setup()
Líneas 5136-5848: Función loop()
```

### 3.2 Clases Principales

#### 3.2.1 Clase I2C (Bit-banging personalizado)

```cpp
class I2C {
public:
  #define I2C_DELAY   6  // 598 kbps a 20MHz
  #define I2C_SDA (1 << 4)  // PC4
  #define I2C_SCL (1 << 5)  // PC5
  
  void beginTransmission(uint8_t addr);
  bool write(uint8_t byte);
  uint8_t endTransmission();
};
```

**Características:**
- Implementación bit-banging para control preciso de timing
- Velocidad hasta 800 kbit/s
- Soporte para comunicaciones SI5351 y expansores GPIO

#### 3.2.2 Clase SI5351

```cpp
class SI5351 {
public:
  volatile int32_t _fout;
  volatile uint8_t _div;
  volatile uint32_t _msb128;
  
  void freq(int32_t fout, uint16_t i, uint16_t q);
  void SendRegister(uint8_t reg, uint8_t* data, uint8_t n);
  void powerDown();
};
```

**Funciones clave:**
- `freq()`: Configura frecuencias de CLK0/CLK1/CLK2
- `freq_calc_fast()`: Cálculo rápido de parámetros PLL
- `ms()`: Configuración de Multisynth
- `reset()`: Reset del PLL

#### 3.2.3 Clase LCD

Implementación optimizada para displays HD44780 con:
- Control de retroiluminación
- Gestión de conflictos con puerto serie
- Caracteres personalizados para iconos (S-meter, VFO A/B)

#### 3.2.4 Clase OLEDDevice

Soporte para displays OLED SSD1306 y SH1106:
- Resolución 128x32 o 128x64
- Interfaz I2C a 0x3C
- Fuentes personalizadas

### 3.3 Funciones de Interrupción (ISR)

```cpp
// Encoder rotatorio
ISR(PCINT2_vect);

// Timer2 para muestreo y procesamiento
ISR(TIMER2_COMPA_vect);
```

---

## 4. Procesamiento de Señal Digital (DSP)

### 4.1 Cadena de Recepción (RX)

```
Antena -> QSD (Tayloe) -> ADC (I/Q) -> CIC Filter -> Hilbert -> AGC -> Filters -> Audio
                                                              |
                                                              v
                                                          CW Decoder
```

### 4.2 Filtros CIC (Cascade Integrator-Comb)

```c
// Filtro CIC de orden 3 para decimación 8:1
#define R 4  // Factor de decimación

// Primera etapa CIC (orden 2)
int16_t i_s1za0 = (ac + (i_s0za1 + i_s0zb0) * 3 + i_s0zb1) >> M_SR;

// Segunda etapa CIC (orden 3)
int16_t ac2 = (i_s1za0 + (i_s1za1 + i_s1zb0) * 3 + i_s1zb1);
```

### 4.3 Transformada de Hilbert

Implementada como filtro FIR de 15 taps para shift de 90 grados:

```c
qh = ((v[0] - q_ac2) + (v[2] - v[12]) * 4) / 64 
   + ((v[4] - v[10]) + (v[6] - v[8])) / 8 
   + ((v[4] - v[10]) * 5 - (v[6] - v[8])) / 128 
   + (v[6] - v[8]) / 2;
```

**Proporciona 43 dB de rechazo de banda lateral** en el rango 650-3400 Hz.

### 4.4 Filtros de Audio (usdx_filter.h)

| Filtro | Modo | Ancho de banda |
|--------|------|----------------|
| 0 | SSB Full | 0-4000 Hz |
| 1 | SSB Wide | 0-2900 Hz |
| 2 | SSB Normal | 0-2400 Hz |
| 3 | SSB Narrow | 0-1800 Hz |
| 4-7 | CW | 500-1000 Hz a 630-680 Hz |

### 4.5 AGC (Automatic Gain Control)

**AGC Rápida (original):**
```c
static int16_t gain = 1024;
inline int16_t process_agc_fast(int16_t in);
```
Rango: x1 a x31 (~30 dB)

**AGC Nueva (M0PUB):**
```c
static int16_t centiGain = 128;  // Representa ganancia x 128
inline int16_t process_agc(int16_t in);
```
Rango: 0.25:255 (~60 dB)

### 4.6 Noise Reduction

```c
inline int16_t process_nr(int16_t in) {
  static int16_t ea1;
  ea1 = EA(ea1, in, 1 << (nr-1));  // Moving average exponencial
  return ea1;
}
```

---

## 5. Generación de Señal SSB (TX)

### 5.1 Proceso de Modulación

```
Microfono -> ADC -> Hilbert -> Phase/Amplitude -> SI5351 (Phase) + PWM (Amplitude)
                                                    |
                                                    v
                                          Señal SSB + PA Class-E
```

### 5.2 Algoritmo de Generación SSB

```c
// Extracción de fase y amplitud
int16_t ph = _arctan3(q, i);  // Phase
int16_t mag = magn(i, q);     // Magnitude

// Restricción de cambio de fase
if(ph > MAX_DP) ph = MAX_DP;
if(ph < -MAX_DP) ph = -MAX_DP;

// Modulación de frecuencia (phase)
int16_t df = ph * 15;

// Modulación de amplitud (PWM)
uint8_t amp = mag >> 5;
```

### 5.3 Parámetros Clave de TX

| Parámetro | Valor |
|-----------|-------|
| Frecuencia muestreo audio | 19.53125 kHz |
| Frecuencia actualización SI5351 | 4800 veces/segundo |
| Resolución PWM | 8 bits (256 niveles) |
| Ancho banda SSB | 2400 Hz |

---

## 6. Modos de Operación

### 6.1 Modos Soportados

| Modo | Recepción | Transmisión |
|------|-----------|-------------|
| **LSB** | SDR phasing | SSB polar |
| **USB** | SDR phasing | SSB polar |
| **CW** | SDR + CW filter | CW keying |
| **AM** | Magnitude detect | AM (experimental) |
| **FM** | Product detector | FM (experimental) |

### 6.2 Configuración de Frecuencias

```c
static int32_t vfo[] = { 7074000, 14074000 };
static uint8_t vfomode[] = { USB, USB };
volatile int32_t freq = 14000000;
```

**Bandas predefinidas:** 80m, 60m, 40m, 30m, 20m, 17m, 15m, 12m, 10m, 6m

### 6.3 Funcionalidades de CW

- **Keyer**: Iambic A/B y Straight key
- **Decoder**: Decodificador Morse integrado
- **Mensajes**: Hasta 6 mensajes preprogramados
- **Semi-QSK**: Silenciamiento RX durante envío

---

## 7. Configuración y Parámetros

### 7.1 Opciones de Compilación (usdx_settings.h)

```c
// Funcionalidades básicas
#define DIAG              1   // Diagnósticos hardware
#define KEYER             1   // Keyer CW
#define CAT               1   // Interface CAT
#define CW_DECODER        1   // Decoder Morse

// Hardware
#define F_XTAL       27000000   // Cristal SI5351
#define SI5351_ADDR     0x60   // Dirección I2C

// Funcionalidades avanzadas
#define LPF_SWITCHING_DL2MAN_USDX_REV3  1   // 8 filtros de banda
#define TX_ENABLE        1   // Habilitar transmisión
#define VOX_ENABLE       1   // Voice-operated TX
#define RIT_ENABLE       1   // Receive In Transit
#define SEMI_QSK         1   // Semi QSK operation
#define KEY_CLICK        1   // Reducción de clicks
#define CW_MESSAGE       1   // Mensajes CW
#define CW_INTERMEDIATE  1   // Caracteres intermedios
```

### 7.2 Parámetros EEPROM

La configuración se almacena en EEPROM del ATMEGA328P:
- Frecuencias VFO A/B
- Modos de operación
- Parámetros de audio
- Calibraciones

### 7.3 Control de Menú

| Función | Botón | Acción |
|---------|-------|--------|
| Volumen | E + giro | Control volumen |
| Menú | L | Entrar menú |
| Menú atrás | R | Salir menú |
| Modo | R simple | Cambiar modo |
| Filtro | R doble | Ancho de banda |
| Banda | E doble | Cambiar banda |
| RIT | R largo | Activar RIT |
| Reset | E largo | Factory reset |

---

## 8. Interfaz CAT (Computer Aided Transceiver)

### 8.1 Protocolo

Implementa subconjunto del protocolo **Kenwood TS-480**:

| Comando | Función |
|---------|---------|
| `FA;` | Frecuencia VFO A |
| `FB;` | Frecuencia VFO B |
| `MD;` | Modo de operación |
| `TX;` | Forzar TX |
| `RX;` | Forzar RX |
| `AI;` | Auto Inform |
| `VS;` | VFO Split |

### 8.2 Extensiones CAT

```c
#define CAT_EXT        1   // Control remoto de pantalla
#define CAT_STREAMING  1   // Streaming de audio
```

---

## 9. Rendimiento y Optimizaciones

### 9.1 Estimación de Carga de CPU

| Función | Carga típica |
|---------|-------------|
| TX (SSB) | ~45-55% |
| RX (SDR) | ~65-75% |
| LCD update | ~5-10% |

### 9.2 Tamaño de Código

Aproximaciones según gcc 7.3.0:

| Función | Tamaño |
|---------|---------|
| DIAG | 1,308 bytes |
| CAT | 4,150 bytes |
| CW_DECODER | 1,468 bytes |
| FAST_AGC | 700 bytes |
| SWR_METER | 1,724 bytes |

### 9.3 Optimizaciones Aplicadas

- **`#pragma GCC optimize ("Ofast")`** en funciones críticas de DSP
- **Macros inline** para funciones pequeñas
- **Variables volátiles** para comunicación ISR/main
- **Lookup tables** para funciones trigonométricas

---

## 10. Dependencias y Librerías

### 10.1 Librerías AVR Standard

```cpp
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
```

### 10.2 Funciones Arduino Utilizadas

| Función | Propósito |
|---------|-----------|
| `analogRead()` | Lectura de ADC |
| `digitalRead/Write()` | Control de pines |
| `pinMode()` | Configuración de pines |
| `delay()/millis()` | Temporización |
| `Serial` | Comunicación CAT |

### 10.3 Sin Librerías Externas

El proyecto **no utiliza librerías externas de Arduino**. Toda la funcionalidad está implementada directamente en el código fuente para maximizar eficiencia y minimizar tamaño.

---

## 11. Variables Volátiles Clave

```c
volatile bool tx = false;           // Estado TX/RX
volatile uint8_t mode = USB;        // Modo actual
volatile int32_t freq = 14000000;   // Frecuencia
volatile uint8_t agc = 1;           // AGC enabled
volatile uint8_t nr = 0;            // Noise reduction
volatile uint8_t att = 0;           // Atenuador
volatile uint8_t volume = 8;        // Volumen
volatile int16_t i, q;              // Muestras I/Q
volatile func_t func_ptr;           // Puntero función DSP
```

---

## 12. Características Destacables

### 12.1 Innovaciones Técnicas

1. **SSB completamente digital**: Generación de SSB modulando fase y amplitud del PLL
2. **EER (Envelope Elimination and Restoration)**: Técnica de eficiencia para PA
3. **Hilbert transform en software**: 90 grados exactos sin ajustes analógicos
4. **CIC filters para decimación**: Alta resolución sin costo computacional excesivo

### 12.2 Ventajas del Diseño

- **Simplicidad hardware**: Solo 4 CI principales
- **Eficiencia energética**: Clase-E no requiere disipador
- **Flexibilidad**: Cambios vía software
- **Costo bajo**: Componentes comunes
- **Open Source**: Modificaciones permitidas

### 12.3 Limitaciones

- Requiere calibración precisa
- Rango dinámico limitado por ADC de 10 bits
- CPU trabaja cerca del límite en algunas configuraciones
- Sensible a interferencias EMI

---

## 13. Referencias Técnicas

### 13.1 Componentes Principales

| Componente | Datasheet |
|------------|-----------|
| ATMEGA328P | Microchip DS40002061A |
| SI5351 | Silicon Labs AN619 |
| HD44780 | SparkFun LCD-00709 |
| BS170 MOSFET | ON Semiconductor |

### 13.2 Referencias de Diseño

- **NorCal 2030** (Dan Tayloe, N7VE) - Detector de cuadratura
- **Ghetto Class-E** (Paul Harden, NA5N) - Amplificador
- **QCX** (Hans Summers, G0UPL) - Plataforma base

---

## 14. Información de Versión

| Campo | Valor |
|-------|-------|
| **Proyecto** | uSDX Plus Orange |
| **Versión** | 1.03x |
| **Indicativo** | EA7LJY |
| **Operador** | JULIAN |
| **Autor original** | Guido PE1NNZ |
| **Fecha del documento** | 10 de enero de 2026 |

---

## 15. Compilación del Proyecto

Para compilar el firmware en Arduino IDE:

1. Seleccionar placa: **Arduino Uno**
2. Procesador: **ATMEGA328P**
3. Frecuencia: **16 MHz** (el cristal del MCU es de 20MHz pero el bootloader estándar es a 16MHz)
4. Puerto serie: Configurar según conexión
5. Subir sketch directamente

**Nota:** El firmware puede compilarse sin bootloader para mayor espacio disponible.

---

*Documento generado automáticamente para servir como referencia técnica del proyecto uSDX Plus Orange.*
