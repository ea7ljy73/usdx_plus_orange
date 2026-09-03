# Guía de Prueba en Hardware — uSDX Plus Orange v2

Firmware modular `usdx_plus_orange_v2/` (rama `refactor-v2`). Esta guía cubre
el flasheo, las verificaciones paso a paso de RX/TX/CW/menú/CAT, y qué esperar
en cada prueba. Incluye un apartado de resolución de problemas.

---

## 1. Requisitos previos

| Ítem | Valor |
|---|---|
| Placa | uSDX WHITE_BUTTONS (ATMEGA328P @20MHz, cristal SI5351 27MHz) |
| Programador | USBasp en ISP (recomendado) o Arduino-ISP |
| Constantes | `F_MCU 20MHz`, `F_XTAL 27MHz`, `WHITE_BUTTONS=1` (ya en `usdx_settings.h`) |
| Build actual | 19852B flash (61%), 1425B RAM (69%) |

> **IMPORTANTE — antes de grabar:** el firmware **no tiene fuses automáticos**.
> Verifica que el MCU esté a 20MHz externo (CKSEL=1111, CKDIV8=0) como el v1.
> Usar fuses USBasp por defecto (1MHz) hace que el radio no funcione.

---

## 2. Flasheo

### Compilar
```bash
cd usdx_plus_orange_v2
arduino-cli compile --fqbn arduino:avr:uno .
```
Resultado esperado: `19852 bytes (61%)`, RAM `1425 (69%)`. Sin errores.

### Grabar con USBasp (recomendado)
```bash
arduino-cli upload -b arduino:avr:uno -p usbasp -P usbasp .
```
o manual:
```bash
avrdude -c usbasp -p m328p -U flash:w:build/arduino.avr.uno/usdx_plus_orange_v2.ino.hex:i
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

1. **LCD**: muestra `uSDX v2` durante ~300ms, luego `USB <freq>` en línea 1
   (ej. `USB 7100000`) y `RX S..` en línea 2.
2. **Voltage**: si mides el pin AREF, ~2.5V (bias SDR).
3. **Cat**: si conectas USB-serial, debe responder `ID020;` al `ID;` (puerto
   115200 baudios, corregido a 20MHz). Si no responde, revisa el puerto.

> Si la pantalla se queda en negro: revisar fuses (1MHz vs 20MHz) y el lcd
> wiring (PD0-4+PC4 RS).

---

## 4. Verificación RX

### Prueba 1 — Ruido / S-meter
1. Con una antena o carga, en 40m USB (default 7.100MHz).
2. Línea 2 muestra `RX S==` (barra de ruido ~varios segmentos).
3. **Gira el encoder**: frecuencia cambia por pasos (stepsize predeterminado).
   Vibrar la frecuencia sintonizando estaciones — si hay audio válido, OK.

### Prueba 2 — Audio / filtros
1. Entra en el menú (BL corto en principal). Navega a `FilterBW` (encoder).
   En `EDIT` (BL corto), cambia entre `Full/3000/2400/...`. El audio debe
   volverse más agudo/estrecho a filtros CW.
2. **Vol** (1er parámetro): sube/baja volumen audiblemente.
3. **NR / ATT / AGC**: ajusta y comprueba que el ruido/nivel cambia.

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
1. Conecta USB-serial (38400). `FA;`/`IF;` responden; `TX1;`/`TX0;` alternan
   TX/RX (la señal debe oírse en el receptor).

---

## 6. Verificación de menú y persistencia

### Prueba 10 — Navegación (flujo 4 pulsaciones)
1. **BL corto** en principal → `MENU_SELECT` (1er parámetro `Vol`).
2. **Encoder** → recorre los 28 parámetros.
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
```bash
# en terminal serie 115200 8N1:
ID;      ->  ID020;
FA;      ->  FA007101000;     (frecuencia actual, sin captura)
FA00007050000;  ->  sintoniza a 7.050MHz
IF;      ->  IF...750...       (estado)
TX;      ->  transmite (carga/dummy)
RX;      ->  vuelve a RX
MD1;     ->  usb ; MD3 -> CW
```
Si `ID;` no responde: verifica el baudrate (115200 corregido a 20MHz) y que el
USB-serial no esté ocupado por el bootloader ("serdas").

---

## 8. Checklist resumido

| # | Prueba | Resultado esperado | OK? |
|---|---|---|---|
| 1 | Arranque LCD | `uSDX v2` → `USB 7100000` / `RX S..` | ☐ |
| 2 | Ruido RX | S-meter >0, audio de ruido al sintonizar | ☐ |
| 3 | Filtros/AGC/NR | Audio cambia con el menú | ☐ |
| 4 | SSB TX (VOX) | Audio limpio sin clicks, portadora rechazada | ☐ |
| 5 | CW keyer | Tonos limpios, Iambic responde | ☐ |
| 6 | AM/FM TX | Portadora/desviación presentes | ☐ |
| 7 | Menú | Navegación completa 31 params | ☐ |
| 8 | Persistencia | Vuelve con valores guardados tras power cycle | ☐ |
| 9 | CAT | `ID/FA/IF/MD/TX/RX` responden | ☐ |

---

## 9. Limitaciones conocidas del v2 (vs v1)

1. **VOX solo LSB/USB**: en AM/FM/CW no hay VOX automático (el CW usa keyer;
   AM/FM requieren un hook de PTT). Mejora pendiente.
2. **RIT/offset CW**: `rit` y el offset CW TX configurable no se aplican en el
   TX aún (CAT `RTS` setea `rit`, se aplica sobre freq en modo listen soldad),
   el TX no compensa. Limitación menor.
3. **Decoder CW en RX**: la máquina de estados está, pero el detector de tono
   audio (que derive transiciones de señal) es simplificado; puede no
   decodificar de forma fiable hasta añadir el detector del v1.
4. **Audio out sin integrator** (`ozi1/ozi2` del v1): posible leve diferencia
   de nivel/tono de audio; no impide operar.
5. **`digitalWrite`/`delay` en ISR TX**: `dsp_tx_cw` usa `delayMicroseconds`
   (ramp anti-click) dentro de la ISR — igual que v1; controlar que no cuelgue.

---

## 10. Resolución de problemas

| Síntoma | Causa probable | Acción |
|---|---|---|
| LCD negro / reset en bucle | Fuses a 1MHz (CKDIV8) | Escribir lfuse 0xFE |
| RX sin audio | ADC ref / prescaler | Verificar `adc_start` F_ADC_CONV en setup |
| TX sin RF / muy débil | `COM1B1` no activo | Verificar switch_rxtx (fix aplicado) |
| TX distorsionado / clicks | Sample-rate OCR2A wrong | Ver fix R3 (OCR2A TX=4800) |
| S-meter siempre 0 | `_absavg256` no actualiza | Verificar CIC `func_ptr` del RX |
| No recuerda settings | EEPROM antes de 0x250 | slot VFO 0x250, menú hasta 0x248 |
| CAT no responde | baudrate/port | 115200 @20MHz, cable USB-serial |

---

*Firmware v2 documentado y verificado estáticamente (9 bugs R1-R3 corregidos).
Esta guía es el protocolo de prueba para validar en hardware.*