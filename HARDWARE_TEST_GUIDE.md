# Guía de Prueba en Hardware — uSDX Plus Orange

Firmware modular en la raíz del proyecto. Esta guía cubre
el flasheo, las verificaciones paso a paso de RX/TX/CW/menú/CAT, y qué esperar
en cada prueba. Incluye un apartado de resolución de problemas.

---

## 1. Requisitos previos

| Ítem | Valor |
|---|---|
| Placa | uSDX WHITE_BUTTONS (ATMEGA328P @20MHz, cristal SI5351 27MHz) |
| Programador | USBasp en ISP (recomendado) o Arduino-ISP |
| Constantes | `F_MCU 20MHz`, `F_XTAL 27MHz`, `WHITE_BUTTONS=1` (ya en `usdx_settings.h`) |
| Build actual | 30788 B flash (95 %), 1188 B RAM (58 %) |

> **IMPORTANTE — antes de grabar:** el firmware **no tiene fuses automáticos**.
> Verifica que el MCU esté a 20MHz externo (CKSEL=1111, CKDIV8=0) como el v1.
> Usar fuses USBasp por defecto (1MHz) hace que el radio no funcione.

---

## 2. Flasheo

### Compilar (desde la raíz del proyecto)
```bash
arduino-cli compile --fqbn arduino:avr:uno .
```
Resultado esperado: `30788 bytes (95%)`, RAM `1188 (58%)`. Sin errores.

### Grabar con USBasp (recomendado)
```bash
arduino-cli upload -b arduino:avr:uno -p usbasp -P usbasp .
```
o manual:
```bash
avrdude -c usbasp -p m328p -U flash:w:build/arduino.avr.uno/usdx_plus_orange.ino.hex:i
```

### Verificar fuses (20MHz)
```bash
avrdude -c usbasp -p m328p -U lfuse:r:-:h
# Esperado LOW=0xFE (o 0xFF), HIGH típico uSDX (boot off)
```
Si el lfuse muestra CKDIV8 activo (no 0xFE/0xFF con div activo), escribe
`avrdude -c usbasp -p m328p -U lfuse:w:0xFE:m`.

---

## 3. Arranque — qué esperar

1. **LCD**: durante ~300 ms muestra `uSDX` + logo en la línea 1 y la versión
   (`v2.1.0`) en la línea 2. Después, la pantalla principal: línea 1 con
   `uSDX` + logo y el S-meter (por defecto en dBm), línea 2 con la flecha de
   VFO, la frecuencia y el modo (`7,074,000 USB`) y una `R` (RX) o `V` (VOX)
   en la última columna.
2. **Voltage**: si mides el pin AREF, ~2.5V (bias SDR).
3. **Cat**: si conectas USB-serial, debe responder `ID020;` al `ID;` (puerto
   38400 baudios). Si no responde, revisa el puerto.

> Si la pantalla se queda en negro: revisar fuses (1MHz vs 20MHz) y el lcd
> wiring (PD0-4+PC4 RS).

---

## 4. Verificación RX

### Prueba 1 — Ruido / S-meter
1. Con una antena o carga, en 40m USB (por defecto 7.074 MHz).
2. La línea 1 muestra el nivel de ruido en el S-meter (formato según el menú
   `S-meter`: dBm, S o barra).
3. **Gira el encoder**: frecuencia cambia por pasos (stepsize predeterminado).
   Vibrar la frecuencia sintonizando estaciones — si hay audio válido, OK.

### Prueba 2 — Audio / filtros
1. Entra en el menú (BL corto en principal). Navega a `Filter BW` (encoder).
   En `EDIT` (BL corto), cambia entre `Full/3000/2400/...`. El audio debe
   volverse más agudo/estrecho a filtros CW.
2. **Volume** (1er parámetro): sube/baja volumen audiblemente.
3. **NR / ATT / AGC**: ajusta y comprueba que el ruido/nivel cambia.

