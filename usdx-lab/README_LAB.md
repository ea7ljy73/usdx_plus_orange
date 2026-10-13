# uSDX Lab — firmware de laboratorio (NO producción)

Sketch Arduino autocontenido en `usdx-lab/`. Es una **copia** del firmware de
producción (raíz del proyecto) con la parte de laboratorio añadida.

**Propósito: aquí se hacen TODAS las pruebas hardware** para verificar que
todo funciona bien, con información en pantalla (menú Engineer) y serie
libre para futuras mediciones. Producción no se toca para probar.

## Diferencias con producción

- **Sin CAT** (`cat.h` eliminado): la serie queda libre para laboratorio.
  A cambio se define `cat_active = 0` fijo (lo exigen `display.h`/`hw.h`).
- **Versión y EEPROM propias**: `FIRMWARE_VERSION "v2.0.0-lab"` (visible en
  el arranque) y `F_VER_ID` distinto → no comparte ajustes con producción.
- **Menú Engineer** (dos filas ON/OFF independientes, eslots 35/36):
  "Eng ADC" y "Eng Audio". Si ambas están on, manda AUDIO.
  - ADC: `ADC <mean> <min> <max>` / `RMS <rms> CL<clips> R<rango>` (ADC crudo).
  - AUDIO: `AU R<rms> P<peak>` / `DC<±> CR<crest x10> CL<clips>` (audio pre-NR, tras AGC/volumen: estable con NR on).
  - Solo en RX y fuera del menú, refresco 2 Hz.

## Build

```bash
cd usdx-lab
arduino-cli compile --fqbn arduino:avr:uno .
```

## Flasheo por ISP (Arduino Uno como programador, verificado 2026-09-17)

El uSDX **no** se flashea por USB-serie: se usa un Arduino Uno como
programador ISP en `/dev/ttyACM0`, cableado por ICSP al uSDX.

1. **Preparar el programador** (una sola vez; se pierde si se sube otro
   sketch al Uno). Subida **normal** por USB del ejemplo ArduinoISP:
   ```bash
   arduino-cli compile --fqbn arduino:avr:uno \
     /snap/arduino/85/examples/11.ArduinoISP/ArduinoISP
   arduino-cli upload -p /dev/ttyACM0 --fqbn arduino:avr:uno \
     /snap/arduino/85/examples/11.ArduinoISP/ArduinoISP
   ```
   (En el IDE: Archivo → Ejemplos → 11.ArduinoISP → ArduinoISP → Subir.)
2. **Flashear el lab** (desde la raíz del proyecto):
   ```bash
   arduino-cli upload -p /dev/ttyACM0 --programmer arduinoasisp \
     --fqbn arduino:avr:uno ./usdx-lab
   ```
   Para producción, sustituir `./usdx-lab` por `.`.
3. Si avrdude da `stk500_getsync() ... not in sync`: el Uno no está
   respondiendo como programador → repetir paso 1, revisar cableado ICSP
   y, si persiste, condensador 10 µF entre RST y GND del Uno.

## Menú (gestos, a sangre)

Solo el botón de menú entra al menú. Dentro, menú y dial hacen lo mismo:
en navegación el dial cambia de opción; una pulsación entra a edición;
girando se modifica el valor; pulsando se deja de editar, graba y vuelve
a la opción (browser). **Solo menú saca a frecuencia**: en edición guarda
y vuelve a la opción; otra pulsación de menú sale a principal. El dial
nunca sale. El dial en navegación envuelve (de Volume bajas 1 y caes en
la última fila) y **el lab recuerda la última fila** (difiere de legacy
a propósito).

- OJO: pulsación larga del botón derecho activa **RIT** (legacy): el dial
  pasa a mover RIT y la línea muestra `RIT ...`. Se quita con click corto
  derecho en MAIN o poniendo RIT OFF en su fila.
- Para pruebas RX: **VOX OFF** (fila 18). Un TX accidental con antena y sin
  carga puede dañar el PA.
- Divergencias LAB vs legacy (el menú de producción también lleva la puerta
  DC y el módulo real): el menú recuerda la última fila; el dial envuelve
  con módulo real también en producción.

## Regla de sincronía

Al actualizar producción, resincronizar esta copia: copiar de la raíz los
`.h` que hayan cambiado (salvo `usdx_settings.h`: mantener versión/magia lab)
y adaptar `usdx-lab.ino` si el original cambió. El lab **nunca** se flashea
como radio de uso: solo banco con carga de 50 Ω.

Guía de instrumentación: `uSDX_Plus_Orange_V2_Instrumentacion_DSP.md`.
Guía de pruebas hardware paso a paso: `GUIA_TEST_HW.md`.
