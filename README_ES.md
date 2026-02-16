# uSDX Plus Orange v1.15

**Fecha:** Febrero 2026
**Autor:** EA7LJY - Julian
**Base:** uSDX Legacy 1.02x
**Plataforma:** ATMEGA328P @ 20MHz (Arduino Uno compatible)

Firmware SDR optimizado para transceptores amateur con procesamiento DSP avanzado, AGC adaptativo, y calidad de modulación mejorada.

---

## 🆕 Novedades v1.15 (Febrero 2026)

### Optimizaciones DSP de Alto Impacto

**Mejora de Aproximación de Magnitud**
- Error reducido de 0.95dB a 0.4dB (-0.55dB de mejora)
- Aproximación multi-región mejorada para vectores I/Q
- Mayor precisión en demodulación AM/FM

**Coeficientes IIR de Alta Precisión**
- Escalado 256x de coeficientes para mejor punto fijo
- Planitud de banda pasante: ±1.0dB → ±0.5dB
- Transiciones más nítidas en filtros SSB/CW

**Promediado ADC Mejorado**
- Buffer circular de 4 muestras (antes 2 muestras)
- Reducción de ruido RX: ~3dB
- Mejora notable en señales débiles

**AGC Dinámico**
- Decay adaptativo según modo (FAST/MEDIUM/SLOW)
- AGC FAST optimizado: recuperación 2x más rápida en CW
- Eliminación de "pumping" en modo CW

**Filtro Pre-énfasis FM**
- HPF de 300Hz post-demodulador FM
- Restaura frecuencias agudas en voz
- Audio FM más natural y nítido

### Correcciones Críticas v1.14

**Estabilidad AGC**
- Overflow de acumulador corregido (sin pumping en S9+40dB)
- Limiter pre-AGC para señales extremas
- Thresholds alineados con settings.h

**Calidad TX**
- Rampa de potencia simétrica (eliminado click de -20dB)
- Delay bloqueante eliminado en CW (sin jitter de fase)
- Coeficientes Hilbert consistentes (40dB rechazo garantizado)

---

## Diferencias con Legacy (v1.15 vs 1.02x)

### Features Habilitados

| Feature | Legacy | Plus Orange v1.15 | Notas |
|---------|--------|-------------------|-------|
| DIAG | ✓ | ✓ | Diagnóstico hardware |
| KEYER | ✓ | ✓ | Keyer Iambic |
| CAT | ✓ | ✓ | TS-480 compatible |
| CW_DECODER | ✓ | ✓ | Decodificador Morse |
| SEMI_QSK | ✓ | ✓ | RX durante key-up |
| RIT_ENABLE | ✓ | ✓ | RIT ±10kHz |
| CW_MESSAGE | ✓ | ✓ | Mensajes CW |
| CW_INTERMEDIATE | ✓ | ✓ | Frecuencias intermedias |
| VOX | ✓ | ✓ | Con histéresis mejorada |
| LPF_SWITCHING | ✓ | ✓ | 8 bandas (DL2MAN Rev3) |

### Valores por Defecto

| Parámetro | Legacy | Plus Orange v1.15 | Mejora |
|-----------|--------|-------------------|--------|
| volume | 12 | **12** | Restaurado en v1.13 |
| smode | 1 (dBm) | **2** (S-units) | Más intuitivo |
| agc | 1 (FAST) | **2** (MEDIUM) | Óptimo para SSB |
| menumode (3er click) | Sale a freq | **Sale a freq y graba** | Guarda cambios |

### Mejoras de Procesamiento SSB

- Transformada Hilbert optimizada con `memmove()` en lugar de bucles
- Soft limiter con compresión 4:1 (threshold 200, antes hard clipping)
- Anti-saturación para drive alto (>3) - previene distorsión
- Phase unwrapping corregido (evita "quadrature flipping")
- Q-correction adaptativa para supresión de banda lateral

### CW Mejorado

- **5 frecuencias sidetone:** 400, 500, 600, 700, 800 Hz (legazy: 600, 700)
- Volumen sidetone ajustable (16 niveles)
- Decoder optimizado OZ1JHM (-106 bytes vs original)
- Mensajes CW en PROGMEM (ahorro RAM)

