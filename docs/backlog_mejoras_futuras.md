# Backlog de Mejoras Futuras - uSDX Plus Orange

## Resumen

Este documento enumera posibles mejoras para el firmware uSDX Plus Orange basadas en análisis de proyectos activos como:
- **(tr)uSDX** de DL2MAN - https://dl2man.de/3b-trusdx-firmware/
- **uSDXOpen** de GW8RDI - https://github.com/GW8RDI/uSDXOpen
- **uSDX+** de coffee dev - https://github.com/coffeedev/usdx_plus
- **QCX-SSB** de threeme3 - https://github.com/threeme3/usdx

---

## Mejoras de Transmisión (TX)

### TX-01: Mejora de Calidad de Voz SSB

| Aspecto | Actual | Propuesto | Prioridad |
|---------|--------|-----------|-----------|
| IMD | Regular | Mejorado | ALTA |
| Spectral purity | Limpia | Muy limpia | ALTA |

**Implementación basada en uSDXOpen:**
- Corregir "quadrature flipping" que causa distorsión
- Mejora en el procesamiento de audio del micrófono
- Ajuste fino del compressor/limiter

**Referencias:**
- https://f5npv.wordpress.com/2024/04/07/usdx-v1-02-enhancement-april-2024/
- https://github.com/GW8RDI/uSDXOpen

### TX-02: Control Automático de Potencia (APC)

```cpp
// Objetivo: Mantener potencia constante independientemente de la frecuencia
#define APC_ENABLE 1
#define APC_TARGET_POWER 3000  // mW (3W)

// Monitorear VSWR y ajustar drive automáticamente
inline uint16_t apply_apc(uint16_t current_amp, int16_t swr) {
  if(swr > 3){  // SWR > 3: reducir potencia
    return current_amp * (3.0 / swr);
  }
  return current_amp;
}
```

### TX-03: Speech Processor (DBX-style)

```cpp
// Compresor DBX-style para mejor inteligibilidad
#define SPEECH_PROCESSOR_ENABLE 1

static int16_t sp_gain = 256;
static int16_t sp_level = 0;

inline int16_t speech_process(int16_t in) {
  int16_t abs_in = abs(in);
  sp_level = (sp_level * 31 + abs_in) >> 5;  // Nivel promedio

  // Gain reduction basada en nivel
  if(sp_level > 200){
    int16_t reduction = (sp_level - 200) / 8;
    sp_gain = 256 - (reduction << 4);
    if(sp_gain < 64) sp_gain = 64;
  } else {
    sp_gain = 256;
  }

  return (in * sp_gain) >> 8;
}
```

---

## Mejoras de Recepción (RX)

### RX-01: Spectrum Display

| Aspecto | Valor |
|---------|-------|
| FFT Size | 64-128 puntos |
| Display | LCD 128x64 OLED |
| Ancho | ~3 kHz |

**Implementación:**
```cpp
// Spectrum waterfall para LCD/OLED
#define SPECTRUM_ENABLE 1
#define FFT_SIZE 64

static int16_t spectrum_buffer[FFT_SIZE];

void process_spectrum() {
  // Tomar muestras y calcular FFT
  // Mostrar en display como barra de espectro
}
```

**Referencia:** uSDXOpen incluye módulo DSP con spectrum

### RX-02: AGC Mejorada con Delay Variable

```cpp
// AGC con delay según modo de operación
#define AGC_MODE_VOX 0  // Fast attack, fast decay
#define AGC_MODE_CW 1   // Medium attack, slow decay
#define AGC_MODE_SSB 2  // Slow attack, very slow decay

static uint8_t agc_mode = AGC_MODE_SSB;

inline int16_t process_agc_variable(int16_t in) {
  static int16_t gain = 128;
  int16_t abs_in = abs(in);

  // Parámetros según modo
  uint8_t attack_rate, decay_rate;
  switch(agc_mode){
    case AGC_MODE_VOX:
      attack_rate = 4; decay_rate = 8; break;
    case AGC_MODE_CW:
      attack_rate = 3; decay_rate = 16; break;
    case AGC_MODE_SSB:
    default:
      attack_rate = 2; decay_rate = 24; break;
  }

  // Algoritmo AGC...
}
```

### RX-03: Filtros Variables Más Flexibles

| Filtro | Actual | Propuesto |
|--------|--------|-----------|
| SSB | Fijo (4 opciones) | Variable continuo |
| CW | Fijo (3 opciones) | Variable continuo |

```cpp
// Filtro pasabanda variable
#define VAR_FILTER_ENABLE 1

// Parámetros ajustables por menú
volatile uint16_t var_filter_low = 300;   // Hz
volatile uint16_t var_filter_high = 2700; // Hz

inline int16_t apply_var_filter(int16_t in) {
  // FIR variable basado en parámetros
}
```

---

## Mejoras de Modos Digitales

### DIG-01: Audio Over USB (FT8/JS8/FT4)

| Aspecto | Valor |
|---------|-------|
| USB Audio | Stereo 48kHz |
| CAT | HID emulation |

