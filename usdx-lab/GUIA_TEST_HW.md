# Guía de test hardware — uSDX Lab

Verificación paso a paso del firmware usando `usdx-lab` (menú Engineer +
pantallas). La radio de producción NO se usa para probar.

## 0. Requisitos

- [ ] Firmware lab flasheado (`cd usdx-lab && arduino-cli compile --fqbn arduino:avr:uno .`)
- [ ] Alimentación estable 13.8 V (anotar tensión real)
- [ ] **TX solo contra carga ficticia de 50 Ω** (nunca antena en pruebas)
- [ ] Papel/esta guía para anotar la hoja de medición (§6)

## 1. Arranque

1. Encender. Debe mostrar `v2.0.0-lab` en la línea 2.
2. Primera vez: resetea EEPROM solo (magia propia, no toca producción).
3. Menú (botón L) → última entrada `Engineer` → OFF/ADC/AUDIO con el encoder.

- [ ] PASS: arranca, versión lab visible, menú Engineer existe

## 2. ADC (Engineer → ADC, sin antena, RX)

Pantalla: `ADC <mean> <min> <max>` / `RMS <rms> CL<clips> R<rango>`.

| Métrica | Esperado en vacío | Anotado |
|---|---|---|
| MEAN | ~512 (±12) | |
| RMS | piso de ruido, estable | |
| CLIP | 0 | |
| Rango (R) | pequeño (<100) | |

- [ ] PASS: MEAN centrado, CLIP 0, RMS estable
- Si MEAN desviado >±30 → revisar polarización 1.1 V / AREF
- Si CLIP >0 en vacío → ruido excesivo o ganancia; no seguir a TX

## 3. ADC con señal (antena o generador, RX)

1. Sintonizar emisora fuerte (AM broadcast 40 m o FM).
2. Observar MIN/MAX: no deben pegarse a 0/1023 de forma continua.

- [ ] PASS: excursión amplia sin CLIP sostenido
- Si CLIP continuo → subir ATT (menú 1.10) o ATT2 (1.11) y repetir

## 4. Audio (Engineer → AUDIO, RX con señal de voz/CW)

Pantalla: `AU R<rms> P<peak>` / `DC<±> CR<crest x10> CL<clips>`.

| Métrica | Esperado | Anotado |
|---|---|---|
| RMS voz | 20–150 según volumen | |
| PEAK | <511, sin CLIP | |
| DC | cercano a 0 (±10) | |
| CR (crest x10) | 10–40 (1.0–4.0) | |

- [ ] PASS: audio presente, DC≈0, sin clipping con volumen medio
- Probar filtros (R doble): el RMS debe bajar al estrechar a CW
- Probar AGC ON/OFF (1.8): con AGC ON el RMS se estabiliza entre señales

## 5. TX a carga ficticia (SOLO 50 Ω, empezar drive bajo)

1. Engineer → OFF (pantalla normal).
2. Conectar carga 50 Ω. Modo USB, drive 2.
3. PTT + silbido suave: observar consumo y, si hay wattmeter, potencia.
4. Subir a drive 4 (valor producción) y repetir.

- [ ] PASS: hay salida RF, sin calentamiento anormal, vuelve a RX al soltar
- [ ] Anotar: consumo RX ____ mA / consumo TX ____ mA / potencia ____ W
- Ante cualquier anomalía (sin salida, consumo disparado, no vuelve a RX):
  parar y revisar hardware antes de seguir

## 6. Separación lab/producción

1. Flashear producción, comprobar que arranca con SUS ajustes (v2.0.0).
2. Volver a flashear lab, comprobar `v2.0.0-lab` y sus ajustes propios.

- [ ] PASS: cada firmware conserva sus ajustes (magias EEPROM distintas)

## 7. Hoja de medición

```text
Fecha: ________  Firmware lab commit: ________
Alimentación: ________ V   Carga: 50 Ω
ADC vacío:  MEAN ____ MIN ____ MAX ____ RMS ____ CLIP ____
ADC señal:  MIN ____ MAX ____ CLIP ____ (ATT/ATT2: ____)
Audio voz:  RMS ____ PEAK ____ DC ____ CR ____ CL ____
TX drive2:  I ____ mA   TX drive4: I ____ mA  P ____ W
Notas: ________________________________________
```

## 8. Fallos comunes

| Síntoma | Mirar |
|---|---|
| LCD en blanco | alimentación, contraste, flasheo |
| MEAN ADC lejos de 512 | AREF 1.1 V, soldaduras QSD/ADC |
| CLIP siempre >0 | ATT/ATT2, ganancia, oscilación |
| Sin audio | volumen, filtro, AGC, altavoz |
| No vuelve de TX | PTT/VOX, relés, timeout semi-QSK |
| Lab pisa ajustes de prod | F_VER_ID distintos (revisar) |