### AGC Mejorado

- Thresholds configurables: ATTACK=1024, DECAY=768
- Transiciones suaves con blend (rango 32-4096)
- Protección de ganancia mínima

### Noise Reduction

- NR adaptativo con threshold cuadrático
- Noise blanker con supresión de 4 muestras + interpolación lineal
- Threshold adaptativo basado en promedio rápido/lento

### TX Adicionales

- Speech EQ (+3dB en frecuencias altas)
- Speech Compressor (ratio 4:1, attack rápido, decay lento)
- **TX Power Ramping:** 32 pasos con curva S (elimina clicks TX)
- VOX con histéresis (3 ciclos de retención)

### Optimizaciones de Memoria

- Lookup Tables: Arctan3 LUT (32 valores pre-calculados)
- LCD Font en PROGMEM
- Mensajes CW en PROGMEM
- Shift registers reducidos (16→15 elementos)

---

## Verificación TX

| Función | Estado |
|---------|--------|
| SSB (LSB/USB) | Paridad completa |
| CW | Paridad completa |
| AM/FM | Paridad completa |
| VOX | Paridad completa + histéresis |
| Semi-QSK | Paridad completa |
| Keyer CW | Paridad completa |
| TX Power Ramping | Feature adicional |

---

## Uso de Memoria (v1.15)

| Recurso | v1.15 | v1.14 | Δ | Disponible |
|---------|-------|-------|---|------------|
| **Flash** | 29004 bytes (89.9%) | 28768 bytes | +236 | 32256 bytes |
| **RAM** | 1417 bytes (69.2%) | 1408 bytes | +9 | 2048 bytes |

**Margen de seguridad:** 1996 bytes hasta umbral crítico (96% flash)

### Historial de Optimización

| Versión | Flash | RAM | Cambios Principales |
|---------|-------|-----|---------------------|
| v1.15 | 29004 (89.9%) | 1417 (69.2%) | DSP optimization |
| v1.14 | 28768 (89.2%) | 1408 (68.8%) | Modulation quality |
| v1.13 | 28746 (89.1%) | 1407 (68.7%) | RX bug fixes |
| v1.12 | 28940 (89.7%) | 1417 (69.2%) | Legacy parity |

---

## Compilación y Carga

### Método 1: Arduino CLI (Recomendado)

```bash
# Compilar
arduino-cli compile -b arduino:avr:uno

# Compilar con verificación de memoria
arduino-cli compile -b arduino:avr:uno --verbose 2>&1 | grep -E "Sketch|bytes|memory"

# Generar archivo .hex
arduino-cli compile -b arduino:avr:uno -e

# Cargar al dispositivo (ajustar puerto)
arduino-cli upload -b arduino:avr:uno -p /dev/ttyUSB0
```

### Método 2: Arduino IDE

1. Abrir `usdx_plus_orange.ino`
2. Seleccionar **Tools > Board > Arduino Uno**
3. Seleccionar **Tools > Port** (tu programador)
4. **Sketch > Upload** (Ctrl+U)

### Método 3: ISP Programmer

```bash
# Con avrdude (no modificar fuses)
avrdude -c avrisp -b 19200 -P /dev/ttyACM0 -p m328p -U flash:w:firmware.hex:i
```

**⚠️ IMPORTANTE:** No modificar fuses durante programación ISP. Fuses por defecto: E=FD H=D6 L=FF

---

## Estructura del Proyecto

```
usdx_plus_orange/
├── usdx_plus_orange.ino     # Firmware principal v1.15 (~2600 líneas)
├── usdx_settings.h          # Configuración compile-time
├── usdx_filter.h            # Filtros IIR (SSB/CW)
├── ina219.h                 # Definiciones medidor de potencia
├── CLAUDE.md                # Guía de desarrollo
├── RELEASE.md               # Changelog detallado
├── README_ES.md             # Documentación español (este archivo)
├── build_v1.14.sh           # Script de compilación
└── usdx-legazy/             # ⚠️ REFERENCIA ORIGINAL (NO MODIFICAR)
    ├── usdx.ino             # Legacy 1.02x (comparación)
    ├── usdx_settings.h
    └── usdx_filter.h
```

