# uSDX Plus Orange v1.14

**Fecha:** Febrero 2026
**Autor:** EA7LJY - Julian
**Base:** uSDX Legacy 1.02x
**Plataforma:** ATMEGA328P @ 20MHz

Refactorización limpia del firmware uSDX con paridad de características y mejoras.

---

## Diferencias con Legazy (v1.12 vs 1.02x)

### Features Habilitados

| Feature | Legazy | Plus Orange v1.12 |
|---------|--------|-------------------|
| DIAG | 1 | 1 (habilitado) |
| KEYER | 1 | 1 (habilitado) |
| CAT | 1 | 1 (habilitado) |
| CW_DECODER | 1 | 1 (habilitado) |
| SEMI_QSK | 1 | 1 (habilitado) |
| RIT_ENABLE | 1 | 1 (habilitado) |
| CW_MESSAGE | 1 | 1 (habilitado) |
| CW_INTERMEDIATE | 1 | 1 (habilitado) |

### Valores por Defecto

| Parámetro | Legazy | Plus Orange v1.12 |
|-----------|--------|-------------------|
| volume | 12 | **10** |
| smode | 1 (dBm) | **2** (S-units) |
| menumode (3er click) | Sale a freq | **Sale a freq y graba** |

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

## Uso de Memoria (v1.12)

| Recurso | Uso | Disponible |
|---------|-----|------------|
| Flash | 28940 bytes (89%) | 32256 bytes |
| RAM | 1417 bytes (69%) | 2048 bytes |

---

## Compilación

```bash
# Arduino CLI
arduino-cli compile -b arduino:avr:uno

# Con generación de .hex
arduino-cli compile -b arduino:avr:uno -e
```

---

## Estructura del Proyecto

```
usdx_plus_orange/
├── usdx_plus_orange.ino     # Firmware v1.12
├── usdx_settings.h          # Configuración
├── usdx_filter.h            # Filtros DSP
├── README.md                # Documentación inglés
└── usdx-legazy/             # Referencia original (NO MODIFICAR)
    ├── usdx.ino
    ├── usdx_settings.h
    └── usdx_filter.h
```

---

## Contacto

**Email:** ea7ljy73@gmail.com  
**Licencia:** MIT
