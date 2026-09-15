# Instrumentación DSP y análisis de calidad para uSDX Plus Orange V2

## Proyecto objetivo

**Repositorio:** `ea7ljy73/usdx_plus_orange`  
**Rama:** `refactor-v2`  
**Directorio:** `usdx_plus_orange_v2`

> Este documento es una guía de teoría, diagnóstico y diseño de firmware orientada específicamente al uSDX Plus Orange V2. Los nombres exactos de funciones, ISR y variables deben verificarse contra el checkout local antes de aplicar modificaciones.

---

## 0. Alcance y objetivo

La idea es añadir al uSDX una pequeña plataforma de **instrumentación DSP** que permita observar y medir su propia cadena de audio, DSP, I/Q y transmisión.

No se pretende convertir el ATmega328P en un analizador de laboratorio. El objetivo realista es conseguir mediciones **relativas, repetibles y útiles para ingeniería**:

- RMS
- pico
- DC offset
- clipping
- factor de cresta
- ruido
- SNR
- FFT
- THD / THD+N
- prueba de dos tonos
- IMD3 / IMD5
- balance I/Q
- fase I/Q
- supresión de portadora
- supresión de banda lateral opuesta
- respuesta de filtros
- diagnóstico ADC
- calibración

La arquitectura histórica de uSDX emplea un ATmega328P, ADC para adquisición, procesamiento digital, Si5351 para generación de reloj/fase y PWM para la envolvente de TX. La arquitectura y las pruebas de dos tonos documentadas para uSDX sirven como base conceptual, pero los valores exactos del Orange V2 deben medirse en el hardware real.

### Regla de seguridad RF

Durante TX:

```text
TX
 │
 ▼
Filtro LPF
 │
 ▼
Carga ficticia 50 Ω
 │
 ├──► wattmeter
 │
 └──► atenuador ──► SDR / analizador
```

**Nunca conectar directamente una salida de potencia RF a un ADC, tarjeta de sonido o SDR.**

---

# 1. Arquitectura mental del uSDX

Conviene separar el firmware en cuatro dominios:

1. tiempo real
2. DSP
3. control RF
4. interfaz de usuario

Un cambio en DSP puede afectar al ADC, PWM, I2C/Si5351, pantalla, botones y temporización.

Una representación simplificada:

```text
                 ┌─────────────────┐
Audio / I/Q ───► │ ADC / captura   │
                 └────────┬────────┘
                          │
                          ▼
                 ┌─────────────────┐
                 │ DSP normal      │
                 │ filtros / AGC   │
                 │ Hilbert / I-Q   │
                 └────────┬────────┘
                          │
             ┌────────────┼────────────┐
             ▼            ▼            ▼
          Audio          FFT          I/Q
             │            │            │
             └────────────┼────────────┘
                          ▼
                   Engineering UI
```

En TX:

```text
Audio
  │
  ▼
DSP / I-Q
  │
  ├────────► amplitud ──► PWM
  │
  └────────► fase ──────► Si5351
                           │
                           ▼
                           PA
                           │
                           ▼
                          LPF
                           │
                           ▼
                       50 Ω / antena
```

---

# 2. Muestreo y ADC

Si una señal se muestrea a frecuencia `fs`, la frecuencia de Nyquist es:

```text
fN = fs / 2
```

La frecuencia de muestreo es:

```text
fs = 1 / Ts
```

donde `Ts` es el periodo entre muestras.

## Aliasing

Si aparece una señal por encima de Nyquist, puede plegarse hacia la banda útil.

Por ello hay dos defensas:

```text
Señal analógica
      │
      ▼
Filtro anti-alias
      │
      ▼
ADC
      │
      ▼
DSP
```

El filtro analógico anterior al ADC es tan importante como el filtro digital posterior.

## RAM del ATmega328P

La RAM es uno de los recursos más críticos.

Un buffer de `N` muestras de 16 bits consume aproximadamente:

```text
RAM = 2 × N bytes
```

Para I + Q:

```text
RAM = 4 × N bytes
```

Ejemplo:

