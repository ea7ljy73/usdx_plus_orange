# Guía — Estación de fonía SSB desde el PC (uSDX Plus Orange v2.x)

Manejar el uSDX en **voz SSB** desde el ordenador: control por CAT, audio de RX
y TX por una tarjeta de sonido USB, procesado de voz en el PC y cascada ancha
con un SDR externo como panadaptador.

> **Estado: sin probar en hardware.** Esta guía describe el montaje previsto;
> no requiere cambios de firmware (v2.1.0 de producción tal cual). Valida cada
> paso en orden y no transmitas hasta que la recepción y el CAT funcionen.
> TX siempre con antena de ROE baja (< 1,5) o carga de 50 Ω.

---

## 1. Esquema general

```
 Cascos USB (micro) ─► PC ─► EasyEffects (puerta, EQ, compresor, LPF) ─► tarjeta USB (salida)
                                                                            │ divisor + condensador
                                                                            ▼
                                                                       MIC del uSDX
 uSDX salida auriculares ─────────────► tarjeta USB (entrada línea) ─► PC ─► cascos
 uSDX UART (PD0/PD1) ◄── adaptador USB-TTL 5 V ──► flrig (frecuencia, modo, PTT)
 SDR (con SU antena) ─► SDR++ (solo cascada, sin audio) ◄── rigctld ◄── flrig
```

Un único programa abre el puerto serie (**flrig**); el resto se cuelga de él.

---

## 2. Material

| Pieza | Notas |
|---|---|
| Adaptador USB-TTL **de 5 V** | CH340, CP2102 o FTDI. Solo RX, TX y GND (DTR no hace falta: se graba por USBasp) |
| 2 resistencias de 1 kΩ + 1 diodo 1N4148 | Coexistencia UART / LCD (§3) |
| Tarjeta de sonido USB | Con entrada de **línea** (o micro) y salida estéreo |
| Divisor de TX: 10 kΩ + 100 Ω, condensador 1-10 µF | Baja el nivel del PC al de un micro (§4) |
| 2 aisladores de audio 600:600 (recomendado) | Evitan bucles de masa y zumbido |
| Ferritas de clip | En USB, audio y alimentación, contra RF en el audio |
| Cascos con micro (USB o jack) | El micro con el que hablarás |

---

## 3. CAT: conexión UART

El LCD comparte los pines del UART (**PD0 = RXD = LCD_D4, pin 2** y
**PD1 = TXD = LCD_D5, pin 3** del ATmega328P). El firmware (igual que legacy)
corta el puerto serie mientras escribe en el LCD y usa **PC2 (pin 25)** para
enmascarar esas escrituras con un diodo. Cableado de legacy
(`usdx-legazy.ino`, comentario de `pre()`):

```
ATmega PD0 / RXD (pin 2) ──[1 kΩ]──────────── TX del adaptador
ATmega PD1 / TXD (pin 3) ──[1 kΩ]──┬───────── RX del adaptador
                                   │
ATmega PC2 (pin 25) ──►|───────────┘   (1N4148: ánodo en PC2, cátodo en el nodo)
GND ───────────────────────────────────────── GND del adaptador
```

- **Comprueba los pines en el esquema de tu placa** antes de soldar.
- PC2 es también la entrada de micro (ADC2): con CAT activo se usa como salida
  en RX y se libera en TX para muestrear el micro. No conectes nada más a PC2.
- Mientras el LCD se escribe no se reciben datos (heredado). Con el CAT activo
  el firmware deja de refrescar la pantalla en reposo (S-meter apagado), así que
  solo hay escrituras cuando cambia algo; usa aun así un sondeo ≥ 120 ms (§5).

**Prueba sin transmitir** (terminal serie a **38400 8N1**, p. ej.
`picocom -b 38400 /dev/ttyUSB0`):

```
ID;   ->  ID020;
FA;   ->  FA00007100000;
MD;   ->  MD1;      (1 LSB, 2 USB, 3 CW, 4 FM, 5 AM)
IF;   ->  IF00007100000...;
```

