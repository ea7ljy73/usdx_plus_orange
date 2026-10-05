# uSDX Plus Orange — Roadmap (v2.0.0 en producción + laboratorio)

## Estado actual (2026-09-15)

- **Producción en la raíz**: `usdx_plus_orange.ino` + módulos, versión
  `v2.0.0`, con CAT. Build: **30804 B (95 %) flash / 1189 B (58 %) RAM**,
  0 warnings. Margen: ~1450 B flash, ~859 B RAM.
- **Paridad verificada con `usdx-legazy/`** (tests host, ver `AGENTS.md`):
  TX `ssb()` bit-exacto; RX core/filtros/AGC 0 mismatches (NR ≥2 diverge
  intencionado: NR de 2 polos).
- **Mejoras integradas tras flag de menú** (todas apagables, default legacy):
  S-meter punto fijo, `freq_calc_fast`, I2C bulk diferencial, banda
  direccional, mic atten, DIGI/FT8 plano, ALC, compresor voz 2:1, LoCut,
  NR 2-polos, AGC Start (precarga x8), AGC Rec (anti-bombeo), AGC Slow M0PUB.
- **Laboratorio en `usdx-lab/`**: copia + `eng_lab.h` (monitor ADC/audio),
  sin CAT, versión/magia EEPROM propias. Toda prueba hardware se hace ahí
  (guía: `usdx-lab/GUIA_TEST_HW.md`).
- **Puerta pendiente**: validación en hardware (`HARDWARE_TEST_GUIDE.md` +
  `usdx-lab/GUIA_TEST_HW.md`). Hasta entonces, ningún comportamiento
  RF/audio se da por cerrado.

## Pendiente (en orden)

1. **Validación HW en banco con usdx-lab** (puerta de release): ADC, audio,
   filtros, AGC, TX a 50 Ω, potencias/consumos. Rellenar hoja de medición.
2. **Instrumentación lab** (no toca producción): detectores selectivos
   (Goertzel f1/f2/IMD3), FFT64/128, generador dos tonos interno, volcado
   Serie a PC (ver plan en `usdx-lab/`).
3. **Backlog aparcado por falta de flash** (B4–B9, LMS/Goertzel, ver abajo):
   solo si se libera margen real medido.
4. **Fase 5 robustez**: SWR foldback (si hay puente), guía de perfiles
   (voz/DX/digital/CW).

## Reglas de juego (vigentes)

- Cada mejora: conmutada por menú/flag (apagable), test A/B host + medida en
  hardware, presupuesto flash/RAM declarado. Sin mejora medible → rollback.
- Orden: instrumentar → liberar margen → fluidez → TX → RX → robustez/UX.
- Descartado explícito: espectro en pantalla (pide OLED+CPU), CAT streaming
  audio (CPU+flash), QUAD (daña TX), más bandas.
- Producción: ver `AGENTS.md` (`## Producción`).

## Techos hardware (límites físicos, no negociables)

- ADC 10 bit + oversampling 4x → ~72 dB DR en 2,4 kHz. Micro sin amplificar
  ~1 mV/LSB: la ganancia de micro es crítica.
- Envolvente PA por PWM 8 bit → 48 dB de rango AM; fase desde multisynth
  SI5351 con cuantización → espurios laterales inevitables.
  IMD3 ~−33 dBc es el techo práctico de la arquitectura (clase-E + EER).
- CPU @20 MHz: ISR RX a 62,5 kHz = 320 ciclos/muestra; ISR TX a 4,8 kHz =
  4166 ciclos, de los cuales el I2C bulk (~88 µs ≈ 1760 ciclos) se come ~40 %.
- Sin `float` en el path principal (smeter a punto fijo desde Fase 1; resto
  entero; SI5351 usa división 64 bit: cara pero puntual).

## Historial — Fase 0: instrumentación (completada)

- [x] Medidor carga CPU: pin sonda PD5 (toggle en ISRs) + loops/s por Serial.
- [x] Script de tamaño: totales + top símbolos por build, CSV por commit.
- [x] Harness A/B host: IMD dos tonos por `ssb()`, SNR/rechazo por RX.