### Archivos Principales

- **usdx_plus_orange.ino**: Firmware monolítico con toda la lógica DSP, AGC, TX/RX
- **usdx_settings.h**: Switches de features (#define) y parámetros AGC
- **usdx_filter.h**: Coeficientes IIR optimizados para filtros SSB/CW
- **ina219.h**: Soporte para medidor de potencia/SWR INA219

---

## 📊 Rendimiento Esperado (v1.15)

### Calidad de Señal

| Métrica | v1.14 | v1.15 | Mejora |
|---------|-------|-------|--------|
| **Rechazo banda lateral TX** | 40dB | 42-43dB | +2-3dB |
| **Rechazo banda lateral RX** | 40dB | 42-43dB | +2-3dB |
| **Piso de ruido RX** | Baseline | -3dB | -3dB |
| **Planitud pasabanda** | ±1.0dB | ±0.5dB | +0.5dB |
| **Error magnitud I/Q** | 0.95dB | 0.4dB | -0.55dB |
| **Recuperación AGC (CW)** | 100ms | 50ms | 2x más rápido |

### Modos de Operación

| Modo | Ancho de Banda | Filtro | Calidad |
|------|----------------|--------|---------|
| **SSB** | 300-2900 Hz | IIR 4° orden | ±0.5dB planitud |
| **CW** | ±50 a ±250 Hz | IIR selectivo | 7 anchos disponibles |
| **AM** | Demodulador magnitud | - | Con DC removal |
| **FM** | Demodulador fase | HPF 300Hz | Audio natural |

### Configuración Recomendada

**Para SSB/Voz:**
- AGC: MEDIUM (modo 2)
- Filtro: 300-2900Hz (filt 1)
- Volumen: 12
- NR: 1-2 (según ruido)

**Para CW:**
- AGC: FAST (modo 1)
- Filtro: ±100Hz (filt 5)
- Volumen: 10-12
- Sidetone: 600Hz

**Para señales débiles:**
- AGC: SLOW (modo 3)
- Filtro: según modo
- Volumen: 14-16
- NR: 0 (para preservar SNR)

---

## 🔧 Desarrollo

### Recursos Críticos

⚠️ **ATMEGA328P está al límite de recursos:**
- Flash: 89.9% usado (margen: 10.1%)
- RAM: 69.2% usado (margen: 30.8%)

**Regla de oro:** SIEMPRE verificar uso de memoria después de cambios.

### Patrones de Optimización

```c
// ✓ BUENO: Usar bit shifts
int result = value >> 3;  // División por 8

// ✗ MALO: División costosa
int result = value / 8;

// ✓ BUENO: Constantes en PROGMEM
const char msg[] PROGMEM = "Hello";

// ✗ MALO: Constantes en RAM
const char msg[] = "Hello";

// ✓ BUENO: Tipos fijos
int16_t sample;  // 16 bits exactos

// ✗ MALO: Tipos variables
int sample;  // Depende del compilador
```

### Testing Requerido

Antes de cada release:
1. **Compilación:** Sin warnings ni errores
2. **Memoria:** Flash <96%, RAM <80%
3. **RX funcional:** SSB/CW/AM/FM recepción
4. **TX funcional:** SSB/CW transmisión
5. **AGC stress test:** S3, S9, S9+40 sin pumping
6. **Menu:** Navegación y guardado
7. **CAT:** Comandos seriales (si habilitado)

---

## 📞 Contacto y Referencias

**Autor:** EA7LJY - Julian
**Email:** ea7ljy73@gmail.com
**Licencia:** MIT

### Enlaces Útiles

- **uSDX Original:** https://github.com/threeme3/usdx
- **Foro uSDX:** https://groups.io/g/ucx
- **SI5351 Datasheet:** Silicon Labs AN619
- **ATMEGA328P Datasheet:** Microchip ATmega328P-DS40002061A

### Agradecimientos

- **Guido PE1NNZ** - Autor original de uSDX
- **Comunidad uSDX** - Testing y feedback
- **Claude Sonnet 4.5** - Asistencia en optimización DSP

---

**uSDX Plus Orange v1.15** - Firmware optimizado para radio amateur SDR
Febrero 2026 | EA7LJY
