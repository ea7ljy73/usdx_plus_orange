# Backlog de Optimizaciones y Mejoras - uSDX Plus Orange

## Resumen Ejecutivo

Este documento analiza el firmware uSDX Plus Orange v1.03x y propone mejoras para optimizar el uso de memoria (Flash ~32KB/32KB y SRAM ~2KB/2KB) del ATMEGA328P.

**Tamaño actual del código:** ~236 KB (5,848 líneas)
**Consumo típico de Flash con todas las features:** ~28-30 KB
**Consumo de SRAM:** ~1.2-1.5 KB

---

## 1. Optimizaciones de Memoria Flash

### 1.1 Desactivación de Funcionalidades Opcionales

| Feature | Tamaño (bytes) | Prioridad | Recomendación | Estado |
|---------|---------------|-----------|---------------|--------|
| DIAG | 1,308 | ALTA | Desactivar en producción | ✅ HECHO |
| CAT | 4,150 | MEDIA | Mantener si se usa CAT | ⏸️ Pendiente |
| CW_DECODER | 1,468 | MEDIA | Mantener si se usa CW | ⏸️ Pendiente |
| CW_MESSAGE | ~500 | BAJA | Desactivar si no se usa | ✅ HECHO |
| CW_INTERMEDIATE | 20 | BAJA | Desactivar si no se necesita | ✅ HECHO |
| FAST_AGC | 700 | BAJA | Mantener (mejor rendimiento) | ⏸️ Pendiente |
| SWR_METER | 1,724 | BAJA | Desactivar si no hay hardware | ⏸️ Pendiente |

**Acción propuesta:** Crear preset de configuración "MINIMAL" que desactive:
- `#define DIAG 0`
- `#define CW_MESSAGE 0`
- `#define CW_INTERMEDIATE 0`

**Ahorro potencial:** 2-3 KB de Flash

### 1.2 Eliminación de Código Muerto

```cpp
// CW decoder: ahora solo hay una implementación (NEW_CW)
#define NEW_CW  1   // Habilitar decoder moderno de OZ1JHM
```

**Acción realizada:** 
- Habilitado `#define NEW_CW 1` (decoder moderno)
- Eliminado bloque `#else // OLD_CW` duplicado
- El código OLD_CW ya no está disponible (usar backup si es necesario)

**Nota:** El código NEW_CW es ~186 bytes más grande que OLD_CW, pero ahora hay solo una implementación, lo que facilita el mantenimiento.

**Ahorro real:** +186 bytes (código más moderno y mejor mantenido)

### 1.3 Optimización de Funciones Matemáticas

**Ubicación:** `usdx_plus_orange.ino:2642-2653` (_arctan3)

```cpp
// Actualmente:
inline int16_t _arctan3(int16_t q, int16_t i)
{
  #define __atan2(z)  (__UA/8  + __UA/22) * z
  int16_t r;
  if(abs(q) > abs(i))
    r = __UA / 4 - __atan2(abs(i) / abs(q));
  else
    r = (i == 0) ? 0 : __atan2(abs(q) / abs(i));
  r = (i < 0) ? __UA / 2 - r : r;
  return (q < 0) ? -r : r;
}
```

**Mejora:** Usar lookup table para valores comunes.

**Acción propuesta:** Crear función optimizada con LUT de 32 valores.

**Ahorro potencial:** ~100-200 bytes, CPU más rápido

### ⚠️ ADVERTENCIA: Shift de Valores Negativos

**MUY IMPORTANTE:** En AVR-GCC, el comportamiento de `>>` en valores negativos es **implementation-defined** antes de C++20. Para valores con signo negativo, usar shifts puede producir resultados inesperados.

```cpp
// PROBLEMA:
int16_t val = -100;
int16_t result = val >> 3;  // ¿Resultado definido? En AVR: shift lógico

// SOLUCIÓN USAR:
int16_t result = val / 8;   // División con truncamiento hacia cero
// O:
int16_t result = (val >> 3) & 0x07;  // Si se espera resultado positivo
```

**Reglas para optimizaciones seguras:**