## Historial — Fase 1: liberar margen

- [x] 1. S-meter `float`→punto fijo con LUT entera de log10 (−1158 B flash;
  maxerr 0,61 dB vs exacto, verificado en host; legacy truncaba + UB att2=16).
- [x] 2. `freq_calc_fast` exacto sin división 64 bit (+742 B, ~3–4× menos
  ciclos; bit-exacto probado en host en 19,7M combinaciones div/df/fxtal).
- [x] 3. I2C bulk diferencial, solo regs cambiados (+80 B, +5 B RAM; mismos
  bytes en bus; **pendiente validar TX + duty PD5 en HW**).

## Historial — Fase 2: fluidez

- [x] 4. No reprogramar SI5351 si `df==0` (subsumido en bulk diferencial F1.3).
- [x] 5. Cambio de banda direccional según último sentido del dial (GW8RDI 4.00d).

## Historial — Fase 3: TX (por dB/esfuerzo)

- [x] 6. Mic gain/atten por menú: atten runtime 0–4 (6 dB/paso, eslot 22).
  AB: con menos nivel mejora imagen (−36,6→−39 dBc). Mic gain on/off
  (cambio de algoritmo) diferido a lote compresor/EQ.
- [x] 7. Perfil DIGI/FT8: DIGI Mode por menú (eslot 23) con path TX plano
  (fórmula legacy DIG_MODE). AB: voz +4,7 dB @2000 vs 400; digi 0,5 dB.
  Sin preset auto (el usuario pone USB/VOX/BW).
- [x] 8. Hard-clipper/ALC: ALC en `ssb()` (ataque 100 ms, release 1 s/paso,
  máx −18 dB; excluye drive=8 y dig_mode). Sin display ni menú (+138 B).
  AB tonos: usb −18 dB ante saturación sostenida (esperado); IMD de fase sin
  cambio (−8 dBc). PENDIENTE HW voz real.
- [x] 9. CESSB: EVALUADO Y REVERTIDO. Con drive en saturación y ALC activo,
  un pre-clipper I/Q no puede actuar nunca (todo lo que supera _amp 16 sale
  plano a drive 4); en zona lineal nunca se alcanza el umbral. Medido en A/B
  (barrido 200–3000, envolvente AM y onset con estado virgen): 0 diferencia.
  Se mantiene ALC, que sí actúa sobre PA real.
10. Hilbert IIR TX mejorado solo con margen CPU; AM `sqrt` y FM `arctan`
    como opciones a evaluar.
11. CW: sidetone por menú; verificar rampas en hardware.

## Historial — Fase 4: RX

- [x] 12. AGC hang: EVALUADO Y REVERTIDO. Sin diferencia medible (dinámica
  lenta del AGC domina; bug uint8→uint16 cazado por el camino). Paridad intacta.
- [x] NB impulsos RX: PORTADO de v1 Y REVERTIDO. Post-Hilbert no puede actuar
  (ringing 14 taps lo puentea + integrador acumula). Un blanker útil iría
  pre-Hilbert. Sin efecto medido.
- [x] Drive-by: `MENU_IDX_CWMSG` desactualizado tras inserts (apuntaba a TX Comp,
  disparaba CQ al editar comp). Corregido a 26 y reverificado tras revert NB.
- [x] 13. NR 2-polos en cascada (12dB/oct, cortes ~3000..850Hz, +~110 B).
  AB: tono intacto en todos los niveles, ruido 53,7→35,3 monótono.
  Divergencia intencional legacy en nr>=2 (paridad TX exacta, RX resto 0 %).
- [x] 16. AGC arranque rápido (F4.16, menú "AGC Start" OFF/ON, eslot 33 banco
  extra; OFF = legacy exacto). Legacy arranca gain x1 y tarda ~7 s en cargar
  con banda floja (medido host); ON precarga x8 (8192): audible al instante,
  t90 7,1→5,3 s en ruido débil, sin overflow int16 (pico 16000 con in=2000) y
  asentamiento <=0,33 s en señal fuerte. Algoritmo/equilibrio intactos (solo
  condición inicial al arrancar): paridad RX 0 mismatches con flag OFF.
  (+52 B flash, +1 B RAM). PENDIENTE validar en HW.