### Prueba 3b — Mejoras de RX (medidas solo en simulador)
Todas arrancan en su valor legacy; actívalas de una en una y compara de oído.
1. **AGC Start = ON**: apaga y enciende sobre una señal débil. El audio debe
   oírse desde el primer segundo (en OFF tarda 2-3 s en subir).
2. **AGC = Slow + AGC Hang = 5**: escucha un QSO de fonía fuerte. Entre
   palabras el ruido NO debe subir ("bombeo"). Compara con `AGC Hang = 1`.
   Al acabar una señal fuerte, una débil debe volver en ~1 s.
   `AGC Hang` no hace nada con `AGC = Fast`.
3. **NR = 2 / 4 / 6**: en banda vacía el ruido baja ~6 / 12 / 18 dB. Con voz
   clara las palabras deben sonar al mismo volumen que con NR 0 y el fondo
   apagarse en las pausas. Anota si corta inicios o colas de palabra.
   Límite esperado: señales muy débiles (<10 dB sobre el ruido) y portadoras
   fijas se atenúan → para DX débil y FT8, NR 0.

### Prueba 3 — S-meter S9
1. Si tienes generador de RF: inyecta ~-73dBm en 7.1MHz USB. El S-meter
   (`_absavg256`→barra) debe subir claramente vs el ruido.

---

## 5. Verificación TX — CON CARGA (dummy) PRIMERO

> **Nunca transmitir a radio abierta sin carga/dummy load (50Ω).** Daños al PA.

> **Paridad garantizada:** el DSP TX (`ssb()`, `freq_calc_fast`, `dsp_tx_*`)
> está verificado byte-idéntico a `usdx-legazy` por el harness de paridad
> (`tests/parity_tx`, 0 mismatches, RMS/peak idénticos). Es aritmética entera
> → determinista → la emisión debe ser igual que legacy en esta placa.

### Prueba 4 — Potencia RF
1. Conecta **dummy load 50Ω** y un medidor de potencia / puente SWR.
2. En USB, pulsa **DIT** (PTT en SSB, legacy 5269) y habla.
3. Esperado: **~3-5W** con `drive=4` (default) y `pwm_max=128`. Si hay poca
   potencia, revisa `TX Drive` y `PA Bias min/max` en el menú.

### Prueba 5 — SSB TX en receptor/SDR
Sintoniza un segundo receptor a la misma frecuencia:
1. Pulsa DIT (PTT) y habla → audio SSB **limpio y natural**.
2. **Portadora suprimida**: NO debe oírse tono continuo (si se oye, la
   portadora no está suprimida).
3. **Banda lateral opuesta suprimida**: el audio no debe sonar "raro"/al revés
   (rechazo de imagen — depende de la fase IQ `rx_ph_q`, restaurada a legacy).
4. Sin **CLICKING** ni cortes (valida el timing ISR).

### Prueba 5b — TX Comp, LoCut y Mic Atten (carga ficticia + segundo receptor)
1. **TX Comp 0 → 1 → 2 → 3** hablando igual: la potencia media debe subir
   (simulador: +3 / +5 / +6 dB) sin que suba el pico. La voz se oye más
   "densa"; en 3 ya suena procesada. Si en 1-2 se oye áspera o hay
   salpicadura fuera del canal, anótalo y vuelve a 0.
2. **TX LoCut 0..3**: quita graves (corte ~100 / 200 / 350 Hz en 1 / 2 / 3).
3. **Mic Atten**: cada paso baja ~6 dB; úsalo si el ALC trabaja siempre a tope.
4. Vigila la temperatura del PA con TX Comp 2-3: sube la potencia media.

### Prueba 5c — TX EnvPre (dos tonos, analizador o SDR)
Preénfasis de envolvente para el conformador RC (10k/10n ×2) entre KEY_OUT y
las puertas del PA. Por defecto 0 (= legacy).
1. Con dos tonos de audio (p. ej. 700 + 1900 Hz) y carga de 50 Ω, mide IMD3
   con `TX EnvPre` 0, 1, 2 y 3.