| N | 1 canal int16 | I + Q int16 |
|---:|---:|---:|
| 64 | 128 B | 256 B |
| 128 | 256 B | 512 B |
| 256 | 512 B | 1 KB |
| 512 | 1 KB | 2 KB |

Por ello, una FFT de 512 puntos con I/Q no es una estrategia razonable para mantenerla permanentemente en un 328P con el resto del transceptor funcionando.

---

# 3. dB, RMS, pico y dBFS

Para amplitud o tensión:

```text
dB = 20 log10(A / Aref)
```

Para potencia:

```text
dB = 10 log10(P / Pref)
```

## RMS

```text
RMS = sqrt( Σ x[n]² / N )
```

## Pico

```text
Peak = max(|x[n]|)
```

## Factor de cresta

```text
Crest Factor = Peak / RMS
```

En dB:

```text
Crest Factor(dB) = 20 log10(Peak / RMS)
```

El RMS describe el nivel medio. El pico sirve para comprobar margen y clipping.

## dBFS

En el dominio digital es muy útil usar dBFS:

- `0 dBFS`: máximo digital
- valores negativos: margen respecto al máximo

No debe confundirse dBFS con dBm.

Para convertir dBFS a una magnitud RF absoluta hace falta una calibración.

---

# 4. Monitor de audio

El primer instrumento recomendable es un **Audio Monitor**.

Debe poder mostrar:

```text
AUDIO MONITOR
--------------
DC       +2
RMS      38
PEAK     61
CREST    1.61
CLIP      0
NOISE    11
```

## Métricas

| Métrica | Significado |
|---|---|
| DC offset | Desplazamiento del centro ADC |
| RMS | Nivel medio |
| Peak | Máximo instantáneo |
| Crest factor | Pico respecto a RMS |
| Clipping | Número de muestras saturadas |
| Noise RMS | Ruido de fondo |

## Cálculo eficiente

No conviene ejecutar operaciones caras por muestra.

Conceptualmente:

```c
sum += x;
energy += (int32_t)x * x;

if (x > CLIP_HIGH || x < CLIP_LOW)
    clip_count++;
```

Al terminar el bloque:

```c
dc = sum / N;
rms = integer_sqrt(energy / N);
peak = max_abs;
crest = peak / rms;
```

La raíz cuadrada puede ejecutarse solamente cuando se actualiza la pantalla.

---

# 5. SNR y ruido

Una aproximación básica:

```text
SNR = 20 log10(RMS_signal / RMS_noise)
```

El ruido debe medirse con una condición conocida:

- entrada sin señal
- filtro definido
- ganancia definida
- mismo ancho de banda

Para una medición más seria, la FFT permite excluir la fundamental y medir la energía de ruido dentro de una banda.

---

# 6. FFT

La FFT transforma muestras temporales en información espectral.

Para `N` muestras:

```text
Δf = fs / N
```

Ejemplo:

```text
fs = 8000 Hz
N  = 128

Δf = 8000 / 128
   = 62.5 Hz
```

## Leakage

Si la frecuencia de entrada no coincide exactamente con un bin, la energía se reparte entre bins.

Las ventanas reducen este efecto.

| Ventana | Característica |
|---|---|
| Rectangular | Máxima resolución, mucho leakage |
| Hann | Buen compromiso |
| Hamming | Buen rechazo lateral |
| Blackman | Muy buen rechazo, menor resolución |

Para el 328P recomiendo empezar con:

- FFT 64
- FFT 128

y medir CPU/RAM antes de aumentar tamaño.

---

# 7. Analizador FFT

Flujo:

```text
ADC
 │
 ▼
Buffer
 │
 ▼
Eliminar DC
 │
 ▼
Ventana
 │
 ▼
FFT
 │
 ▼
Magnitud
 │
 ├──► Pico
 ├──► Ruido
 └──► Frecuencias
```

Para un modo Engineering no es necesario mostrar todos los bins.

Se puede calcular:

- frecuencia del pico
- magnitud del pico
- nivel medio de ruido
- algunos bins seleccionados

Una FFT pequeña y estable es preferible a una FFT grande que perjudique al transceptor.

---

# 8. THD

