# uSDX Plus Orange — Roadmap (v2.1.0 en producción + laboratorio)

## Estado actual (2026-10-25)

- **Producción en la raíz**: `usdx_plus_orange.ino` + módulos, versión
  `v2.1.0` + CAT TS-480 ampliado en `dev` (`F_VER_ID` 5, sin cambio de EEPROM). Build: **31964 B (99 %) flash / 1097 B (53 %) RAM**,
  0 warnings. Margen: ~1468 B flash, ~860 B RAM.
- **Tests**: `./tests/run_all.sh --build --avr` en verde (paridad TX/RX,
  A/B AGC, menú, fw_sim, sincronía, simavr, compilación prod y lab).
- **Paridad verificada con `usdx-legazy/`** (tests host, ver `AGENTS.md`):
  TX `ssb()` bit-exacto; RX core/filtros/AGC 0 mismatches (NR ≥2 diverge
  intencionado: puerta de ruido DNR).
- **Mejoras integradas tras flag de menú** (todas apagables, default legacy):
  mic atten, DIGI/FT8 plano, TX Comp (recorte de envolvente 0..3), LoCut,
  TX EnvPre, puerta de ruido NR (suelo adaptativo), AGC Start (precarga x8),
  AGC Hang (anti-bombeo, AGC Slow), AGC Slow M0PUB. Siempre activas:
  S-meter punto fijo, `freq_calc_fast`, banda direccional, ALC de pico.
  (El I2C bulk diferencial F1.3 se retiró el 2026-10-13, ver Fase 2.)
- **Laboratorio en `usdx-lab/`**: copia + `eng_lab.h` (monitor ADC/audio),
  sin CAT, versión/magia EEPROM propias. Toda prueba hardware se hace ahí
  (guía: `usdx-lab/GUIA_TEST_HW.md`).
- **Puerta pendiente**: validación en hardware (`HARDWARE_TEST_GUIDE.md` +
  `usdx-lab/GUIA_TEST_HW.md`). Hasta entonces, ningún comportamiento
  RF/audio se da por cerrado.

## Correcciones 2026-10-13 (detectadas con `tests/avr_sim`, firmware real en simavr)

- **`F_CPU` no se redefinía** (legacy 135-139 sí: 20007000). Timer2 iba a
  78125 Hz en RX y 6010 Hz en TX, ICR1 a 204: RX con 2 % de CPU libre, TX
  perdiendo el 18,7 % de las muestras y tonos desplazados un 2-3 %.
  Corregido en `usdx_settings.h` (producción y lab).
- **ISR de TX**: `freq_calc_fast` reescrito en 32 bit (bit-exacto; el
  "rápido" de Fase 1 costaba ~1000 ciclos, como la división de legacy);
  `/3` y `/9` de `ssb()` por multiplicación exacta; compresor con división de
  16 bit (la de 32 desbordaba: 4300 ciclos de media para 4160 de periodo).
  Resultado simulado: 3456 ciclos de media (83 %), máx 4007; con voz fuerte +
  compresor + low-cut 3846 (92,5 %), máx 4085. Legacy: 3803 (91,4 %), máx 4596.
- Emisión de dos tonos reconstruida del firmware real: IMD3 −29 dBc
  (legacy −8,9 dBc, envolvente 89 % del tiempo en el tope).
- **F3.10 TX EnvPre (menú, eslot 37, default 0 = legacy)**: preénfasis de
  envolvente `a' = a + c1·(a − a_ant)`, c1 = 0,5/1/1,5. Compensa el retardo y
  el suavizado de las dos células RC 10k/10n entre KEY_OUT y las puertas del
  PA (esquema `usdx.png`). Simulado con el firmware real + modelo RC, dos
  tonos: IMD3 −19,9 (off) → −25,6 / **−31,6** / −28,3 dBc (1/2/3); IMD5
  −22,0 → −29,8; canal adyacente −26,3 → −29,6 dBc. +30 ciclos de ISR,
  +98 B flash. Si la placa NO lleva ese RC empeora (−29 → −20 dBc): ajustar
  en banco con dos tonos. PENDIENTE HW.
- **Medidor S**: `__builtin_clz` opera sobre `int` de 16 bit en AVR; el
  log2 salía 16..31 y el medidor marcaba −4…+14 dBm donde legacy marca
  −109…−52. Corregido con `__builtin_clzl`; simulado: −96 dBm (3 cuentas) y
  −53 dBm (400), legacy −95 / −52.