- [ ] 14. LMS notch solo CW / 15. Goertzel (aparcados: flash al 95 %).
14. LMS notch solo en path CW (en voz interfería).
15. S-meter calibrado real tras punto fijo; Goertzel CW como experimento.

## Historial — Fase 5: robustez/UX

17. SWR foldback (si hay puente), CAT_TX status, DIAG ligero, guía de
    perfiles (voz/DX/digital/CW).

## Backlog RX investigado (2026-09-14; QMX, Flex, Yaesu, Icom, WDSP/Thetis,
GW8RDI, F5NPV — SIN implementar; ordenado por valor/coste para AVR)

Regla: si algo se evalúa y no funciona o no mide mejor → revert + entrada
"EVALUADO Y REVERTIDO" con motivo, y no se reintenta por esa vía.

- [x] B1. AGC anti-bombeo como F4.17 "AGC Rec" (divisor recovery 1..8, 1=legacy;
  eslot 34 banco extra). Solo ralentiza la SUBIDA (+1 cada N muestras); ataque
  intacto, mismo equilibrio. A/B host ráfagas: varianza soplo 18,3→2,5 (rec=4),
  overshoot flanco 625→250 (ya no clipa), rampa débil ×N (cubierta por F4.16).
  (+62 B flash, +2 B RAM). Knee bajo umbral (QMX Threshold, retorno rápido a
  máx): EVALUADO Y REVERTIDO — A/B mostró soplo 4x, varianza 15x y clipping en
  flancos; no reintentar por esa vía. PENDIENTE HW (recomendado rec=4).
- [ ] B2. Blanker pre-Hilbert: EVALUADO Y REVERTIDO (2026-09-14). Implementado
  (sample-and-hold, media U32 x4096 tau 131ms, suelo 150, k 8/4/2, menú NB) y
  medido en tests/ab_nb (eliminado tras revertir): en este tap (tras LPF
  analógico 1.5kHz) un crash real (ADC pegado ±512) y un tono fuerte (400) son
  indistinguibles por amplitud — solo servía en banda quieta — y costaba ~430B
  (flash al 99%, sin margen publicable). De paso cazó: bug unsigned-underflow
  en EA de media (`avg += a-mean` con a<mean explota: usar resta saturada) y
  bug de driver (runs sin fork contaminan estado DSP CIC/Hilbert: fork por
  run). Slew-rate como alternativa: descartado sobre el papel (LPF difumina
  crash a ~85/muestra vs ~154 legítimo en 3.4kHz full-scale: sin margen).
  No reintentar blanker de amplitud en este tap.
- [x] B3/F4.18. AGC Slow/M0PUB opcional (legacy `FAST_AGC`: menú AGC 0..2
  OFF/Fast/Slow, default 1 intacto; `process_agc` ya estaba portado, ahora
  enlazado). A/B host dits CW: nivel ON var 97,9→1,4 (70x más estable),
  overshoot 191→118 sin clipa. (+286 B flash: cuerpo M0PUB, +2 B RAM).
  PENDIENTE HW.
- [ ] B4. Bloqueador DC pre-AGC (todos): HPF 1-polo ~100 Hz; el DC de desbalance
  IQ/deriva ADC bombea el AGC. ~15 ciclos @7812 Hz. A/B: inyectar DC, ganancia
  estable.
- [ ] B5. Detector AGC post-filtro anti-desense (slices Flex/Yaesu): medir nivel
  tras `filt_var()` para que un adyacente fuerte no hunda la señal útil. Medio.
  A/B con interferente adyacente simulado.
- [ ] B6. Trim balance IQ en amplitud (mcHF 60 dB mirror, QMX image sweep): hoy
  solo fase (`rx_ph_q`); 1 mult int16 @7812 Hz + menú. Lo caro es la metodología
  HW (anular imagen inyectada). Alternativa HW F5NPV (cargar FST3253 10-50 Ω).
- [ ] B7. Squelch con histéresis (todos; útil FM) + B8. Volumen sidetone CW por
  menú (GW8RDI `CW_VOLUME`, F5NPV). Decenas de bytes c/u.