La distorsión armónica total se define como:

```text
THD =
sqrt(V2² + V3² + V4² + ...) / V1
```

En porcentaje:

```text
THD% = 100 × THD
```

En dB:

```text
THD(dB) = 20 log10(THD)
```

`V1` es la fundamental.

`V2`, `V3`, etc. son los armónicos.

## THD+N

THD+N incluye además el ruido dentro del ancho de banda de medición.

En un 328P es preferible hacer una estimación basada en FFT o detectores selectivos, no intentar replicar un analizador de audio profesional.

---

# 9. SSB

En SSB queremos transmitir:

- una banda lateral
- con portadora suprimida
- y con la banda lateral opuesta muy atenuada

Una señal analítica puede expresarse como:

```text
xa(t) = x(t) + j Hilbert{x(t)}
```

I y Q forman una señal compleja:

```text
z(t) = I(t) + jQ(t)
```

La calidad de la separación depende del filtro, amplitud y fase entre I/Q.

---

# 10. I/Q

Para una muestra:

```text
z = I + jQ
```

Magnitud:

```text
|z| = sqrt(I² + Q²)
```

Fase:

```text
phase = atan2(Q, I)
```

## Qué medir

| Medición | Qué detecta |
|---|---|
| RMS(I) | Nivel I |
| RMS(Q) | Nivel Q |
| diferencia I/Q | Desbalance de amplitud |
| fase I/Q | Error de cuadratura |
| imagen | Calidad final de rechazo |

Un error de amplitud o fase produce fuga de imagen/banda lateral opuesta.

---

# 11. Prueba de dos tonos

Es una de las pruebas más importantes del TX.

Se generan dos tonos:

```text
f1 = 700 Hz
f2 = 1900 Hz
```

Conceptualmente:

```text
700 Hz ─────┐
            ├──► TX ──► FFT
1900 Hz ────┘
```

Productos de tercer orden:

```text
2f1 - f2
2f2 - f1
```

Con estos tonos:

```text
2×700 - 1900 = -500 Hz
2×1900 - 700 = 3100 Hz
```

Los signos se interpretan respecto a la portadora y banda lateral.

Productos de quinto orden:

```text
3f1 - 2f2
3f2 - 2f1
```

## Tabla

| Producto | Fórmula | Orden |
|---|---|---:|
| Fundamental | f1 | 1 |
| Fundamental | f2 | 1 |
| IMD3 inferior | 2f1-f2 | 3 |
| IMD3 superior | 2f2-f1 | 3 |
| IMD5 | 3f1-2f2 | 5 |
| IMD5 | 3f2-2f1 | 5 |

---

# 12. IMD en dBc

Si el producto de intermodulación tiene amplitud `Vimd` y la fundamental de referencia tiene amplitud `Vfund`:

```text
IMD(dBc) = 20 log10(Vimd / Vfund)
```

Ejemplo:

```text
fundamental = 0 dBc
IMD3 = -33 dBc
```

Cuanto más negativo, mejor.

Es importante repetir siempre las mismas condiciones:

- banda
- filtro
- nivel de audio
- alimentación
- potencia
- carga
- temperatura
- firmware
- frecuencia

---

# 13. Generador de dos tonos

El generador interno puede usar una tabla seno y dos acumuladores de fase.

Conceptualmente:

```c
phase1 += phase_inc1;
phase2 += phase_inc2;

tone = sine_table[phase1] +
       sine_table[phase2];

tone = tone * level;
```

Hay que dejar margen para que la suma no produzca clipping.

El generador debe probarse por separado antes de utilizarlo para evaluar el TX.

---

# 14. Detección selectiva para IMD

Una FFT completa no siempre es necesaria.

Como conocemos las frecuencias que queremos medir, podemos utilizar detección síncrona o correlación:

```text
muestra × sin(ωt)
muestra × cos(ωt)
```

y obtener una estimación de amplitud.

Ventajas:

- menos RAM
- menos CPU
- sólo mide las frecuencias importantes
- excelente para IMD

Esto puede ser una de las mejores soluciones para el ATmega328P.

---

# 15. Portadora y banda lateral opuesta

