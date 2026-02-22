# uSDX Plus Orange - Documentación Técnica

**Versión:** 5.14  
**Autor:** EA7LJY - Julian  
**Fecha:** Febrero 2026

---

## Tabla de Contenidos

1. [Descripción General](#1-descripción-general)
2. [Cadena de Señal TX](#2-cadena-de-señal-tx)
3. [Cadena de Señal RX](#3-cadena-de-señal-rx)
4. [Funciones DSP](#4-funciones-dsp)
5. [Procesamiento por Modos](#5-procesamiento-por-modos)
6. [Síntesis de Frecuencia SI5351](#6-síntesis-de-frecuencia-si5351)
7. [Resumen del Flujo de Señal](#7-resumen-del-flujo-de-señal)
8. [Estructura de Archivos](#8-estructura-de-archivos)

---

## 1. Descripción General

uSDX Plus Orange es una radio definida por software (SDR) para hardware basado en ATMEGA328P. Implementa una cadena de señal SDR completa usando:

- **TX:** Síntesis Digital Directa (DDS) con SI5351
- **RX:** Detector de Muestreo en Cuadratura (QSD) con muestreo ADC
- **DSP:** Aritmética entera optimizada para microcontrolador AVR

### Especificaciones Clave

| Parámetro | Valor |
|-----------|-------|
| Plataforma | ATMEGA328P @ 20MHz |
| Flash | 32KB |
| RAM | 2KB |
| ADC | 10-bit @ 62.5kSPS (RX) |
| Modos TX | SSB (USB/LSB), CW, AM, FM |
| Modos RX | SSB (USB/LSB), CW, AM, FM |

---

## 2. Cadena de Señal TX

La cadena TX convierte el audio del micrófono en señales de RF.

### 2.1 Diagrama de TX

```
ENTRADA MICRÓFONO
       |
       v
   +---------+     +-------------+     +--------------+     +--------------+
   |   ADC   | --> |   Procesado  | --> |   Generación | --> |   Fase a     |
   |  (ADC)  |     |   de Voz     |     |   SSB        |     |   Frecuencia |
   +---------+     +-------------+     +--------------+     +   Converter   |
       |                   |                     |                    |
       v                   v                     v                    v
   ADC 10-bit           Compresor           Hilbert             Diferencia de fase
   @ F_SAMP_TX         EQ, Pre-énfas       Transform           a frecuencia
                                                                   |
                                                                   v
                                                       +-----------------------+
                                                       |   Síntesis SI5351     |
                                                       +-----------------------+
                                                               |
                                                               v
                                                       +-----------------------+
                                                       |   I2C a SI5351        |
                                                       +-----------------------+
                                                               |
                                                               v
                                                       +-----------------------+
                                                       |   Salida PWM          |
                                                       |   (OCR1BL)            |
                                                       +-----------------------+
```

### 2.2 Entrada de Micrófono y ADC

**Ubicación:** Líneas 2211-2213

```cpp
int16_t adc = ADC - 512;  // Centra el rango del ADC
int16_t df = ssb(adc >> MIC_ATTEN);  // Procesa con cadena TX
```

**Parámetros:**
- Resolución ADC: 10 bits
- Offset DC: 512 (centra el rango)
- MIC_ATTEN: Factor de atenuación (default 0)

### 2.3 Procesado de Voz

Tres etapas de procesamiento preparan el audio para modulación SSB:

#### 2.3.1 Compresor de Voz

**Ubicación:** Líneas 2005-2021

```cpp
inline int16_t voice_compressor(int16_t in) {
  int16_t abs_in = in < 0 ? -in : in;
  
  // Ataque: ~1.5ms (más rápido desde v5.14)
  // Release: ~27ms
  if(abs_in > comp_envelope)
    comp_envelope = comp_envelope + ((abs_in - comp_envelope) >> 1);
  else
    comp_envelope = comp_envelope - ((comp_envelope - abs_in) >> 7);

  // Ratio: 2:1 (más suave desde v5.14)
  if(comp_envelope > comp_threshold) {
    int16_t gain = (comp_envelope - comp_threshold) / comp_ratio + comp_threshold;
    return (int16_t)((int32_t)in * gain / comp_envelope);
  }
  return in;
}
```

**Parámetros (v5.14):**
| Parámetro | Valor | Propósito |
|-----------|-------|-----------|
| `comp_enable` | 1 | Compresor activado |
| `comp_ratio` | 2 | Ratio 2:1 (más suave) |
| `comp_threshold` | 128 | Umbral de activación |
| `comp_envelope` | 0 | Seguimiento de envolvente |

#### 2.3.2 EQ del Micrófono

**Ubicación:** Líneas 2023-2034

EQ paramétrico de dos bandas para dar forma a la voz.

```cpp
inline int16_t mic_eq(int16_t in) {
  if(eq_low == 0 && eq_high == 0)
    return in;
  // Procesamiento de estantes graves y agudos
  // ...
}
```

#### 2.3.3 Pre-énfasis

**Ubicación:** Líneas 2040-2044

```cpp
if(pre_emph > 0) {
  int16_t pre_in = in;
  in = in + ((pre_in - pre_z1) * pre_emph);
  pre_z1 = pre_in;
}
```

| Valor | Efecto |
|-------|---------|
| 0 | Apagado |
| 1 | 6dB/oct (defecto) |
| 2 | 12dB/oct |
| 3 | 18dB/oct |

### 2.4 Transformada de Hilbert para SSB

**Ubicación:** Líneas 2069-2087

Crea la señal con desplazamiento de fase de 90° para SSB.

```cpp
// Componente I (en fase)
i = v[7] * 2;

// Componente Q (cuadratura) con Hilbert
q = ((v[0] - v[14]) * 2 + 
     (v[2] - v[12]) * 8 + 
     (v[4] - v[10]) * 21 + 
     (v[6] - v[8]) * 16) / 64 + (v[6] - v[8]);
```

**Rendimiento:** 40dB de rechazo de banda lateral en rango 400-1900Hz.

### 2.5 Conversión de Fase a Frecuencia

**Ubicación:** Líneas 2105-2129

```cpp
int16_t phase = arctan3(q, i);      // Calcula fase
int16_t dp = phase - prev_phase;    // Diferencia de fase = frecuencia
prev_phase = phase;

if(dp < 0)
  dp = dp + _UA;  // Hace positivas todas las diferencias
```

---

## 3. Cadena de Señal RX

RX usa QSD (Detector de Muestreo en Cuadratura) para muestreo RF directo.

### 3.1 Diagrama de RX

```
ENTRADA RF (del Detector QSD)
       |
       v
   +---------+     +-------------+     +-------------+     +-------------+
   |   ADC   | --> |   CIC        | --> |   Decimación | --> |   Hilbert    |
   |  (ADC)  |     |  Decimación |     |   (8x)       |     |   Transform  |
   +---------+     +-------------+     +-------------+     +-------------+
   Line 3433          Line 3358-3418       Line 3228-3347      Line 2983-3009
       |                   |                    |                   |
       v                   v                    v                   v
   ADC 10-bit          CIC 3er orden        Downsample          Shift 90° fase
   @ 62.5kSPS          R=4, M=2             62.5k -> 7.8kSPS    para SSB
                                                                   |
                                                                   v
                                                       +---------------------+
                                                       |   Demodulación      |
                                                       |   (por modo)        |
                                                       +---------------------+
                                                               Line 3030-3100
                                                                   |
                                                   +-------+-------+-------+
                                                   |       |       |       |
                                                   v       v       v       v
                                              +-------+ +-------+ +-------+
                                              |  SSB  | |  AM   | |  FM   |
                                              |  demod| | demod | | demod |
                                              +-------+ +-------+ +-------+
                                              Line3096 Line3030 Line3074
                                                   |       |       |
                                                   v       v       v
                                              +-------+ +-------+ +-------+
                                              |  AGC  | |  AGC  | | Deemph|
                                              +-------+ +-------+ +-------+
                                              Line2712 Line2712 Line2141
                                                   |       |       |
                                                   v       v       v
                                              +-------+ +-------+ +-------+
                                              |  NR   | |  NR   | |  NR   |
                                              +-------+ +-------+ +-------+
                                              Line2881 Line2881 Line2881
                                                   |       |       |
                                                   v       v       v
                                              +-------+ +-------+ +-------+
                                              |  BPF  | |  BPF  | |  BPF  |
                                              +-------+ +-------+ +-------+
                                              Line2945 Line2945 Line2945
                                                   |
                                                   v
                                              +---------------------+
                                              |   Salida Audio PWM  |
                                              |   (OCR1AL)          |
                                              +---------------------+
```

### 3.2 Muestreo I/Q

**Ubicación:** Líneas 3423-3456

El QSD produce muestras I y Q alternadas:

```cpp
// Canal I (muestras pares)
inline int16_t sdr_rx_common_i() {
  int16_t adc = ADC - 511;  // Eliminación DC
  int16_t ac = (prev_adc + adc) >> 1;  // Promedio de 2 muestras
  prev_adc = adc;
  return ac;
}

// Canal Q (muestras impares)
inline int16_t sdr_rx_common_q() {
  return ADC - 511;  // Eliminación DC
}
```

**Parámetros:**
- Tasa de Muestreo: 62,500 SPS
- Offset DC: 511
- Promedio: 2 muestras para reducción de ruido

### 3.3 Decimación CIC

**Ubicación:** Líneas 3228-3418

Filtro CIC (Cascaded Integrator-Comb) de 3er orden:

```cpp
#define R 4  // Factor de cambio de tasa
#define M_SR 1  // Shift de decimación

void sdr_rx_00() {
  int16_t ac = sdr_rx_common_i();
  int16_t i_s1za0 = (ac + (i_s0za1 + i_s0zb0) * 3 + i_s0zb1) >> M_SR;
  int16_t ac2 = (i_s1za0 + (i_s1za1 + i_s1zb0) * 3 + i_s1zb1);
  process(ac2, q_ac2);
}
```

**Cadena de Decimación:**
| Etapa | Tasa Entrada | Tasa Salida | Factor |
|-------|--------------|-------------|--------|
| CIC | 62,500 SPS | 7,812.5 SPS | 8x |

### 3.4 Transformada de Hilbert para Demodulación

**Ubicación:** Líneas 2983-3009

```cpp
// Hilbert transform para SSB/CW
qh = ((v[0] - q_ac2) + (v[2] - v[12]) * 4) / 64 + 
     ((v[4] - v[10]) + (v[6] - v[8])) / 8 +
     ((v[4] - v[10]) * 5 - (v[6] - v[8])) / 128 + 
     (v[6] - v[8]) / 2;
```

**Rendimiento:** 43dB de rechazo de banda lateral en rango 650-3400Hz.

### 3.5 Implementación AGC

**Ubicación:** Líneas 2712-2740

```cpp
inline int16_t process_agc(int16_t in) {
  // Cálculo de ganancia
  if(centiGain >= 128)
    out = (centiGain >> 5) * in;
  else
    out = (centiGain >> 2) * (in >> 3);
  out >>= 2;
  
  // Ataque rápido, release lento
  if(HI(abs(out)) > HI(1536))
    centiGain -= (centiGain >> 4);  // Ataque rápido
  else {
    if(--decayCount == 0) {
      if(small)
        centiGain += (centiGain >> 4);
      decayCount = (uint16_t)agc_decay * 100;
    }
  }
  return out;
}
```

**Parámetros:**
| Parámetro | Defecto | Propósito |
|-----------|---------|-----------|
| `agc` | 1 | Modo AGC (1=normal, 2=rápido) |
| `agc_decay` | 8 | Tiempo de release (~800ms) |

### 3.6 Filtrado de Audio

**Ubicación:** `usdx_filter.h` Líneas 46-222

```cpp
inline int16_t filt_var(int16_t za0) {
  // High-pass 300Hz
  za0 = ((30 * (za0 - zz2) + 25 * zz1) >> 5);
  
  // Pasabanda IIR 4to orden (SSB)
  zb0 = ((za0 + 2*za1 + za2) >> 1) - ((13*zb1 + 11*zb2) >> 4);
  zc0 = ((zb0 + 2*zb1 + zb2) >> 1) - ((18*zc1 + 11*zc2) >> 4);
  
  return zc0;
}
```

**Opciones de Filtro:**
| Filtro | Ancho de Banda | Caso de Uso |
|--------|----------------|-------------|
| 1 | 0-2900Hz | SSB Ancho |
| 2 | 0-2400Hz | SSB Estrecho |
| 3 | 0-1800Hz | SSB Muy Estrecho |
| 4 | 500-1000Hz | CW |
| 5 | 650-840Hz | CW Estrecho |
| 6 | 650-750Hz | CW Muy Estrecho |
| 7 | 630-680Hz | CW Extremo |

---

## 4. Funciones DSP

### 4.1 arctan3() - Cálculo de Fase

**Ubicación:** Líneas 1951-1967

```cpp
inline int16_t arctan3(int16_t y, int16_t x) {
  if (x == 0 && y == 0) return 0;
  
  int8_t c = 0;
  int16_t rx = x, ry = y;
  
  if (ry < 0) {
    if (rx < 0) { ry = -ry; rx = -rx; c = 2; }
    else { rx = -rx; ry = -ry; c = 3; }
  } else {
    if (rx < 0) { ry = -ry; rx = -rx; c = 1; }
  }
  
  int16_t r = (ry << 8) / (rx + (rx >> 4));
  int16_t ang = (r * 128) >> 8;
  if (ang > 90) ang = 90;
  
  static const int8_t atan_table[] = { /* 0-90 grados */ };
  int16_t result = atan_table[ang];
  
  switch (c) {
    case 1: result = -result; break;
    case 2: result = -180 + result; break;
    case 3: result = 180 - result; break;
  }
  return result;
}
```

**Error:** ~0.8 grados

### 4.2 magn() - Cálculo de Magnitud

**Ubicación:** Líneas 1969-1971

```cpp
#define magn(i, q) \
  (abs(i) > abs(q) ? abs(i) + (abs(q)/4) : abs(q) + (abs(i)/4))
```

**Error:** ~0.95dB vs magnitud real

### 4.3 slow_dsp() - Post-procesamiento

**Ubicación:** Líneas 2970-3174

Función principal de DSP para RX:

```cpp
inline int16_t slow_dsp(int16_t i_ac2, int16_t q_ac2) {
  // Control de ganancia digital
  q_ac2 >>= att2;
  
  // Hilbert (SSB/CW)
  if(mode != AM && mode != FM) {
    qh = /* cálculo Hilbert */;
  }
  
  // Demodulación por modo
  if(mode == AM) {
    ac = magn(i, q);
  } else if(mode == FM) {
    int16_t z0 = _arctan3(q, i);
    ac = z0 - z1;
  } else {  // SSB, CW
    acm = -id - qh;
    ac = acm;
  }
  
  // AGC
  if(agc == 1)
    ac = process_agc(ac);
  
  // Reducción de ruido
  if(nr)
    ac = process_nr(ac);
  
  // Filtro pasabanda
  if(filt)
    ac = filt_var(ac);
  
  return ac;
}
```

### 4.4 process_nr() - Reducción de Ruido

**Ubicación:** Líneas 2881-2941

Dos métodos disponibles:

**Método 1: Promedio Exponencial (nr < 3)**
```cpp
inline int16_t process_nr(int16_t in) {
  static int16_t ea1;
  ea1 = EA(ea1, in, 1 << (nr - 1));
  return ea1;
}
```

**Método 2: FIR Paso Bajo (nr >= 3)**
```cpp
// Filtro FIR con tamaños de ventana 7-21 taps
// Frecuencias de corte: 2700Hz hasta 500Hz
```

---

## 5. Procesamiento por Modos

### 5.1 SSB (USB/LSB)

**TX:** Líneas 2036-2129
```
Audio --> Compresor --> EQ --> Pre-énfasis --> Hilbert --> I/Q --> arctan3 --> df --> SI5351
```

**RX Demodulación:** Líneas 3096-3100
```cpp
else {  // USB, LSB, CW
  acm = -id - qh;  // Demodulación SSB
  ac = acm;
}
```

### 5.2 CW

**TX Codificación:** Líneas 2266-2284

Algoritmo de Minsky para generación de tono:

```cpp
inline void process_minsky() {
  int8_t alpha127 = tones[cw_tone] * 798 / _F_SAMP_TX;
  p_sin += alpha127 * n_cos / 127;
  n_cos -= alpha127 * p_sin / 127;
}
```

**RX Decodificador:** Líneas 2460-2530+

Detección de umbral y decodificación Morse.

### 5.3 AM

**TX:** Líneas 2286-2302

```cpp
void dsp_tx_am() {
  int16_t in = (ADC - 512) >> MIC_ATTEN;
  in = in << (drive - 4);
  in = max(0, min(255, (in + 32)));  // Offset base
  amp = in;
}
```

**RX Demodulación:** Líneas 3030-3045
```cpp
acm = -i - q;  // S-Meter
ac = magn(i, q);  // Detección de magnitud
```

### 5.4 FM

**TX:** Líneas 2304-2321

```cpp
void dsp_tx_fm() {
  int16_t in = (ADC - 512) >> MIC_ATTEN;
  in = in << drive;
  int16_t df = in;
  si5351.freq_calc_fast(df);  // Desviación de frecuencia directa
}
```

**RX Demodulación:** Líneas 3074-3094
```cpp
else if(mode == FM) {
  int16_t z0 = _arctan3(q, i);
  ac = z0 - z1;  // Diferenciador
  z1 = z0;
}
```

---

## 6. Síntesis de Frecuencia SI5351

### 6.1 Descripción General

El SI5351 genera la portadora de RF y el reloj de muestreo.

**Funciones Clave:**
| Función | Líneas | Propósito |
|----------|-------|-----------|
| `freq_calc_fast()` | 1412, 2193 | Calcular registros de frecuencia |
| `SendPLLRegisterBulk()` | 1435, 2168 | Enviar registros por I2C |
| `freq()` | 1559 | Configurar CLK0/CLK1/CLK2 |

### 6.2 Síntesis TX

**SSB/CW:**
```cpp
int16_t df = ssb(adc >> MIC_ATTEN);
si5351.freq_calc_fast(df);
```

**AM:**
```cpp
amp = in;  // Control de amplitud directo
si5351.freq_calc_fast(0);
```

**FM:**
```cpp
int16_t df = in;
si5351.freq_calc_fast(df);
```

---

## 7. Resumen del Flujo de Señal

### 7.1 Flujo TX Completo

```
MIC --> ADC --> ssb() --> Fase Calc --> freq_calc_fast() --> SI5351 --> PA --> ANT
              |           |
              |           v
              |        VOX (umbral de amplitud)
              |
              v
        Compresor/EQ/Pre-énfas
```

### 7.2 Flujo RX Completo

```
ANT --> QSD --> ADC --> CIC Decimate --> Hilbert --> Demod --> AGC/NR/Filter --> PWM
                      |                  |
                      v                  v
                 Muestra I/Q           Slow DSP (por modo)
```

---

## 8. Estructura de Archivos

| Archivo | Líneas | Propósito |
|---------|--------|-----------|
| `usdx_plus_orange.ino` | 7250 | Firmware principal |
| `usdx_settings.h` | 143 | Configuración |
| `usdx_filter.h` | 221 | Filtros DSP |

### Referencias de Líneas Clave

| Componente | Líneas |
|-----------|--------|
| Compresor de Voz | 2005-2021 |
| EQ Micrófono | 2023-2034 |
| Pre-énfasis | 2040-2044 |
| Generación SSB | 2036-2129 |
| Hilbert TX | 2069-2087 |
| arctan3() | 1951-1967 |
| ISR dsp_tx() | 2160-2234 |
| Decimación CIC | 3228-3418 |
| Hilbert RX | 2983-3009 |
| slow_dsp() | 2970-3174 |
| process_agc() | 2712-2740 |
| process_nr() | 2881-2941 |
| filt_var() | usdx_filter.h:46-222 |
| Decodificador CW | 2460-2530+ |

---

## Apéndice: Archivos Testbench

Los archivos testbench están en la carpeta `test/`:

| Archivo | Descripción |
|---------|-------------|
| `testbench_ssb_modulation.cpp` | Test genérico de modulación SSB |
| `testbench_exact_ssb.cpp` | Verificación de algoritmos exactos |
| `testbench_tx_7100_lsb.cpp` | Simulación TX a 7.1MHz |

**Compilación:**
```bash
cd test
g++ -std=c++11 -O2 -o <nombre> <archivo>.cpp
./<nombre>
```

---

*Documentación generada para uSDX Plus Orange v5.14*