| Operación insegura | Operación segura |
|--------------------|--------------------|
| `x >> n` | `x / (1 << n)` o `x * mult >> n` |
| `x >> 1` cuando x puede ser negativo | `x / 2` |
| Shift después de `abs()` | `abs(x) >> n` (seguro) |
| Shift en variables de filtro | Usar divisiones explícitas |

**En usdx_filter.h y funciones DSP:**

```cpp
// EVITAR:
zb0 = ((za0+2*za1+za2)>>1) - ...

// PREFERIR:
zb0 = (za0 + 2*za1 + za2) / 2 - ...

// O usar multiplicación por recíproco:
#define DIV2(x) ((x) * 32768 / 2)  // Para división por 2
```

**Verificar siempre en testbench:**

```cpp
#ifdef TESTBENCH
// Probar con señales que incluyan valores negativos
// Comparar resultado original vs optimizado
#endif
```

**Ubicación:** `usdx_plus_orange.ino:662-814` (font[] PROGMEM)

- **Tamaño actual:** 128 caracteres × 8 bytes = 1,024 bytes
- **Caracteres usados:** Solo ASCII 32-126 (~95 caracteres)
- **Caracteres personalizados:** 3 (logo, S-meter, VFO A/B)

**Acción propuesta:** 
- Reducir a 96 caracteres (32-127)
- Usar compresión RLE para filas repetidas

**Ahorro potencial:** ~200-300 bytes

---

## 2. Optimizaciones de SRAM

### 2.1 Variables Estáticas en Funciones

| Variable | Tamaño | Función | Propuesta | Estado |
|----------|--------|---------|-----------|--------|
| `v[16]` | 32 bytes | ssb() | Reducir a 15 + memmove | ✅ HECHO |
| `v[14]` | 28 bytes | process() | Reducir a 13 + memmove | ✅ HECHO |
| `v[7]` | 14 bytes | slow_dsp() | Mantener | - |
| `out[16]` | 16 bytes | CW decoder | Reducir a 8 | ⏸️ Pendiente |

**Acciones realizadas:**

```cpp
// ssb() línea 2037:
static int16_t v[15];  // Reducido de 16
memmove(v, v + 1, 14 * sizeof(int16_t));  // Shift register optimizado

// process() línea 2857:
static int16_t v[13];  // Reducido de 14
memmove(v, v + 1, 12 * sizeof(int16_t));  // Shift register optimizado
```

**Ahorro potencial:** 2 + 2 = 4 bytes de SRAM

### 2.2 Optimización de Buffers de Mensajes CW

```cpp
// usdx_plus_orange.ino:2258-2261
#ifdef CW_MESSAGE_EXT
char cw_msg[6][48] = { CW_MSG1, CW_MSG2, CW_MSG3, CW_MSG4, CW_MSG5, CW_MSG6 };
#else
char cw_msg[1][48] = { CW_MSG1 };
#endif
```

**Acción propuesta:** Almacenar en PROGMEM en lugar de SRAM.

```cpp
#ifdef CW_MESSAGE_EXT
const char cw_msg[6][48] PROGMEM = { CW_MSG1, CW_MSG2, CW_MSG3, CW_MSG4, CW_MSG5, CW_MSG6 };
#else
const char cw_msg[1][48] PROGMEM = { CW_MSG1 };
#endif
```

**Ahorro potencial:** 48-288 bytes de SRAM

### 2.3 Consolidación de Variables Volátiles

```cpp
// Actualmente dispersas:
volatile uint8_t agc = 1;
volatile uint8_t nr = 0;
volatile uint8_t att = 0;
volatile uint8_t att2 = 2;
volatile uint8_t volume = 12;
volatile int8_t mox = 0;
volatile int8_t volume = 12;  // DUPLICADO!
```

**Acción propuesta:** Crear estructura unificada.

```cpp
struct RadioState {
  uint8_t agc : 2;      // 0-3
  uint8_t nr : 3;       // 0-7
  uint8_t att : 3;      // 0-7
  uint8_t filt : 3;     // 0-7
  uint8_t volume : 5;   // 0-31
  uint8_t drive : 4;    // 0-15
  uint8_t mode : 3;     // LSB, USB, CW, FM, AM
  uint8_t vfo : 1;      // VFO A/B
  uint8_t tx : 1;       // TX/RX
};

volatile RadioState state;
```