2. Simulado con el RC del esquema: −20 dBc (0) → −26 / −32 / −28 dBc (1/2/3).
3. **Si tu placa no lleva ese RC, empeora**: quédate con el valor que mida
   mejor en tu radio, o 0.

### Prueba 6 — CW TX (keyer)
1. Modo CW (BR cambia modo). Conecta paddle en DIT/DAH.
2. En el SDR sintoniza **frecuencia − 600Hz** (el v2 transmite a `freq -
   cw_offset`, como legacy 3725). Debes oír CW limpio, sin chirp ni clicks.
3. Iambic: mantener DIT+DAH → alterna (Iambic A/B según `Keyer Mode`).

### Prueba 7 — AM / FM TX
1. Modo AM/FM vía menú `Mode`. Pulsa **DIT/DAH** (el keyer SINGLE funciona en
   cualquier modo, legacy 5269).
2. En AM la portadora sube con la voz; en FM cambia la desviación audible.
   *Nota:* VOX solo LSB/USB (legacy 5144); para AM/FM usar DIT como PTT.

### Prueba 8 — Frecuencia y armónicos
1. Compara la lectura del SDR con el display: deben coincidir (±100Hz, cristal).
2. Barre el SDR en 2× y 3× la frecuencia: el LPF debe atenuar los armónicos.

### Prueba 9 — CAT TX
1. Conecta USB-serial (38400). `FA;`/`IF;` responden. `TX;` (y también
   `TX0;`/`TX1;`/`TX2;`, como legacy) pasa a TX; `RX;` vuelve a RX (la señal
   debe oírse en el receptor).
2. En TX, `IF;` debe dar `1` en la posición 28 **también callado** (el micro
   en silencio apaga la portadora pero la radio sigue en TX), y `PC025;`
   baja la potencia en el aire.

---

## 6. Verificación de menú y persistencia

### Prueba 10 — Navegación (flujo 4 pulsaciones)
1. **BL corto** en principal → `MENU_SELECT` (1er parámetro `Vol`).
2. **Encoder** → recorre los 35 parámetros.
3. **BL corto** → `EDIT` (cursor `>` visible).
4. **Encoder** → cambia el valor.
5. **BL corto** → guarda (EEPROM) y vuelve a selección.
6. **BL corto** → sale a la pantalla principal (instantáneo).

### Prueba 11 — Persistencia
1. Cambia `Vol` a 4, `Mode` a CW, sintoniza 7.050MHz.
2. **Apaga y vuelve a encender.**
3. Debe recordar: volumen, modo, filtro, y la frecuencia de la banda
   (memoria de bandas vía `vfo`). Si vuelve a defaults → revisar EEPROM.

---

## 7. CAT

### Prueba 9 — Control por PC
CAT compatible **Kenwood TS-480** (flrig, Hamlib/rigctld, WSJT-X con el
modelo TS-480). Las escrituras no responden (como el TS-480); un rechazo o un
comando desconocido responde `?;`.

```bash
# en terminal serie 38400 8N1 (picocom -b 38400 /dev/ttyUSB0):
ID;             ->  ID020;
FA;             ->  FA00007074000;   (frecuencia actual, 11 dígitos)
FA00007050000;  ->  sintoniza a 7.050MHz
IF;             ->  IF00007050000...;  (38 caracteres; pos. 28 = 1 en TX)
MD;             ->  MD2;             (modo: 1 LSB, 2 USB, 3 CW, 4 FM, 5 AM)
AG0;            ->  AG0192;          (volumen 12 -> 192)
AG0080;         ->  baja el volumen a 5 (se oye)
SM0;            ->  SM00005;         (0..30, S9 = 0015; sube con señal)
GT;             ->  GT001;           (AGC Fast); GT002; -> Slow
FB;             ->  FB00014074000;   (VFO B, sin conmutar)
FR1;            ->  pasa a VFO B (pantalla con flecha B); FR0; vuelve a A
RU00500;        ->  RIT +500 Hz (pantalla "RIT +0500"); RC; lo borra
EX0300000;      ->  EX03000000;      (menú 30, TX Comp); EX03000001; lo pone a 1
EX0400000;      ->  ?;               (no existe)
TX;             ->  transmite (carga/dummy); IF; muestra '1' en la pos. 28
RX;             ->  RX0;  vuelve a RX
```
Durante TX se rechaza (`?;`) lo que toca RF (modo, banda, VFO, atenuador…);
`FA`/`MD`/RIT se aplican al volver a RX. Volumen, potencia y micro funcionan
también en TX. Lo cambiado por CAT se guarda en EEPROM como desde el menú:
**apaga y enciende** y comprueba con `AG0;`, `GT;`, `EX0300000;`.
Referencia completa de comandos y tabla `EX`: `usdxcontrol/docs`
(`cat-extensiones-peticion.md`, `cat-generico-interprete-especificacion.md`).
Si `ID;` no responde: verifica el baudrate (38400) y que el
USB-serial no esté ocupado por el bootloader ("serdas").
Con el CAT activo el S-meter de la pantalla se apaga (como legacy) para no
escribir en la LCD, que comparte pines con la UART.

