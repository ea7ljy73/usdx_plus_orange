# uSDX Plus Orange v2 — Roadmap de mejora (post-paridad)

Base: paridad funcional legacy verificada (tests host a 0 mismatches).
Build de partida: 29750 B (92 %) flash / 1157 B (56 %) RAM, 0 warnings.
Margen: ~2,4 KB flash, ~890 B RAM.

## Techos hardware (límites físicos, no negociables)

- ADC 10 bit + oversampling 4x → ~72 dB DR en 2,4 kHz. Micro sin amplificar
  ~1 mV/LSB: la ganancia de micro es crítica.
- Envolvente PA por PWM 8 bit → 48 dB de rango AM; fase desde multisynth
  SI5351 con cuantización → espurios laterales inevitables.
  IMD3 ~−33 dBc es el techo práctico de la arquitectura (clase-E + EER).
- CPU @20 MHz: ISR RX a 62,5 kHz = 320 ciclos/muestra; ISR TX a 4,8 kHz =
  4166 ciclos, de los cuales el I2C bulk (~88 µs ≈ 1760 ciclos) se come ~40 %.
- Único `float` del firmware: `smeter()` (`log10`). Resto entero (SI5351 usa
  división 64 bit: cara pero puntual).

## Reglas de juego

- Cada mejora: conmutada por menú/flag (apagable), test A/B host + medida en
  hardware, presupuesto flash/RAM declarado. Sin mejora medible → rollback.
- Orden: instrumentar → liberar margen → fluidez → TX → RX → robustez/UX.
- Descartado explícito: espectro en pantalla (pide OLED+CPU), CAT streaming
  audio (CPU+flash), QUAD (daña TX), más bandas.

## Fase 0 — Instrumentación (esta fase)

- [x] Medidor carga CPU: pin sonda PD5 (toggle en ISRs) + loops/s por Serial.
- [x] Script de tamaño: totales + top símbolos por build, CSV por commit.
- [x] Harness A/B host: IMD dos tonos por `ssb()`, SNR/rechazo por RX.

## Fase 1 — Liberar margen (~+1,5 KB flash, +headroom CPU)

1. S-meter `float`→punto fijo con LUT entera de log10 (~−1 KB flash;
   paridad ±1 dB).
2. `freq_calc_fast` versión AG6NS (−81 % ciclos; tune y TX más rápidos).
3. I2C bulk diferencial (solo registros cambiados; −30–50 % tiempo I2C en TX).

## Fase 2 — Fluidez

4. No reprogramar SI5351 si `df==0`.
5. Cambio de banda direccional según último sentido del dial (GW8RDI 4.00d).

## Fase 3 — TX (por dB/esfuerzo)

6. Mic gain/atten por menú (la más rentable según la comunidad).
7. Perfil DIGI/FT8 (HPF <100 Hz plano + preset VOX/USB/BW).
8. Hard-clipper/ALC + indicador overload (evita sobremodulación, +2–3 dB).
9. CESSB, compresor, EQ, low-cut, pre-énfasis una a una, conmutadas y medidas.
10. Hilbert IIR TX mejorado solo con margen CPU; AM `sqrt` y FM `arctan`
    como opciones a evaluar.
11. CW: sidetone por menú; verificar rampas en hardware.

## Fase 4 — RX

12. AGC hang + noise floor, detector tras filtros (opción), decay rápido CW.
13. NR_FIR opcional por niveles (3–8 FIR, 0–2 EA actual).
14. LMS notch solo en path CW (en voz interfería).
15. S-meter calibrado real tras punto fijo; Goertzel CW como experimento.

## Fase 5 — Robustez/UX

16. SWR foldback (si hay puente), CAT_TX status, DIAG ligero, guía de
    perfiles (voz/DX/digital/CW).

## Log de medidas (Fase 0 en adelante)

| Fecha | Build flash/RAM | CPU RX % | CPU TX % | loops/s | Nota |
|---|---|---|---|---|---|
| 2026-09-10 | 29820 / 1157 | — (medir PD5) | — (medir PD5) | — (medir) | base pre-Fase 1 |
| 2026-09-10 | — | — | — | — | AB base TX: img −36dBc, imd3 −8dBc, car −60…−90dBc (drive 2–6, mic 60–300); RX: piso 0, pico/hd2/hd3 por filtro (ver tests/ab/ab_last.txt) |