- **RX simulado (firmware real)**: rechazo de banda lateral −78 dB (legacy
  −80), respuesta idéntica a legacy (−0,4 / −4,2 / −9,8 / −19,2 dB a 300 /
  2000 / 2800 / 3600 Hz), IMD3 −47 dBc, sin muestras perdidas, 76 % de CPU.
- **CW simulado**: rampas de 1,08 ms tras el RC, clics −39,6 dB fuera de
  ±500 Hz, sidetone 607 Hz: igual que legacy.
- **PB3/SIG_OUT (pin 17)**: quedaba como entrada flotante; ahora salida a
  LOW como legacy (4242/4249). Estado tras el arranque idéntico a legacy
  (92 registros/variables, `run_avr_sim.sh --legacy`).
- **Ruido RX sin AGC "intermitente" (0,83 vs 0,60 cuentas)**: era un artefacto
  del analizador del simulador, no del firmware. La rejilla de remuestreo del
  audio caía, según el desfase de arranque, justo sobre el instante en que la
  ISR escribe OCR1AL. Ahora la rejilla se aleja de las escrituras: producción
  y legacy dan 0,60 / 0,56 cuentas en todos los arranques.
- **Mejoras de TX comprobadas en el simulador** (firmware real, 2026-10-13):
  - *TX LoCut*: −3 dB a ≈100 / 200 / 350 Hz (1/2/3), como se diseñó; +1…2,5 dB
    de realce de agudos en 2/3 (propio del filtro de un polo).
  - *Mic Atten*: ≈6,2 dB por paso. *DIGI*: respuesta más plana (−1,4 dB a
    300 Hz frente a −3,8) y tono tipo FT8 mucho más limpio a nivel alto
    (resto en banda −38 dBc frente a −27; banda opuesta −44 frente a −34).
  - *TX Comp* (compresor de audio 2:1): **no cumplía**. Con habla sintética
    (`--speech`) la potencia media no subía (−0,6…−1,3 dB) y la ISR de TX
    desbordaba (máx 4457 de 4160 ciclos). **Rehecho** como recorte de
    envolvente tras el ALC, niveles 0..3 (el ALC deja pasar picos hasta
    255+128·nivel y se recorta a 255): +3,3 / +5,0 / +6,2 dB de potencia
    media, canal adyacente sin cambio, 0 ciclos de ISR y −146 B de flash.
    Coste: IMD3 de dos tonos −32 (0) / −18 / −13,5 / −11,5 dBc (legacy −9).
    Mismo eslot de EEPROM (24); un "ON" guardado pasa a ser nivel 1.
  - Peor caso de CPU (habla fuerte + TX Comp 3 + LoCut): media 3466, máx
    4116 de 4160 ciclos, 0 muestras perdidas.
- **Mejoras de RX comprobadas en el simulador** (escenario `rxt`: señal y
  ruido programables en I/Q, EEPROM real, audio analizado; regresión en
  `tests/avr_sim/rx_mejoras.py`, dentro de `run_avr_sim.sh`):
  - *AGC Start*: cumple. Señal débil desde el encendido: 4–5× más audio el
    primer segundo y 2–3 s de adelanto; mismo régimen final.
  - *AGC Rec* (divisor de subida del AGC Fast): **no cumplía**. Bajaba el
    audio de la propia voz (hasta 13 dB con señal floja) y alargaba 8× la
    recuperación tras una señal fuerte; el bombeo que debía quitar ya era
    ≤3 dB. **Sustituido por *AGC Hang*** (mismo eslot, 1 = legacy): retención
    de (n−1)·0,1 s en el AGC Slow (M0PUB). Con Hang 5 el bombeo en pausas de
    0,5 s pasa de 3,9 a 0,3 dB y una señal 20 dB más débil vuelve en ~1 s
    (AGC Fast: >7 s).
  - *NR ≥2*: **no cumplía**. El umbral absoluto quedaba por encima del nivel
    que regula el AGC: la puerta no abría nunca y NR atenuaba por igual voz
    y ruido (−6/−12/−18/−24 dB, S/N sin cambio). **Rehecho** con umbral
    relativo al suelo de ruido (3× el mínimo de envolvente): palabras 0,0 dB,
    banda vacía −6/−12/−18/−23 dB y pausas −6/−11,5/−16/−17 dB (NR 2/4/6/8).
    Límites: atenúa señales a <10 dB del ruido y tonos continuos a los 2–3 s.
  - −158 B de flash entre los dos cambios.