El analizador de TX debería mostrar:

```text
TX TEST
----------------
F1       -0.0 dBc
F2       -0.3 dBc
IMD3 L  -33.2 dBc
IMD3 H  -34.0 dBc
Carrier -47.0 dBc
Opp-SB  -46.0 dBc
Noise   -58.0 dBc
```

Tres mediciones son especialmente importantes:

1. señal útil
2. portadora residual
3. banda lateral opuesta

Después hay que investigar espurias alejadas.

---

# 16. Espurias y armónicos

Las espurias pueden proceder de:

- PWM
- reloj
- Si5351
- PA
- alimentación
- conmutación
- no linealidad
- filtrado insuficiente
- mezcla digital
- acoplos

El análisis RF debe realizarse con carga de 50 Ω y atenuación adecuada.

---

# 17. Filtros

Un filtro tiene una respuesta:

```text
H(f)
```

Puede medirse mediante barrido.

Conceptualmente:

```c
for (f = f_start; f <= f_stop; f += step) {
    set_generator(f);
    wait_settle();
    level = measure_rms();
    store(f, level);
}
```

Después se normaliza:

```text
level_dB = level_dB - max_level_dB
```

y se pueden encontrar:

- frecuencia central
- −3 dB
- −6 dB
- −20 dB
- ancho de banda

Es importante distinguir:

**barrido digital interno**

de

**barrido de la respuesta analógica/RF completa**.

---

# 18. Modo Engineering propuesto

```text
ENGINEERING
------------
1 Audio monitor
2 FFT analyzer
3 Two-tone generator
4 IMD analyzer
5 I/Q analyzer
6 TX modulation
7 Filter sweep
8 Noise measurement
9 ADC diagnostics
A Calibration
B Signal generator
C CPU / DSP diagnostics
```

Cada modo debe poder entrar y salir sin dejar temporizadores, ISR o generadores activos.

---

# 19. Arquitectura de software

La estrategia recomendada es reutilizar las muestras existentes.

```text
            adquisición existente
                    │
                    ▼
                 DSP uSDX
                    │
              diagnostic tap
                    │
       ┌────────────┼────────────┐
       ▼            ▼            ▼
     Audio          FFT          I/Q
       │            │            │
       └────────────┼────────────┘
                    ▼
             Engineering UI
```

No conviene crear un segundo pipeline DSP completo.

El mejor punto de observación depende de lo que queramos medir.

Ejemplo:

```text
ADC_RAW
   │
   ├──► diagnóstico ADC
   │
   ▼
Filtro
   │
   ├──► calidad de audio
   │
   ▼
AGC
   │
   ├──► audio procesado
   │
   ▼
TX_COMPLEX
   │
   ├──► I/Q
   └──► modulación
```

---

# 20. Entrada y salida del modo Engineering

Conceptualmente:

```c
enter_engineering(mode)
{
    save_runtime_state();
    stop_nonessential_ui_tasks();
    configure_test_mode();
    engineering_active = true;
}

exit_engineering()
{
    engineering_active = false;
    stop_test_generator();
    restore_runtime_state();
    clear_buffers();
    redraw_normal_ui();
}
```

La función de salida es tan importante como la de entrada.

---

# 21. Uso eficiente de CPU

Evitar:

- `float` por muestra
- `sqrt()` por muestra
- `atan2()` por muestra
- FFT continua durante TX normal
- buffers enormes

Preferir:

- enteros
- tablas
- acumuladores
- cálculos por bloques
- raíz cuadrada al actualizar pantalla
- detección selectiva
- FFT sólo en Engineering Mode

---

# 22. Diagnóstico ADC

Pantalla propuesta:

```text
ADC DIAG
---------
MEAN    512
MIN     480
MAX     545
RMS      11
CLIP      0
RANGE    65
```

Esto permite detectar:

- polarización incorrecta
- clipping
- poco margen
- ruido excesivo
- ausencia de señal
- ganancia excesiva

El diagnóstico ADC debería ser la primera herramienta que se valide.

---

# 23. Calibración

Separar:

### Calibración interna