Comandos (TS-480): `FA` `FB` `FR`/`FT` `IF` `MD` `TX`/`TX0-2` `RX` `RT`
`RU`/`RD` `RC` `AG0` (volumen) `SM0` (S-meter) `GT` (AGC) `RA` (atenuador)
`NR`/`RL` `PC` (potencia) `MG` (micro) `PR` (TX Comp) `KS` (keyer) `VX`
(VOX) y `EX` (cualquier parámetro del menú por su número), más `ID` `PS` `AI`
`XT1` `FL0` `RS`. Lo desconocido o fuera de rango responde `?;`. Sin split
(`FT` = `FR`). Referencia: `usdxcontrol/docs/cat-extensiones-peticion.md`.

En Linux, si el puerto da *permiso denegado*:
`sudo usermod -aG dialout $USER` y vuelve a iniciar sesión.

---

## 4. Audio

### RX: uSDX → PC
- Salida de auriculares del uSDX → (aislador) → **entrada de línea** de la
  tarjeta USB.
- Empieza con el volumen del uSDX bajo y súbelo hasta que los picos queden en
  unos −12 dBFS en el PC. **Que no recorte** (en las grabaciones del
  2026-10-18 el recorte estaba en la cadena de grabación).

### TX: PC → MIC del uSDX
La salida de una tarjeta de sonido da del orden de 1 V; un micro electret,
unos milivoltios. Atenúa unos 40 dB y bloquea la continua:

```
Salida tarjeta ──[10 kΩ]──┬──[1-10 µF]──► MIC del uSDX
                          │
                        [100 Ω]
                          │
GND tarjeta ──────────────┴─────────────► GND del uSDX
```

- El condensador evita que la polarización del electret entre en la tarjeta
  (y al revés).
- Ajuste fino: volumen de salida del PC y, si aún sobra nivel, `28 Mic Atten`
  (6 dB por paso).

---

## 5. Ajustes en la radio y en flrig

### Menú del uSDX (fonía con procesado en el PC)

| Menú | Valor | Por qué |
|---|---|---|
| 29 DIGI Mode | **ON** | Path de TX plano: sin `TX Comp`, sin `TX LoCut` ni realce del micro. El procesado lo hace el PC; no dupliques |
| 18 VOX | **OFF** | PTT por CAT desde flrig |
| Modo | **LSB** (160-40 m) / **USB** (20 m y arriba) | DIGI Mode no cambia la banda lateral |
| 20 TX Drive | **4** | Igual que la configuración aconsejable (README) |
| 28 Mic Atten | 0 (subir si sobra nivel) | |
| 30 TX Comp / 31 TX LoCut | indiferente | Con DIGI ON no actúan |

Sin PC (micro directo), vuelve a la configuración aconsejable del README:
`DIGI Mode OFF`, `TX Comp 1`, `TX LoCut 2`.

### flrig
- Rig: **Kenwood TS-480** · puerto `/dev/ttyUSB0` · **38400** · 8N1 ·
  sin RTS/DTR · PTT **por CAT**.
- Sondeo (*poll*) lento: 500-1000 ms, con reintentos activados (por la
  coexistencia con el LCD).
- flrig ya puede leer el S-meter (`SM0`) y manejar volumen (`AG0`), AGC (`GT`),
  potencia (`PC`), micro (`MG`), NR y atenuador. Lo que no implementa (split,
  filtros SH/SL, memorias…) responde `?;`: es normal.

---

## 6. Procesado de voz en el PC (EasyEffects, Linux)

EasyEffects procesa el micro de los cascos. Cadena en la pestaña **Entrada**,
en este orden:

| Efecto | Ajuste de partida | Para qué |
|---|---|---|
| Puerta de ruido | Umbral −45 dB, ataque 1 ms, liberación 150 ms | Silencio entre frases (no transmites ruido de la habitación) |
| Filtro paso alto | 200 Hz, 24 dB/oct | Quita graves y retumbe que gastan potencia |
| Ecualizador | +3 dB de 1,5 a 2,3 kHz; −3 dB en 300-500 Hz | Inteligibilidad, voz menos "hueca" |
| Compresor | Ratio 3:1, umbral −24 dB, ataque 5 ms, liberación 100 ms, ganancia de compensación +6 dB | Más potencia media, suave |
| **Filtro paso bajo** | **2,4 kHz, 24-48 dB/oct** | **Obligatorio**, ver aviso |
| Limitador | Techo −3 dB | Que nada sature la entrada de micro |