- Build: 30780 B (95 %) flash / 1188 B (58 %) RAM.

## CAT TS-480 ampliado (2026-10-25)

- Nuevos (sintaxis TS-480, para flrig/Hamlib/WSJT-X y usdxcontrol): `AG0`
  volumen, `SM0` S-meter (S9 = 15), `GT` AGC, `RA` atenuador, `NR`/`RL`, `PC`
  potencia, `MG` micro, `PR` TX Comp, `KS` keyer, `VX` VOX, `RT`/`RU`/`RD`
  RIT, `FB` VFO B, `FR`/`FT` (sin split) y **`EX`**: los 34 parámetros
  numéricos del menú por su número de LCD. `IF` con RIT, TX/RX y VFO.
- Un único mecanismo sobre `MENU[]`: rango, EEPROM y `on_change` como el menú;
  lo que toca RF se rechaza en TX. Respuestas montadas en `CATcmd` con `F()`:
  −91 B de RAM. +1176 B de flash en total.
- Encontrado con `tests/avr_sim --scenario cat` (UART real):
  - `display_tick()` reponía el cursor cada 100 ms aunque no pintase nada: cada
    escritura de la LCD apaga la UART (pines compartidos) y se perdía ~1 % de
    los bytes de CAT (comandos desalineados). Con `smode` 0 ya no escribe.
  - `tx` es el latch de portadora (`CARRIER_COMPLETELY_OFF_ON_LOW`): en TX por
    CAT cae a 0 en cada pausa de la voz, y `cat_apply()` habría tocado el
    SI5351 con la ISR de TX activa. Ahora el estado de TX sale de `OCR2A`.
  - El S-meter se calcula aunque `smode` sea 0 (para `SM;`).
- Fix de paso: `on_vfosel()` (VFO A/B desde el menú) pisaba el otro VFO.
- Fuera por flash (se pueden recuperar si se libera margen): `KY` (≈150 B),
  `BU/BD` (≈90 B), `UP/DN` (≈110 B), `FW` (≈100 B); sus funciones se cubren con
  `EX`/`FA`.
- PENDIENTE HW: con el adaptador UART-USB (§7 de `HARDWARE_TEST_GUIDE.md`).

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
  **F1.3 RETIRADO 2026-10-13** (rollback a bulk completo legacy, −100 B):
  medido en `tests/fw_sim`: hacía variable el instante de aplicación de
  frecuencia/amplitud (+2,7 dB de IMD3 a 7 MHz con envolvente limpia) y no se
  recuperaba de una escritura I2C perdida con df constante. Se conserva
  `freq_calc_fast` sin división de 64 bit (bit-exacto, verificado).
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
  **Rehecho 2026-10-13 (ALC de pico)**: el de pasos recortaba a 255 ANTES de
  atenuar (misma distorsión con −18 dB de potencia). Ahora ganancia fina
  aplicada antes del recorte: ataque 0,56 dB/muestra, recuperación ~5 dB/s.
  Simulado (`tests/fw_sim/txamp_sim`, `txtiming_sim`), dos tonos saturando a
  drive 2–6: IMD3 −8…−10 → −28…−30 dBc, PEP completa, potencia media −3,6 dB
  (el máximo de dos tonos limpios es −3,0). En reposo, bit-exacto con legacy.
  Coste 0 B flash, −2 B RAM. PENDIENTE HW.
- [x] 9. CESSB: EVALUADO Y REVERTIDO. Con drive en saturación y ALC activo,
  un pre-clipper I/Q no puede actuar nunca (todo lo que supera _amp 16 sale
  plano a drive 4); en zona lineal nunca se alcanza el umbral. Medido en A/B
  (barrido 200–3000, envolvente AM y onset con estado virgen): 0 diferencia.
  Se mantiene ALC, que sí actúa sobre PA real.
10. Hilbert IIR TX mejorado solo con margen CPU; AM `sqrt` y FM `arctan`
    como opciones a evaluar.
11. CW: sidetone por menú; verificar rampas en hardware.

## Historial — Fase 4: RX

- [x] 12. AGC hang (sobre AGC Fast): EVALUADO Y REVERTIDO. Sin diferencia medible (dinámica
  lenta del AGC domina; bug uint8→uint16 cazado por el camino). Paridad intacta.