---

## 8. Checklist resumido

| # | Prueba | Resultado esperado | OK? |
|---|---|---|---|
| 1 | Arranque LCD | `uSDX` + `v2.1.0` → `7,074,000 USB` + S-meter | ☐ |
| 2 | Ruido RX | S-meter >0, audio de ruido al sintonizar | ☐ |
| 3 | Filtros/AGC/NR | Audio cambia con el menú | ☐ |
| 3b | AGC Start / AGC Hang / NR | Sin bombeo, ruido baja en pausas, voz intacta | ☐ |
| 4 | SSB TX (VOX) | Audio limpio sin clicks, portadora rechazada | ☐ |
| 5b | TX Comp / LoCut / Mic Atten | Más potencia media, sin salpicadura | ☐ |
| 5c | TX EnvPre | IMD3 de dos tonos igual o mejor que con 0 | ☐ |
| 5 | CW keyer | Tonos limpios, Iambic responde | ☐ |
| 6 | AM/FM TX | Portadora/desviación presentes | ☐ |
| 7 | Menú | Navegación completa 35 params | ☐ |
| 8 | Persistencia | Vuelve con valores guardados tras power cycle | ☐ |
| 9 | CAT | `ID/FA/IF/MD/TX/RX` + `AG/SM/GT/FB/FR/RU/EX` responden y persisten | ☐ |

---

## 9. Limitaciones conocidas (heredadas de legacy)

1. **VOX solo en LSB/USB**: en AM/FM se usa DIT como PTT y en CW el keyer.
2. **RIT solo afecta a RX**: en TX se emite en la frecuencia del dial.
3. **Rampa anti-clic de CW dentro de la ISR**: `dsp_tx_cw` usa
   `delayMicroseconds` (31 pasos de 60 µs) al iniciar cada elemento, igual
   que legacy.

---

## 10. Resolución de problemas

| Síntoma | Causa probable | Acción |
|---|---|---|
| LCD negro / reset en bucle | Fuses a 1MHz (CKDIV8) | Escribir lfuse 0xFE |
| RX sin audio | ADC ref / prescaler | Verificar `adc_start` F_ADC_CONV en setup |
| TX sin RF / muy débil | Drive/PA Bias bajos, relés, PA | Revisar `TX Drive`, `PA Bias min/max` y alimentación del PA |
| TX distorsionado / clicks | Nivel de micro o drive excesivos | Bajar `Mic Atten`/`TX Drive`; probar `TX Comp` 0 |
| S-meter siempre 0 | Sin señal en el ADC | Revisar entrada RX, `ATT` y el menú `S-meter` (OFF) |
| No recuerda settings | EEPROM de otra versión | Reset de fábrica: encender con el encoder pulsado |
| CAT no responde | baudrate/port | 38400 8N1, cable USB-serial |

---

*Firmware verificado con los tests host y el simulador AVR (`tests/`). Esta
guía es el protocolo para validarlo en hardware.*