- [ ] B9. ATT automático (filosofía QMX RF-gain + docs uSDX 80/40 m): seguir
  estado AGC con histéresis (sin clicar relés). Medio; gran valor portable.
- Parked: ANF LMS notch (Flex ANF/Yaesu DNF/Icom auto-notch; sin flash),
  `NR_FIR` GW8RDI (nuestro 2-polos ya mide bien), NR espectral/RNNoise +
  waterfall (imposibles en AVR), Goertzel decoder (QMX lo usa; aparcado),
  Twin-PBT/sharp-soft (Icom), APF CW (cubierto por filtros 500/200/100/50).

## Log de medidas (Fase 0 en adelante)

| Fecha | Build flash/RAM | CPU RX % | CPU TX % | loops/s | Nota |
|---|---|---|---|---|---|
| 2026-09-10 | 29820 / 1157 | — (medir PD5) | — (medir PD5) | — (medir) | base pre-Fase 1 |
| 2026-09-10 | — | — | — | — | AB base TX: img −36dBc, imd3 −8dBc, car −60…−90dBc (drive 2–6, mic 60–300); RX: piso 0, pico/hd2/hd3 por filtro (ver tests/ab/ab_last.txt) |
| 2026-09-10 | 29484 / 1166 | — | — | — | fin Fase 1 (−336 B netos). AB idéntico al base (F1 sin cambio DSP). PENDIENTE HW: TX ok + duty PD5 (F1.2/F1.3) |
| 2026-09-11 | 30078 / 1172 | — | — | — | F2.5 + F3.6 (+594 B). AB: atten mejora imagen −36,6→−39 dBc |
| 2026-09-11 | 30188 / 1172 | — | — | — | F3.7 DIGI (+110 B). AB flat: voz +4,7dB, digi 0,5dB |
| 2026-09-11 | 30326 / 1177 | — | — | — | F3.8 ALC (+138 B). AB tonos: usb −18dB sostenido |
| 2026-09-11 | 30326 / 1177 | — | — | — | F3.9 CESSB revertido (0 efecto medido). Queda: comp/EQ si cabe |
| 2026-09-11 | 30476 / 1180 | — | — | — | F3.9b comp voz 2:1 (+150 B, menú+eslot 24). AB mixto: IMD −10,3→−12,6dBc |
| 2026-09-11 | 30598 / 1183 | — | — | — | F3.9c LoCut (+122 B, menú+eslot 32 banco extra). AB: −3,6/−9/−19dB@100Hz |
| 2026-09-11 | 30724 / 1185 | — | — | — | F4 NR 2-polos (+126 B). AB: tono intacto, ruido 53,7→35,3 |
| 2026-09-14 | 30456 / 1184 | — | — | — | F4.16 AGC arranque rápido (+52 B/+1 B, menú+eslot 33). AB host: t_aud 1,46→0 s, t90 7,1→5,3 s (ruido débil); paridad RX 0 mismatches (flag OFF). PENDIENTE HW |
| 2026-09-14 | 30518 / 1187 | — | — | — | F4.17 AGC Rec (+62 B/+2 B, menú+eslot 34). AB ráfagas: var soplo 18,3→2,5, overshoot 625→250; knee EVALUADO Y REVERTIDO (soplo 4x/var 15x/clip). Paridad 0 (rec=1). PENDIENTE HW |
| 2026-09-14 | 30804 / 1189 | — | — | — | F4.18 AGC Slow M0PUB (+286 B/+2 B, menú AGC 0..2 default 1). AB dits: var nivel 97,9→1,4. Paridad 0 (agc 0/1). PENDIENTE HW |
| 2026-09-14 | 30804 / 1189 | — | — | — | B2 blanker REVERTIDO (no separable en este tap + 99% flash). Vuelve a 30804/1189. Ver B2 |
| 2026-09-15 | 30804 / 1189 | — | — | — | Fusión a raíz (v2.0.0 producción) + `usdx-lab/` creado (v2.0.0-lab, sin CAT, Engineer ADC/AUDIO). Paridad reverificada. Sin cambio de build |