**Basado en (tr)uSDX v2.00u+:**
- Audio integrado por USB (no requiere cables de audio)
- Interfaz más limpia para operación portable
- Compatible con WSJT-X, JS8Call

**Implementación:**
```cpp
#define USB_AUDIO_ENABLE 1
#define USB_AUDIO_SAMPLE_RATE 48000

// Usar USB HID para audio (requiere librería externa)
// Alternativa: Virtual COM port con audio codec
```

**Referencia:** https://n1ugk.com/2023/06/trusdx-ft8-without-audio-cables/

### DIG-02: CAT Extendido para Digital

| Comando | Función |
|---------|---------|
| `FT1;` | Seleccionar modo digital |
| `AI0;` | Auto-info para WSJT-X |
| `RX0;` | Enable RX para digital |

---

## Mejoras de Interfaz de Usuario

### UI-01: Menú Mejorado

| Feature | Descripción |
|---------|-------------|
| Menú cíclico | No ir al inicio al final |
| Bandas bidireccionales | Girar encoder para cambiar banda |
| Quick access | Doble click para funciones comunes |

**Implementación basada en uSDXOpen:**
```cpp
// Cambiar banda con doble click y dirección
if(encoder_double_click){
  if(encoder_delta > 0) band_change_direction = +1;
  else band_change_direction = -1;
  change_band(band_change_direction);
}

// Menú cíclico
if(menu_position > menu_max) menu_position = 0;
```

### UI-02: Custom LCD Characters

```cpp
// Caracteres personalizados para display
const uint8_t smeter_bar[] PROGMEM = {
  0b00000,  // Vacío
  0b00100,  // 25%
  0b01110,  // 50%
  0b11111,  // 75%
  0b11111,  // 100%
};

#define CHAR_BAR_EMPTY 0
#define CHAR_BAR_25 1
#define CHAR_BAR_50 2
#define CHAR_BAR_75 3
#define CHAR_BAR_FULL 4
```

### UI-03: Visualización de Frecuencia Mejorada

```
Pantalla actual:  14.074.000
Propuesta:        14.074 MHz
                  +1.0 kHz RIT
```

---

## Mejoras de Memoria y Rendimiento

### MEM-01: Eliminar Código Duplicado

| Sección | Tamaño | Acción |
|---------|--------|--------|
| OLD_CW | ~800 bytes | Eliminar (NEW_CW ya está activo) |
| Funciones no usadas | ~500 bytes | Comment out |
| Filtros redundantes | ~200 bytes | Consolidar |

### MEM-02: Optimización de SRAM

```cpp
// Usar F() para strings constantes
lcd.print(F("uSDX Plus Orange"));  // En lugar de lcd.print("uSDX...")

// Mover tablas grandes a PROGMEM
const uint16_t sin_table[256] PROGMEM = { ... };
```

---

## Mejoras de Hardware (Opcional)

### HW-01: Soporte para más bandas

| Banda | Frecuencia | Notas |
|-------|------------|-------|
| 160m | 1.8-2.0 MHz | Requiere filtro paso bajo |
| 60m | 5 MHz | Frecuencias limitadas |
| 6m | 50-54 MHz | Requiere preamplificador |

### HW-02: LCD/TFT Touch

```
Display: 2.8" TFT Touch (SPI)
Resolución: 320x240
Interface: SPI (comparte pines con LCD)
```

### HW-03: Batería Integrada

```
Monitor de batería:
- Medición de voltaje (ADC)
- Indicador de carga
- Alerta de batería baja
```

---

## Resumen de Prioridades

### ALTA Prioridad

| ID | Mejora | Dificultad | Impacto |
|----|--------|------------|---------|
| TX-01 | Calidad de voz SSB | Media | Alto |
| RX-02 | AGC variable | Baja | Alto |
| DIG-01 | Audio USB | Alta | Alto |

### MEDIA Prioridad

| ID | Mejora | Dificultad | Impacto |
|----|--------|------------|---------|
| TX-02 | APC | Media | Medio |
| RX-01 | Spectrum | Alta | Medio |
| UI-01 | Menú mejorado | Baja | Medio |

### BAJA Prioridad

| ID | Mejora | Dificultad | Impacto |
|----|--------|------------|---------|
| UI-02 | Custom chars | Baja | Bajo |
| HW-02 | Touch display | Alta | Bajo |
| HW-03 | Batería | Media | Bajo |

---

## Referencias

1. **(tr)uSDX:** https://dl2man.de/3b-trusdx-firmware/
2. **uSDXOpen:** https://github.com/GW8RDI/uSDXOpen
3. **QCX-SSB Original:** https://github.com/threeme3/usdx
4. **F5NPV Mejoras:** https://f5npv.wordpress.com/2024/04/07/usdx-v1-02-enhancement-april-2024/
5. **FT8 USB Audio:** https://n1ugk.com/2023/06/trusdx-ft8-without-audio-cables/

---

*Documento generado: 10 de enero de 2026*
*Basado en investigación de proyectos activos uSDX*
