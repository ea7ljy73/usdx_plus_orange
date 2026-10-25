# CAT — uSDX Plus Orange v2.2.0 (compatible Kenwood TS-480)

Referencia completa del control por puerto serie del firmware de producción
(`cat.h`). La radio se presenta como un **Kenwood TS-480** (`ID020;`): sirven
flrig, Hamlib/rigctld (modelo TS-480), WSJT-X y cualquier programa que hable
TS-480. Lo que el TS-480 no tiene (los parámetros propios del uSDX) va por
`EX`, el menú extendido, con todos los parámetros del menú por su número.

- **Estado:** implementado en v2.2.0 y probado en simulación (ver
  [Pruebas](#pruebas)). **Pendiente de validar en hardware**
  (`HARDWARE_TEST_GUIDE.md` §7).
- **Conexión:** UART del ATmega328P (PD0/PD1, compartidos con la LCD) con un
  adaptador USB-TTL de 5 V; cableado en `GUIA_ESTACION_PC.md` §3.
- El laboratorio (`usdx-lab/`) **no** lleva CAT.

## Reglas generales

- Serie **38400 8N1**, tramas ASCII terminadas en `;`. Un comando cada ≥ 120 ms (el firmware
  espera 10 ms tras cada trama, como legacy).
- **Lectura**: el comando sin parámetro (`AG0;`, `GT;`…) devuelve el valor.
- **Escritura**: el comando con parámetro **no devuelve nada** (igual que el TS-480). El cliente
  confirma releyendo.
- **Rechazo**: si el valor no es numérico o está fuera de rango, o si el comando no existe, la
  radio responde `?;` y no hace nada. Si la trama pasa de 40 bytes, responde `E;`.
- **En TX**: lo que toca el sintetizador, los relés o el ADC (modo, banda, VFO, RIT on/off,
  atenuador, calibración…) se **rechaza con `?;`**. `FA`/`MD`/`RU`/`RD`/`RC` se anotan y se aplican
  al volver a RX. Volumen, potencia, micro, AGC, NR, VOX y keyer funcionan también en TX.
- **Persistencia**: lo que se cambia por CAT se guarda en EEPROM igual que desde el menú, y solo
  si el valor cambia (una escritura con el mismo valor no hace nada).
- **Estado de TX**: lo da el campo P8 de `IF;` (TX real de la ISR, incluidas las pausas de la voz).
  No hay tramas espontáneas (`AI0`).

## Comandos

### Frecuencia, modo, VFO

| Comando | Lectura → respuesta | Escritura | Notas |
|---|---|---|---|
| `FA` | `FA;` → `FA00007074000;` | `FA00014074000;` | VFO activo, 11 dígitos en Hz |
| `FB` | `FB;` → `FB…;` | `FB00014200000;` | VFO **no activo**: no conmuta ni toca RF |
| `FR` | `FR;` → `FR0;` (A) / `FR1;` (B) | `FR0;` / `FR1;` | Conmuta A/B (en TX → `?;`) |
| `FT` | `FT;` → igual que `FR` | solo el mismo valor que `FR` | Sin split: otro valor → `?;` |
| `MD` | `MD;` → `MD1`..`MD5` | `MD1;`..`MD5;` | 1 LSB, 2 USB, 3 CW, 4 FM, 5 AM |
| `IF` | `IF;` → trama de 38 caracteres | — | Ver abajo |

Trama `IF` (posiciones desde 0):

| Posición | Campo | Valor en el uSDX |
|---|---|---|
| 2–12 | P1 frecuencia | 11 dígitos |
| 13–17 | P2 paso | `00000` |
| 18–22 | P3 offset del RIT | `+0250`, `-0100`… (±9999 Hz) |
| 23 | P4 RIT on | `0` / `1` |
| 24 | P5 XIT | `0` |
| 25–27 | P6–P7 memoria | `000` |
| **28** | **P8 TX/RX** | **`1` en TX** (CAT, PTT, VOX, CW) |
| 29 | P9 modo | como `MD` |
| 30 | P10 VFO | `0` A, `1` B |
| 31–36 | P11–P15 | `000000` |

### RIT

| Comando | Efecto |
|---|---|
| `RT;` | → `RT0;` / `RT1;` |
| `RT0;` / `RT1;` | Apaga / enciende el RIT. Al encender sin offset queda en +1 Hz, como en el menú; si ya hay offset, se conserva |
| `RU;` / `RD;` | Sube / baja 10 Hz |
| `RU00250;` / `RD00250;` | Sube / baja n Hz (límite ±9999, como el dial) |
| `RC;` | Borra el RIT (responde `RC;`, por compatibilidad con legacy) |

El offset se lee en `IF` (P3/P4). Un offset que llega a 0 apaga el RIT, porque en el uSDX el RIT
activo es `rit != 0`.

### Recepción y transmisión

| Comando | Rango TS-480 | Parámetro del uSDX | Conversión |
|---|---|---|---|
| `AG0nnn` | 000–255 | **Volume** (menú 0, 0..16) | lectura `v·16` (16 → 255); escritura `(n+8)/16`. Nunca pone −1 (apagado) |
| `SM0` | 0000–0030 | S-meter (dBm) | `(dBm+133)/4`: S9 (−73 dBm) = 15, 4 dB por cuenta. Solo lectura |
| `GTnnn` | 000/001/002 | AGC (menú 7) | 000 OFF, 001 Fast, 002 Slow |
| `RAnn` | 00–07 | ATT (menú 9) | lectura `RAnn00;`. 0 dB, −13, −20, −33, −40, −53, −60, −73 dB |
| `NRn` | 0/1/2 | NR (menú 8) | 0 = off, 1 = NR 1, 2 = puerta de ruido (NR ≥ 2; `NR2` conserva el nivel) |
| `RLnn` | 00–08 | NR (menú 8) | nivel directo |
| `PCnnn` | 000–100 | TX Drive (menú 20, 0..8) | lectura `drive·12,5`; escritura `(2n+12)/25` |
| `MGnnn` | 000–100 | Mic Atten (menú 28, 4..0) | 100 = sin atenuación, 0 = −24 dB; pasos de 25 |
| `PRn` | 0/1 | TX Comp (menú 30) | `PR1` conserva el nivel; el nivel 0..3 va por `EX030` |
| `KSnnn` | 001–060 | Keyer Speed (menú 14) | wpm |
| `VXn` | 0/1 | VOX (menú 18) | **ojo**: `VX1` deja la radio transmitiendo con el audio del micro |
| `TX;` `TX0;` `TX1;` `TX2;` | — | PTT | sin respuesta |
| `RX;` | — | vuelta a RX | responde `RX0;` |

### Menú extendido (todo lo demás)

| Comando | Respuesta |
|---|---|
| `EXnnn0000;` | `EXnnn0000v;` (`nnn` = número de menú de la LCD, `v` en decimal; puede llevar `-`) |
| `EXnnn0000v;` | sin respuesta; `?;` si se rechaza |

Tabla de números, rangos y efectos: [§ Menú extendido `EX`](#menú-extendido-ex-todos-los-parámetros).

### Compatibilidad (legacy)

`ID;` → `ID020;` · `PS;` → `PS1;` · `AI;`/`AI0;` → `AI0;` · `XT1;` → `XT1;` · `FL0;` → `FL0;` · `RS;` → `RS0;`.

## Menú extendido `EX` (todos los parámetros)

`EX` da acceso a **todos los parámetros numéricos del menú** con un único
mecanismo: el firmware recorre su tabla `MENU[]`, valida el rango, escribe,
guarda en EEPROM y ejecuta el mismo efecto que al editar con el dial.

### Formato

| Trama | Respuesta |
|---|---|
| `EXnnn0000;` | `EXnnn0000v;` |
| `EXnnn0000v;` | sin respuesta (escritura correcta) o `?;` |

- `nnn`: **número de menú que se ve en la LCD**, con 3 dígitos (`000`–`034`). No es la posición
  en el array: las entradas 28–31 y 34 están insertadas detrás de la 20.
- `0000`: campos P2–P4 del TS-480, fijos.
- `v`: decimal sin ceros a la izquierda; con `-` en valores negativos (solo el volumen −1).
- `?;` si: número inexistente, `CQ Message` (texto), valor no numérico o fuera de rango, o
  parámetro con efecto sobre la RF mientras se transmite (columna «TX»).
- Escribir el valor que ya tiene no hace nada (ni EEPROM ni efecto).

### Tabla (de `usdx_plus_orange.ino`, `MENU[]`)

| `nnn` | Menú | Rango | Valores | Efecto al escribir | TX |
|---|---|---|---|---|---|
| 000 | Volume | −1..16 | −1 = mudo | — | sí |
| 001 | Mode | 0..4 | LSB, USB, CW, FM, AM | como el menú: fija paso y filtro (CW: filtro 500, NR 0) y reprograma | no |
| 002 | Filter BW | 0..7 | Full, 3000, 2400, 1800, 500, 200, 100, 50 Hz | — | sí |
| 003 | Band | 0..10 | 160, 80, 60, 40, 30, 20, 17, 15, 12, 10, 6 m | memoria de banda, LPF y SI5351 | no |
| 004 | Tune Rate | 0..9 | 10M, 1M, 0.5M, 100k, 10k, 1k, 0.5k, 100, 10, 1 | — | sí |
| 005 | VFO Mode | 0..1 | A, B | conmuta A/B (igual que `FR`) | no |
| 006 | RIT | 0..1 | off/on (on = +1 Hz) | como `RT` | no |
| 007 | AGC | 0..2 | OFF, Fast, Slow | — | sí |
| 008 | NR | 0..8 | 0 off, 1, ≥ 2 puerta de ruido | — | sí |
| 009 | ATT | 0..7 | 0, −13, −20, −33, −40, −53, −60, −73 dB | atenuador analógico | no |
| 010 | ATT2 | 0..16 | atenuación digital (CIC) | — | sí |
| 011 | S-meter | 0..4 | OFF, dBm, S, S-bar, wpm | (con CAT el firmware lo pone a 0 para no escribir en la LCD) | sí |
| 012 | CW Decoder | 0..1 | off/on | — | sí |
| 013 | Semi QSK | 0..1 | off/on | — | sí |
| 014 | Keyer Speed | 1..60 | wpm | recalcula tiempos | no |
| 015 | Keyer Mode | 0..2 | Iambic A, Iambic B, Straight | — | no |
| 016 | Keyer Swap | 0..1 | off/on | — | sí |
| 017 | Practice | 0..1 | off/on | — | sí |
| 018 | VOX | 0..1 | off/on | — | sí |
| 019 | Noise Gate | 0..255 | umbral del VOX | — | sí |
| 020 | TX Drive | 0..8 | 8 = amplitud constante | — | sí |
| 021 | CQ Interval | 0..60 | s | — | sí |
| 022 | CQ Message | — | texto | **no disponible por CAT** | — |
| 023 | PA Bias min | 0..254 | | rehace la tabla del PA | no |
| 024 | PA Bias max | 1..255 | | rehace la tabla del PA | no |
| 025 | Ref freq | 14000000..28000000 | Hz del cristal (calibración) | reprograma el SI5351 | no |
| 026 | IQ Phase | 0..180 | grados | reprograma el SI5351 | no |
| 027 | Backlight | 0..1 | off/on | — | sí |
| 028 | Mic Atten | 0..4 | 6 dB por paso | — | sí |
| 029 | DIGI Mode | 0..1 | off/on (TX plano para FT8) | — | sí |
| 030 | TX Comp | 0..3 | recorte de envolvente | — | sí |
| 031 | TX LoCut | 0..3 | off, ≈100, 200, 350 Hz | — | sí |
| 032 | AGC Start | 0..1 | off/on (precarga x8 al arrancar) | — | sí |
| 033 | AGC Hang | 1..8 | retención (n−1)·0,1 s; 1 = legacy | — | sí |
| 034 | TX EnvPre | 0..3 | preénfasis de envolvente | — | sí |

«TX = no» significa que el parámetro tiene efecto sobre RF, LCD o tiempos y se rechaza con `?;`
mientras la radio transmite. Los demás se aceptan en TX.

### Notas

- **VFO (005)**: equivale a `FR0;`/`FR1;`. Conmutar no altera la frecuencia guardada del otro VFO.
- **Backlight (027)**: el valor de fábrica es 8 (legacy), fuera del rango 0..1; se lee `8` hasta
  que se escribe 0 o 1.
- **PA Bias**: el menú limita `min < max` mientras se edita; por CAT, si `max < min`, el efecto
  sube `max` a `min` (no se graba en EEPROM hasta la siguiente edición).
- **Lectura inicial**: 35 lecturas a 1 cada 120 ms ≈ 4,2 s. No hace falta sondearlas en bucle:
  solo cambian desde el menú de la radio o por CAT.

## Detección de firmware ampliado

Hasta la v2.1.0 el firmware responde a `AG0;` con `AG0;` (eco) y a `EX…;` con `?;`.
Al conectar:

1. `ID;` → `ID020;`.
2. `EX0000000;` → si responde `EX0000000<v>;`, el firmware es ampliado: se activan todos los
   controles. Si responde `?;`, solo `FA/MD/IF/TX/RX/RC`.

## Cambios respecto a v2.1.0 y legacy

| Comando | v2.1.0 / legacy | v2.2.0 |
|---|---|---|
| `AG0;` | eco `AG0;` sin efecto | lee el volumen `AG0nnn;`; `AG0nnn;` lo cambia |
| `RT1;` | eco `RT1;` sin efecto | enciende el RIT; `RT0;` lo apaga; `RT;` lo lee |
| `VXn;` | eco `VXn;` sin efecto | activa/desactiva el VOX (sin respuesta); `VX;` lo lee |
| `RC;` | `rit = 0` pero el SI5351 seguía desplazado | borra y reprograma |
| `IF;` | RIT, TX/RX y VFO siempre a 0 | P3/P4 RIT, P8 TX/RX, P10 VFO |
| `SM`, `FB`, `FR`, `FT`, `GT`, `RA`, `NR`, `RL`, `PC`, `MG`, `PR`, `KS`, `RU`, `RD`, `EX` | `?;` | implementados |
| Comandos conocidos con parámetro inválido (`TX5;`, `IFx;`…) | `?;` | sin respuesta |

`FA`, `MD`, `TX`, `RX`, `ID`, `PS`, `AI`, `XT1`, `FL0` y `RS` responden igual
que en legacy.

## Implementación (`cat.h`)

- **Recepción:** `serialEvent()` → `cat_serial_event()` lee un byte por vuelta
  del bucle en `CATcmd[40]`; con `;` analiza la trama y espera 10 ms (legacy).
  Cada byte bloquea la LCD 10 ms (`rxend_event`), como legacy.
- **Despacho:** `switch` sobre las dos letras. Los comandos que son un
  parámetro del menú con escala TS-480 están en la tabla `CAT_MAP` (PROGMEM:
  letras, nº de menú, longitud de lectura, dígitos y tipo de escala), así que
  añadir uno cuesta 6 bytes de tabla. `EX` usa directamente `MENU[]`.
- **Escritura genérica** (`cat_set`): rango de `MENU[]` → si no cambia, nada →
  si tiene `on_change` y la radio está en TX, `?;` → escribe, guarda en EEPROM
  y ejecuta `on_change` (y `cat_change` para LPF, RIT y pantalla).
- **Cambios de RF diferidos:** `FA`, `MD`, `RU`/`RD`/`RC` solo anotan; el
  SI5351, los relés LPF y la pantalla se tocan en `cat_apply()` desde `loop()`
  y nunca en TX (legacy 5676: acceso I2C simultáneo con la ISR de TX).
- **Estado de TX (`cat_busy`)**: `tx || vox_tx || OCR2A > 16`. `tx` solo no
  basta: con `CARRIER_COMPLETELY_OFF_ON_LOW` es el latch de portadora y
  `ssb()` lo pone a 0 en cada pausa de la voz. `OCR2A` es el divisor del
  muestreo (4 en RX a 62,5 kHz, 64 en TX a 4,8 kHz) y solo cambia en
  `switch_rxtx`; un `static_assert` del `.ino` vigila esos valores.
- **Respuestas:** se montan sobre el propio `CATcmd` (ya trae las dos letras
  del comando, y en `EX` la cabecera entera) con `cat_num()` y salen con un
  único `Serial.print`. Textos fijos con `F()`.
- **S-meter:** `smeter()` calcula los dBm siempre (para `SM0`) y solo pinta
  si `smode` ≠ 0. Al primer comando CAT el firmware pone `smode = 0` (legacy),
  y entonces `display_tick()` **no escribe en la LCD**: cada escritura apaga la
  UART (pines compartidos) y antes se perdía ~1 % de los bytes.
- **Parámetros numéricos:** `cat_arg()` exige un dígito o `-` antes de grabar
  (`RTC;`, `GTx;` → `?;`, nunca un 0 por basura).

### Coste

v2.1.0 → v2.2.0: **+1176 B de flash, −91 B de RAM** (textos a flash y
respuestas sin buffer propio). Build: 31964 B (99 %) / 1097 B (53 %), unos
290 B libres.

### No implementado (por flash, por orden de interés)

| Comando | Coste aprox. | Alternativa |
|---|---|---|
| `KY` (texto CW) | 150 B | — |
| `UP` / `DN` (un paso) | 110 B | `FA` |
| `FW` (filtro en Hz) | 100 B | `EX0020000<0..7>;` |
| `BU` / `BD` (banda) | 90 B | `EX0030000<0..10>;` |
| `AI1`/`AI2` (avisos espontáneos), split, memorias, `SH`/`SL` | — | — |
| Audio por la serie | — | descartado (CPU y flash, ver ROADMAP) |

## Pruebas

```bash
cd tests/fw_sim && ./run_fw_sim.sh            # cat_sim: cat.h real + MENU[] real del .ino
./tests/avr_sim/run_avr_sim.sh                # incluye los escenarios CAT por la UART real
./tests/run_all.sh --build --avr              # todo
```

- `tests/fw_sim/cat_sim` (125 casos): respuestas de legacy, formato de `IF`,
  escalas y rangos de cada comando, `?;` sin efecto, persistencia (eslot),
  rechazo en TX incluso con `tx` = 0 (pausa de la voz), `EX` sobre la tabla
  `MENU[]` real (`gen_cat_menu.py`).
- `tests/fw_sim/ui_sim`: U5 (con `smode` 0 no se escribe en la LCD) y U6 (VFO
  A/B desde el menú sin pisar el otro VFO).
- `tests/avr_sim --scenario cat`: el `.elf` real en un ATmega328P simulado; los
  bytes entran por la UART a 38400 baudios al ritmo de la línea
  (`--cat "CMD;=respuesta@variable=valor|..."`, `--cat-step s`, `#` = cualquier
  carácter, `*` = el resto). Comprueba respuestas, RAM, bytes perdidos y que la
  ISR de RX no pierde muestras. Casos: guion de 65 pasos, sondeo tipo Hamlib a
  120 y 50 ms, y persistencia con `--ee-save`/`--ee-load`.
  `CAT_WATCH=variable` traza quién la cambia (depuración).

## Prueba en la radio

`HARDWARE_TEST_GUIDE.md` §7 (con terminal serie a 38400 8N1) y, para flrig,
`GUIA_ESTACION_PC.md` §5.