- offset ADC
- ganancia ADC
- referencia de frecuencia
- parámetros DSP

### Calibración de sistema

- S-meter
- potencia
- nivel RF
- IMD

Tabla:

| Calibración | Referencia |
|---|---|
| ADC offset | entrada sin señal |
| ADC gain | tono conocido |
| Frecuencia | frecuencímetro / patrón |
| S-meter | generador RF |
| Potencia | wattmeter + 50 Ω |
| IMD | dos tonos + receptor/analizador |

La frecuencia del Si5351 debe verificarse con una referencia externa si se quiere precisión.

---

# 24. Procedimiento de validación

No implementar todo a la vez.

### Fase 1

Compilar el firmware actual sin instrumentación.

### Fase 2

Añadir monitor ADC.

### Fase 3

Añadir RMS / pico / DC.

### Fase 4

Añadir FFT 64.

### Fase 5

Añadir FFT 128.

### Fase 6

Añadir generador de dos tonos.

### Fase 7

Añadir IMD3.

### Fase 8

Añadir IMD5.

### Fase 9

Añadir I/Q.

### Fase 10

Añadir calibración.

### Fase 11

Validar RF externamente.

### Fase 12

Optimizar CPU/RAM.

---

# 25. Hoja de medición

```text
Firmware: usdx_plus_orange_v2
Commit: __________________

Banda: __________________
Modo: USB / LSB
Filtro: _________________
TX Drive: _______________
Alimentación: ___________ V
Carga: 50 Ω

Instrumento: ____________
Atenuación: _____________ dB

Tono 1: _________________ Hz
Tono 2: _________________ Hz

F1: _____________________ dBc
F2: _____________________ dBc
IMD3 L: _________________ dBc
IMD3 H: _________________ dBc
IMD5 L: _________________ dBc
IMD5 H: _________________ dBc
Carrier: ________________ dBc
Opp-SB: _________________ dBc
Noise: __________________ dBc
```

---

# 26. Checklist del firmware

```text
[ ] Compila con la configuración actual
[ ] RAM dentro de límites
[ ] Flash dentro de límites
[ ] No rompe ISR
[ ] ADC monitor
[ ] RMS
[ ] Peak
[ ] DC offset
[ ] Clipping
[ ] FFT 64
[ ] FFT 128
[ ] Generador
[ ] Dos tonos
[ ] IMD3
[ ] IMD5
[ ] Portadora
[ ] Banda lateral opuesta
[ ] I/Q
[ ] Ruido
[ ] Calibración
[ ] Cleanup al salir
[ ] Medición RF mediante carga/atenuación
```

---

# 27. Qué NO debemos hacer

| Error | Problema |
|---|---|
| FFT continua durante TX | Consume CPU |
| Float por muestra | Muy costoso en 328P |
| Buffers enormes | Falta de RAM |
| Cambiar ISR sin medir | Puede romper temporización |
| RF directa al ADC | Riesgo de daño |
| Interpretar dBFS como dBm | Falta calibración |
| Comparar IMD sin condiciones | Resultado no trazable |
| Cambiar diez cosas simultáneamente | No se identifica la causa |

---

# 28. Plan de desarrollo recomendado para Orange V2

## Paso 1 — Conocer el código real

Antes de modificar nada:

- identificar ISR de ADC
- identificar buffer de audio
- identificar variables I/Q
- localizar generación TX
- localizar PWM
- localizar acceso Si5351
- localizar UI
- comprobar uso real de RAM
- comprobar uso real de Flash

## Paso 2 — Crear un diagnostic tap

Crear una interfaz lógica del estilo:

```text
diag_feed_adc()
diag_feed_audio()
diag_feed_iq()
diag_feed_tx()
```

pero sin duplicar los datos.

## Paso 3 — Implementar métricas básicas

Orden:

```text
DC
↓
Peak
↓
RMS
↓
Clipping
↓
Noise
```

## Paso 4 — FFT

Primero 64 puntos.

Después 128.

Sólo aumentar si las mediciones demuestran que el procesador puede asumirlo.

## Paso 5 — Dos tonos

Generador interno.

## Paso 6 — IMD