- [x] NB impulsos RX: PORTADO de v1 Y REVERTIDO. Post-Hilbert no puede actuar
  (ringing 14 taps lo puentea + integrador acumula). Un blanker útil iría
  pre-Hilbert. Sin efecto medido.
- [x] Drive-by: `MENU_IDX_CWMSG` desactualizado tras inserts (apuntaba a TX Comp,
  disparaba CQ al editar comp). Corregido a 26 y reverificado tras revert NB.
  **Se repitió con TX EnvPre (2026-10-15)**: ahora se identifica la fila por tipo
  `P_TEXT`, sin índice fijo.
- [x] 13. NR expansor (puerta DNR, F4; sustituye al 2-polos: el paso-bajos
  empastaba la voz y el suelo adaptativo la mataba en régimen permanente).
  Puerta con umbral por nivel compensado por volumen + histéresis + ganancia
  suavizada; ABIERTA = bit-exacto. AB: tono 23,9 intacto en todos los niveles,
  ruido 53,7→30,6→27,2→13,6→13,6→6,8→6,8→3,4 (−5…−24dB monótono).
  Caveats (como Yaesu/Icom): voz débil bajo umbral duckea en nr alto (usar nr
  bajo para DX), mejor con AGC on. Divergencia intencional legacy en nr>=2
  (nr 0/1 bit-idénticos; paridad RX resto 0 %).
  (+444 B flash, +4 B RAM). PENDIENTE validar en HW (oído: pausas mudas,
  sin bombeo/aspereza en nr 2-4).
  **Rehecho 2026-10-13** con umbral relativo al suelo de ruido (ver arriba).
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

- [x] B1. (**Sustituido 2026-10-13 por AGC Hang**, ver arriba.) AGC anti-bombeo como F4.17 "AGC Rec" (divisor recovery 1..8, 1=legacy;
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
  `NR_FIR` GW8RDI (puerta actual mide mejor y cabe), NR espectral/RNNoise +
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
| 2026-09-17 | 31248 / 1193 | — | — | — | F4 NR expansor-puerta (+444 B/+4 B, sustituye 2-polos). AB: tono 23,9 intacto 0-8, ruido 53,7→3,4 (−5…−24dB). Paridad 0 (nr 0/1). PENDIENTE HW |
| 2026-09-17 | 31264 / 1193 | — | — | — | Menú: doble-click (banda/filtro) puerta en MAIN + move con módulo real (+16 B). Divergencia intencional legacy (en legacy saltaba editando). Cazado en banco (lab). PENDIENTE HW |
| 2026-10-09 | 31470 / 1195 | — | — | — | Fix paridad: CAT FA/MD fuera de TX + `RC;`, RIT=0 al arrancar, keyer cancela CQ, decoder CW legacy, debounce + puerta PT + timeout ADC |
| 2026-10-13 | 31390 / 1188 | — | — | — | ALC de pico (F3.8 rehecho) + rollback F1.3 a bulk completo legacy (−100 B) |
| 2026-10-13 | 30920 / 1188 | — | — | — | `F_CPU` 20007000, `freq_calc_fast` 32 bit, `/3` `/9` por multiplicación; `tests/avr_sim` |
| 2026-10-13 | 31018 / 1190 | — | — | — | F3.10 TX EnvPre (+98 B) |
| 2026-10-13 | 31084 / 1190 | — | — | — | Fix medidor S (`__builtin_clzl`), PB3 como legacy, modo USB por defecto |
| 2026-10-13 | 30938 / 1188 | — | — | — | TX Comp rehecho como recorte de envolvente (−146 B) |
| 2026-10-13 | 30780 / 1188 | 76 (simavr) | 83 (simavr) | — | NR rehecho (suelo adaptativo) y AGC Hang sustituye a AGC Rec (−158 B) |
| 2026-10-15 | 30788 / 1188 | — | — | — | Fix: el CQ se disparaba en `CQ Interval` en vez de `CQ Message` (índice fijo desfasado al insertar TX EnvPre); ahora por tipo `P_TEXT` + check en `tests/check_sync.py` (+8 B) |
| 2026-10-25 | 31964 / 1097 | — | — | — | CAT TS-480 ampliado (AG/SM/GT/RA/NR/RL/PC/MG/KS/VX/PR/RT/RU/RD/FB/FR/FT + EX sobre `MENU[]`, +1176 B / −91 B RAM); LCD sin escribir con `smode` 0; TX por `OCR2A`; fix `on_vfosel`. Probado en host y simavr (UART real). PENDIENTE HW |