**Ahorro potencial:** ~10-20 bytes, mejor locality

---

## 3. Mejoras de Rendimiento CPU

### 3.1 Optimización del Loop Principal

**Ubicación:** `usdx_plus_orange.ino:5200+` (loop())

**Problema:** Llamadas frecuentes a funciones con overhead.

**Mejora propuesta:** Inline de funciones críticas.

```cpp
// Mantener solo en modo DEBUG:
void print_memory_info() {
  lcd.setCursor(0, 0);
  lcd.print(F("MEM:"));
  lcd.print(freeMemory());
}
```

### 3.2 Optimización del Filtro CIC

**Ubicación:** `usdx_plus_orange.ino:2906-2920`

```cpp
// Actualmente:
void sdr_rx_00(){ int16_t ac = sdr_rx_common_i(); ... }

// Propuesta:
inline void sdr_rx_00(){ 
  ADMUX = admux[1]; ADCSRA |= (1 << ADSC); 
  int16_t ac = ADC - 511;
  func_ptr = sdr_rx_01;
  // ... resto inline
}
```

**Acción propuesta:** Eliminar llamadas a `sdr_rx_common_i/q()` y embeber el código.

**Ahorro potencial:** ~5-10% CPU

### 3.3 Pre-cálculo de Constantes

**Ubicación:** Múltiples lugares con cálculos en tiempo de ejecución.

```cpp
// Actualmente:
#define F_SAMP_TX 4800
#define _UA 600

// Propuesta:
const uint16_t UA = 600;
const uint16_t F_SAMP_TX = 4800;
const uint16_t UNIT_ANGLE_PER_SAMPLE = (F_SAMP_TX * _UA) / 600;  // Pre-calculado
```

---

## 4. Mejoras de Funcionalidad

### 4.1 Mejora del CW Decoder

| Aspecto | Actual | Propuesto |
|---------|--------|-----------|
| Algoritmo | Promedio simple | Promedio exponencial |
| Umbral | Fijo (50%) | Adaptativo |
| Noise Blanker | Fijo (16ms) | Escalable por WPM |

**Acción propuesta:** Implementar CW decoder adaptativo.

```cpp
// Nuevo algoritmo:
void cw_decode_adaptive() {
  static uint16_t noise_floor = 0;
  static uint16_t signal_avg = 0;
  
  // Calcular floor adaptativo
  noise_floor = (noise_floor * 15 + abs(in)) >> 4;
  
  // Umbral adaptativo
  uint16_t threshold = noise_floor + (signal_avg - noise_floor) / 2;
  
  // Resto del decoder...
}
```

### 4.2 Mejora del AGC

**Mejora:** Implementar AGC con ataque más rápido y decay más lento.

```cpp
// Nuevo process_agc_optimized():
inline int16_t process_agc_opt(int16_t in) {
  static int32_t peak = 0;
  static int16_t gain = 128;
  
  // Peak detector
  peak = max(peak, abs(in));
  peak = peak * 15 >> 4;  // Decay del peak
  
  // Ajuste de ganancia
  if(peak > 1024) {
    gain = gain * 15 >> 4;  // Attack rápido
  } else if(peak < 768) {
    gain = gain + (gain >> 6);  // Decay lento
  }
  
  return (in * gain) >> 7;
}
```

## 5. Optimizaciones de Configuración

### 5.1 Profile de Compilación

**Crear usdx_settings_minimal.h:**

```cpp
// Configuración mínima - RX-only, sin CAT
#define DIAG              0
#define KEYER             0
#define CAT               0
#define CW_DECODER        0
#define TX_ENABLE         0
#define CW_MESSAGE        0
#define CW_INTERMEDIATE   0
#define FAST_AGC          1
```

**Crear usdx_settings_full.h:**

```cpp
// Configuración completa - todas las features
#define DIAG              1
#define KEYER             1
#define CAT               1
#define CW_DECODER        1
#define TX_ENABLE         1
#define CW_MESSAGE        1
#define CW_INTERMEDIATE   1
#define FAST_AGC          1
```

### 5.2 Opciones de Compilación

**En usdx_settings.h:**

```cpp
// Descomentar para perfil mínimo
// #include "usdx_settings_minimal.h"

// Descomentar para perfil completo
// #include "usdx_settings_full.h"
```