> **Aviso — paso bajo a 2,4 kHz.** La TX del uSDX muestrea el micro a unos
> 4,8 kHz: lo que entre por encima de ~2,4 kHz se **pliega** (aliasing) y
> aparece como distorsión dentro del canal. Con `DIGI Mode ON` el firmware no
> filtra; el filtro lo pone el PC.

### Llevar la voz procesada a la tarjeta USB
EasyEffects deja la voz procesada en su fuente virtual; hay que enviarla a la
salida de la tarjeta USB con un *loopback*:

```bash
# nombre de la salida de la tarjeta USB
pactl list short sinks
# enviar la voz procesada a esa salida (20 ms de latencia)
pactl load-module module-loopback source=easyeffects_source \
      sink=<nombre_de_la_tarjeta_usb> latency_msec=20
# para quitarlo: pactl unload-module module-loopback
```

- Funciona igual con PipeWire (compatibilidad `pactl`).
- Mientras el loopback esté activo, todo lo que digas va a la entrada de micro
  del uSDX, pero **solo sale al aire con PTT**.
- Escucha la RX por los cascos y **no** por la tarjeta USB, para no crear un
  bucle RX → TX.

En Windows el equivalente es Equalizer APO o VoiceMeeter; la cadena es la misma.

---

## 7. Cascada ancha con el SDR (SDR++)

El uSDX no puede enviar su IQ al PC (UART lento y CPU al límite), así que la
cascada ancha la pone el SDR con **su propia antena**:

1. flrig es el dueño del puerto serie. Haz de puente con Hamlib:
   ```bash
   rigctld -m 4 -t 4532      # modelo 4 = FLRig (lee y fija frecuencia vía flrig)
   ```
2. En SDR++, activa el módulo **Rigctl Client**, `localhost:4532`: la
   sintonía del SDR seguirá al uSDX.
3. **SDR++ sin audio** (solo cascada): escuchas por el uSDX.
4. Pon la frecuencia central del SDR **a un lado** de la señal (p. ej. centro
   en 7,090 y escucha en 7,100 MHz): evita el pico de continua del SDR, que en
   las grabaciones del 2026-10-18 apareció como retumbe < 30 Hz.
5. En TX la cascada del SDR se saturará (es normal); para verte sin saturar,
   ganancia al mínimo y antena del SDR desconectada (ver la guía de pruebas).

---

## 8. Puesta en marcha (orden)

- [ ] CAT: `ID;` responde `ID020;` en el terminal serie
- [ ] flrig lee y cambia frecuencia y modo
- [ ] RX: audio del uSDX en el PC, sin recorte ni zumbido
- [ ] EasyEffects: la voz procesada suena bien en un monitor local
- [ ] Loopback hacia la tarjeta USB y nivel ajustado **sin transmitir**
- [ ] SDR++ sigue la frecuencia del uSDX
- [ ] Primera TX: antena con ROE < 1,5, `TX Drive 4`, pasadas cortas,
      escuchándote en el SDR o pidiendo control a otra estación
- [ ] Graba 10 s en el SDR (mejor en IQ/baseband) y compárala con las del
      2026-10-18 en `audios/`

---

## 9. Problemas típicos

| Síntoma | Causa probable | Qué hacer |
|---|---|---|
| `ID;` no responde | Baudios, TX/RX cruzados, adaptador de 3,3 V | 38400 8N1; intercambia TX/RX; adaptador de 5 V |
| flrig pierde la conexión a ratos | Refresco del LCD (coexistencia) | Sondeo más lento, reintentos |
| Zumbido en RX o en TX | Bucle de masa por el USB | Aisladores 600:600; alimentar el uSDX aparte |
| Pitidos o realimentación al transmitir | RF entrando en el audio | Ferritas en todos los cables; ROE baja |
| Voz distorsionada o áspera | Nivel alto o sin LPF a 2,4 kHz | Baja la salida del PC / sube `Mic Atten`; revisa el LPF |
| No sale voz en TX | Loopback sin cargar, DIGI ON sin nivel | `pactl list short modules`; sube la salida de la tarjeta |
| Se transmite solo | VOX activo con audio del PC | `VOX OFF`; PTT solo por flrig |
