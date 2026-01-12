# uSDX Plus Orange - Release Notes (Spanish)

**Versión:** 1.10x
**Base:** uSDX Legacy 1.02x
**Plataforma:** ATMEGA328P @ 20MHz
**Autor:** EA7LJY - Julian (modificaciones)

---

## 1. Mejoras en el Procesador de Señal SSB

### 1.1 Transformada de Hilbert Optimizada
- Uso de `memmove()` en lugar de bucles manuales para el registro de desplazamiento
- Eliminación de operaciones de desplazamiento en valores potencialmente negativos
- Nueva implementación segura
- Reducción del buffer de 16 a 15 elementos para ahorrar memoria RAM

### 1.2 Limitador Suave con Compresión
- Sustitución del hard clipping por compresión suave
- Threshold reducido de 250 a 200 para mejor inteligibilidad
- Relación de compresión 4:1 por encima del threshold
- Previene saturación manteniendo calidad de audio

### 1.3 Compensación Anti-Saturación para Alto Drive
```c
if(drive > 3 && _amp > 200){
  _amp = 200 + (_amp - 200) * (6 - drive) / (6 - 3);
}
```
- Reduce progresivamente la ganancia a niveles de drive altos
- Previene distorsión por saturación del transmisor

---

## 2. Mejoras en CW (Morse Code)

### 2.1 Frecuencias de Sidetone Expandidas
- 5 frecuencias seleccionables: 400, 500, 600, 700, 800 Hz

### 2.2 Control de Volumen de Sidetone
- Volumen ajustable en tiempo de ejecución
- 16 niveles de volumen disponibles

### 2.3 Mensajes CW en PROGMEM
- Mensajes CW almacenados en flash en lugar de RAM

---

## 3. Mejoras en el Decodificador CW

### 3.1 Nuevo Decoder Moderno (NEW_CW)
- Decoder optimizado de OZ1JHM
- Eliminado código duplicado OLD_CW
- Ahorro de 106 bytes

### 3.2 Tracking de Dirección del Encoder
- Implementación uSDXOpen: tracking de dirección del encoder
- Permite cambio de banda bidireccional suave

---

## 4. Sistema de Control de Ganancia Automática (AGC) Mejorado

### 4.1 Nuevos Thresholds Configurables
```c
#define AGC_ATTACK_THRESHOLD 1280  // Lower threshold for faster attack
#define AGC_DECAY_THRESHOLD 1024   // Threshold for decay
```

### 4.2 Respuesta Multi-Nivel
- Attack muy rápido para señales fuertes
- Respuesta media para decay suave

### 4.3 Protección de Ganancia Mínima
- Previene que la ganancia caiga por debajo de un umbral seguro

---

## 5. Reducción de Ruido (Noise Reduction)

### 5.1 Nivel por Defecto Aumentado
- Nivel NR por defecto = 2

### 5.2 Noise Gate Espectral Adaptativo
- Expansor temporal adaptativo
- Threshold proporcional al nivel NR seleccionado

---

## 6. Noise Blanker Adaptativo

- Detección de picos de ruido basada en floor adaptativo
- Supresión de 4 muestras con interpolación lineal
- Threshold adaptativo basado en diferencia entre promedios rápido/lento

---

## 7. Filtro Notch (Opcional)

- Filtro IIR biquad notch
- Elimina interferencias de frecuencia específica (hum 50/60Hz)
- Requiere ~600 bytes de Flash cuando habilitado
- Deshabilitado por defecto para ahorrar memoria

---

## 8. Lookup Table para Arctan3

- 32 valores pre-calculados para z = 0 a 31/32
- Acelera significativamente `_arctan3()`
- Ahorro de ciclos de CPU en el procesamiento de demodulación

---

## 9. S-Meter con Peak Hold

- `P` - Peak acaba de alcanzar nuevo máximo
- `p` - Peak en decaimiento
- ` ` (espacio) - Sin actividad de peak
- Peak hold visible por ~30 actualizaciones

---

## 10. Mejoras TX Adicionales

### 10.1 Speech Pre-emphasis (EQ)
- Filtro high-pass que realza frecuencias altas de voz
- +4dB de ganancia global
- Mejora la inteligibilidad y claridad de la voz transmitida

### 10.2 Speech Compressor
- Compresión de rango dinámico 4:1 automática
- Attack rápido, decay lento para respuesta natural
- Ganancia mínima garantizada (floor de 64/256)

### 10.3 TX Power Ramping
- 32 pasos de rampa suave al inicio/fin de transmisión
- Curva S-curve para transiciones naturales
- Elimina "pops" y clicks en la conmutación TX/RX

### 10.4 VOX con Hysteresis
- Evita oscilaciones del VOX
- 3 ciclos de retención después de caer bajo threshold
- Transición más estable entre RX/TX

---

## 11. Mejoras TX-01: Calidad de Voz SSB (uSDXOpen)

### 11.1 Corrección de Phase Unwrapping
- Previene "quadrature flipping" que causa distorsión
- Corrige automáticamente saltos grandes de fase

### 11.2 Filtro de Suavizado de Fase
- Reduce fluctuaciones de fase
- Suaviza la señal de audio transmitida

### 11.3 Corrección Adaptativa de Q
- Suaviza la transición entre cuadrantes I/Q
- Mejora la supresión de banda lateral no deseada

---

## 12. Optimizaciones de Memoria

### 12.1 Uso de PROGMEM para Constantes
- Tabla de fuentes LCD en PROGMEM
- Mensajes CW en PROGMEM
- LUT de arctan en PROGMEM
- Curva de ramp en PROGMEM

### 12.2 Reducción de Tamaños de Buffers
- SSB shift register: 16 → 15 elementos
- Q processing: 14 → 13 elementos
- Uso de `memmove()` para shift registers

### 12.3 Cálculos Optimizados
- División por potencias de 2 reemplazada por shifts
- Eliminación de operaciones redundantes

---

## Resumen de Cambios

| Categoría | Cambios |
|-----------|---------|
| Procesamiento SSB | Hilbert optimizada, soft limiter, anti-saturación |
| CW | Decoder optimizado (-106 bytes), sidetone 5 freq, volumen |
| AGC | Multi-threshold, protección de ganancia |
| NR | Noise gate adaptativo |
| Noise Blanker | Nuevo, adaptativo |
| TX Optimizations | Speech EQ, Compressor, Power Ramping, VOX Hysteresis |
| TX-01 SSB Quality | Phase unwrapping, smoothing, Q correction |
| Memoria | PROGMEM extensivo, buffers reducidos, decoder optimizado |

---

## Compilación

Para compilar:
- **Arduino IDE:** Board "Arduino Uno", Clock "20MHz"
- **AVR-GCC:** `-DF_MCU=20000000 -DF_CPU=20007000`

### Uso de Memoria (configuración completa)
| Recurso | Uso | Disponible |
|---------|-----|------------|
| Flash | 32026 bytes (99%) | 32256 bytes |
| RAM | 1482 bytes (72%) | 2048 bytes |

### Características opcionales
| Característica | Flash |
|----------------|-------|
| DIAG | +1308 bytes |
| CAT | +4150 bytes |
| CW_DECODER | +1468 bytes |
| NOTCH_FILTER | +600 bytes |
| PER_BAND_TRACKING | +500 bytes |

---

**Fecha de Release:** Enero 2026
**Autor:** EA7LJY - Julian
**Licencia:** MIT License