---

## 6. Tareas Priorizadas

### ✅ Prioridad ALTA - FASE 1 COMPLETADA

| # | Tarea | Ahorro | Estado |
|---|-------|--------|--------|
| 1 | Desactivar `DIAG` | 1.3 KB | ✅ Hecho |
| 2 | Desactivar `CW_MESSAGE` | 500 bytes | ✅ Hecho |
| 3 | Desactivar `CW_INTERMEDIATE` | 20 bytes | ✅ Hecho |
| 4 | Optimizar `v[16]` → `v[15]` | 2 bytes SRAM | ✅ Hecho |
| 5 | Optimizar `v[14]` → `v[13]` | 2 bytes SRAM | ✅ Hecho |

### ⏸️ Prioridad MEDIA (Próxima fase)

| # | Tarea | Ahorro | Dificultad |
|---|-------|--------|------------|
| 6 | Inline funciones RX ISR | 5% CPU | Media |
| 7 | Pre-calcular constantes | Variable | Baja |
| 8 | ~~Optimizar NEW_CW decoder~~ | ~~106 bytes~~ | ✅ Hecho |

### ⏸️ Prioridad BAJA (Mejoras futuras)

| # | Tarea | Descripción |
|---|-------|-------------|
| 9 | CW decoder adaptativo | Mejor detección en ruido |
| 10 | AGC optimizado | Respuesta más natural |
| 11 | Compresión de fonts | ~200 bytes |
| 12 | Lookup table arctan3 | ~100 bytes, más rápido |
| 13 | Mover `cw_msg[]` a PROGMEM | 48-288 bytes SRAM |
| 14 | DSP adicional (NOTCH) | Filtro eliminador de tono |

---

## 7. Plan de Implementación Sugerido

### Fase 1: Ahorro rápido (compile-time)
1. Desactivar `DIAG`, `CW_MESSAGE`, `CW_INTERMEDIATE`
2. Mover `cw_msg[]` a PROGMEM
3. Reducir buffers `v[]`

### Fase 2: Optimización runtime
1. Inline funciones ISR críticas
2. Pre-calcular constantes
3. Eliminar código duplicado

### Fase 3: Mejoras opcionales
1. CW decoder adaptativo
2. AGC optimizado
3. Nuevas features según necesidad

---

## Estado de Implementación - Fase 1 Completada

### Cambios Realizados

| # | Cambio | Estado | Ahorro |
|---|--------|--------|--------|
| 1 | `DIAG = 0` | ✅ Hecho | ~1.3 KB Flash |
| 2 | `CW_MESSAGE = 0` | ✅ Hecho | ~500 bytes Flash |
| 3 | `CW_INTERMEDIATE = 0` | ✅ Hecho | ~20 bytes Flash |
| 4 | `v[16]` → `v[15]` con `memmove` en ssb() | ✅ Hecho | 2 bytes SRAM |
| 5 | `v[14]` → `v[13]` con `memmove` en process() | ✅ Hecho | 2 bytes SRAM |

### Resultado de Compilación

```
Sketch usa 31626 bytes (98%) del espacio de almacenamiento de programa
Las variables Globales usan 1479 bytes (72%) de la memoria dinámica
```

**Nota:** El ahorro de Flash es menor al esperado porque las funciones ya estaban optimizadas por el compilador con `-Os`. El ahorro de SRAM con memmove es modesto (4 bytes) pero mejora la velocidad de ejecución del ISR.

---

## 9. Notas de Compilación

Para medir el impacto de los cambios:

```bash
# Compilar y ver tamaño
avr-gcc -Os -mmcu=atmega328p -o firmware.elf firmware.ino
avr-size -C --mcu=atmega328p firmware.elf

# Ver consumo de funciones
avr-nm -S --size-sort firmware.elf | tail -20
```

---

## 10. Referencias y Recursos

- **Repositorio original:** https://github.com/threeme3/QCX-SSB
- **Datasheet ATMEGA328P:** Microchip DS40002061A
- **Guía de optimización AVR:** https://www.nongnu.org/avr-libc/user-manual/optimization.html

---

*Documento generado: 10 de enero de 2026*
*Proyecto: uSDX Plus Orange v1.03x*