Primero IMD3.

Después IMD5.

## Paso 7 — I/Q

Medir amplitud, fase e imagen.

## Paso 8 — Calibración

Guardar parámetros calibrados.

---

# 29. Interpretación de resultados

Un resultado de:

```text
IMD3 = -34 dBc
```

no significa automáticamente que el PA sea responsable.

Puede intervenir:

```text
Generador
   ↓
DAC/PWM
   ↓
DSP
   ↓
I/Q
   ↓
Si5351
   ↓
PA
   ↓
LPF
   ↓
Atenuador
   ↓
Instrumento
```

La distorsión puede originarse en cualquier bloque.

Por eso es útil medir en varios puntos:

1. audio generado
2. audio después de DSP
3. I/Q
4. salida RF

La comparación permite localizar dónde aparece el producto.

---

# 30. Estrategia de localización de distorsión

Ejemplo:

### Caso A

```text
Audio generator:
THD = -70 dB

TX:
IMD3 = -33 dBc
```

La distorsión probablemente aparece después del generador.

### Caso B

```text
Audio generator:
THD = -40 dB

TX:
IMD3 = -35 dBc
```

El generador/DSP ya está introduciendo distorsión significativa.

### Caso C

IMD aumenta bruscamente al aumentar potencia:

```text
1 W  → -40 dBc
2 W  → -38 dBc
3 W  → -30 dBc
```

Esto es una pista de no linealidad en la etapa RF, alimentación, drive o control de envolvente.

---

# 31. Principio de ingeniería más importante

No debemos intentar medir todo al mismo tiempo.

La metodología correcta es:

```text
medir
  ↓
establecer referencia
  ↓
cambiar UNA cosa
  ↓
volver a medir
  ↓
comparar
```

Así el firmware se convierte en una herramienta científica y no solamente en una colección de indicadores.

---

# 32. Chuleta de fórmulas

| Concepto | Fórmula |
|---|---|
| Nyquist | `fN = fs / 2` |
| Resolución FFT | `Δf = fs / N` |
| RMS | `sqrt(Σx²/N)` |
| dB amplitud | `20 log10(A/Aref)` |
| dB potencia | `10 log10(P/Pref)` |
| Crest factor | `Peak/RMS` |
| SNR | `20 log10(RMSsignal/RMSnoise)` |
| THD | `sqrt(V2²+V3²+...)/V1` |
| IMD | `20 log10(Vimd/Vfund)` |
| Señal compleja | `z = I + jQ` |
| Magnitud I/Q | `sqrt(I²+Q²)` |
| Fase I/Q | `atan2(Q,I)` |

---

# 33. Referencias técnicas

La documentación pública de uSDX describe la arquitectura SDR, el uso del ATmega328P, ADC, procesamiento I/Q, Hilbert, filtros DSP, AGC, generación SSB mediante fase del Si5351 y PWM de envolvente, además de pruebas de dos tonos.

El firmware original contiene también una variante específica para White Buttons.

Estas referencias deben considerarse documentación de contexto. Para implementar modificaciones exactas sobre Orange V2 se debe trabajar sobre el código de la rama `refactor-v2` y verificar los puntos reales de adquisición, ISR, DSP, Si5351, PWM y UI.

---

# 34. Conclusión

El enfoque más adecuado para el uSDX Plus Orange V2 es construir una instrumentación ligera y seleccionable:

```text
ADC monitor
    ↓
RMS / Peak / DC / clipping
    ↓
FFT pequeña
    ↓
Generador de dos tonos
    ↓
IMD3 / IMD5
    ↓
I/Q
    ↓
Portadora / Opp-SB
    ↓
Calibración
```

El ATmega328P impone restricciones fuertes de RAM, Flash y CPU. Por ello, las funciones deben ejecutarse principalmente bajo demanda, reutilizar buffers existentes y preferir algoritmos enteros o detectores selectivos.

El resultado puede ser mucho más útil que un simple analizador: un **modo Engineering integrado en el propio transceptor**, capaz de ayudar a entender qué ocurre en el DSP y a comparar objetivamente cambios de firmware y hardware.
