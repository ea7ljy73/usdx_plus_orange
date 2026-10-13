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
- **Menú Engineer** (eslot 35): 0=OFF, 1=ADC, 2=AUDIO (`eng_lab.h`).
  - ADC: `ADC <mean> <min> <max>` / `RMS <rms> CL<clips> R<rango>` (ADC crudo).
  - AUDIO: `AU R<rms> P<peak>` / `DC<±> CR<crest x10> CL<clips>` (audio final).
  - Solo en RX y fuera del menú, refresco 2 Hz.

## Build

```bash
cd usdx-lab
arduino-cli compile --fqbn arduino:avr:uno .
```

## Regla de sincronía

Al actualizar producción, resincronizar esta copia: copiar de la raíz los
`.h` que hayan cambiado (salvo `usdx_settings.h`: mantener versión/magia lab)
y adaptar `usdx-lab.ino` si el original cambió. El lab **nunca** se flashea
como radio de uso: solo banco con carga de 50 Ω.

Guía de instrumentación: `uSDX_Plus_Orange_V2_Instrumentacion_DSP.md`.
Guía de pruebas hardware paso a paso: `GUIA_TEST_HW.md`.
