# uSDX Plus Orange v2.1.0 — EA7LJY

## 🇪🇸 Español

**uSDX Plus Orange v2.1.0** es una reconstrucción modular del firmware uSDX (PE1NNZ, linaje WhiteButtons de GW8RDI) para ATmega328P. Parte de una regla estricta: **por defecto se comporta igual que el firmware legacy**. Cada mejora va en su propia opción de menú, se puede apagar y se ha medido antes de entrar.

### Qué aporta frente al legacy

Además de dejar de ser el fichero monolítico del legacy (ahora el código está separado en módulos: RX, TX, CW, CAT, menú, pantalla…), estas son las mejoras:

**Transmisión**
- **ALC de pico**: con dos tonos saturando, IMD3 de −8…−10 dBc a −28…−30 dBc manteniendo la PEP completa.
- **TX Comp (0–3)**: procesador de voz por recorte de envolvente tras el ALC. +3 / +5 / +6 dB de potencia media, sin coste de CPU.
- **TX EnvPre (0–3)**: preénfasis de envolvente que compensa el filtro RC antes de las puertas del PA. IMD3 de dos tonos de −20 a −32 dBc (nivel 2). Ajústalo en banco: si tu placa no lleva ese RC, empeora.
- **TX LoCut**: corte de graves a 100 / 200 / 350 Hz.
- **Mic Atten**: atenuador de micro de 6 dB por paso.
- **DIGI Mode**: camino de TX plano para FT8 y otros modos digitales; señal más limpia a nivel alto.
- **ISR de TX más ligera**: 83 % de carga media frente al 91 % del legacy, con más margen en el peor caso.

**Recepción**
- **NR por puerta de ruido** (niveles 2–8): sigue el suelo de ruido y quita 6 / 12 / 18 / 24 dB de ruido en las pausas y con la banda vacía, sin tocar la voz.
- **AGC Start**: la radio se oye desde el primer segundo, sin esperar 2–3 s a que suba la ganancia.
- **AGC Hang**: el bombeo de ruido entre palabras baja de 3,9 a 0,3 dB, y las señales débiles vuelven en ~1 s.
- **AGC Slow (M0PUB)** seleccionable, con nivel mucho más estable en CW.
- **S-meter en punto fijo**: más exacto que el legacy (error máximo 0,6 dB) y sin el fallo con ATT2 = 16.

**Uso y seguridad**
- Cambio de banda según el sentido del dial.
- Las órdenes CAT (TS-480) de frecuencia o modo que llegan durante TX se aplican al volver a RX: nunca se reprograma el oscilador ni los filtros mientras se transmite.
- Al arrancar, VOX y RIT siempre apagados: la radio nunca entra en TX al encender.
- Menú de 35 opciones con memoria en EEPROM.

**Calidad del código**
- Tests automáticos que comprueban, muestra a muestra, que el DSP de TX y RX da exactamente lo mismo que el legacy.
- El firmware compilado se ejecuta en un ATmega328P simulado ciclo a ciclo, con su SI5351. Ese simulador encontró fallos que el legacy arrastraba o que ningún test veía: timers mal calculados, S-meter desviado, ISR de TX desbordada.

**Antes de instalarlo**
- Después de flashear, enciende con el **botón del encoder pulsado** para volver a los valores de fábrica. Es obligatorio al venir de otra versión.
- Las cifras son **medidas en simulación**. La validación en hardware sigue en curso: prueba siempre el TX contra una carga de 50 Ω y comparte tus resultados.
- Código abierto con licencia MIT: https://github.com/ea7ljy73/usdx_plus_orange

---

## 🇬🇧 English

**uSDX Plus Orange v2.1.0** is a modular rebuild of the uSDX firmware (PE1NNZ, GW8RDI WhiteButtons lineage) for the ATmega328P. It follows one strict rule: **by default it behaves exactly like the legacy firmware**. Every improvement has its own menu option, can be turned off and was measured before going in.

### What it adds over legacy

Besides no longer being legacy's single monolithic file (the code is now split into modules: RX, TX, CW, CAT, menu, display…), these are the improvements:

**Transmit**
- **Peak ALC**: two-tone IMD3 when overdriven goes from −8…−10 dBc to −28…−30 dBc, keeping full PEP.
- **TX Comp (0–3)**: envelope-clipping speech processor after the ALC. +3 / +5 / +6 dB average power, at no CPU cost.
- **TX EnvPre (0–3)**: envelope pre-emphasis that compensates the RC network before the PA gates. Two-tone IMD3 from −20 to −32 dBc (level 2). Set it on the bench: it makes things worse if your board lacks that RC.
- **TX LoCut**: low cut at 100 / 200 / 350 Hz.
- **Mic Atten**: microphone attenuator, 6 dB per step.
- **DIGI Mode**: flat TX path for FT8 and other digital modes; cleaner signal at high level.
- **Lighter TX ISR**: 83 % average load against legacy's 91 %, with more headroom in the worst case.

**Receive**
- **Noise-gate NR** (levels 2–8): tracks the noise floor and removes 6 / 12 / 18 / 24 dB of noise in speech pauses and on an empty band, leaving the voice untouched.
- **AGC Start**: audio from the first second, instead of waiting 2–3 s for the gain to build up.
- **AGC Hang**: noise pumping between words drops from 3.9 to 0.3 dB, and weak signals come back in ~1 s.
- Selectable **AGC Slow (M0PUB)**, with a much steadier level on CW.
- **Fixed-point S-meter**: more accurate than legacy (0.6 dB max error) and without the ATT2 = 16 bug.

**Operation and safety**
- Band change follows the direction of the dial.
- CAT (TS-480) frequency or mode commands that arrive during TX are applied on return to RX: the oscillator and filters are never reprogrammed while transmitting.
- VOX and RIT always off at power-up: the radio never keys up when switched on.
- 35-entry menu stored in EEPROM.

**Code quality**
- Automated tests check, sample by sample, that the TX and RX DSP output is identical to legacy.
- The compiled firmware runs on a cycle-accurate simulated ATmega328P with its SI5351. That simulator found bugs that legacy carried or that no test could see: wrong timer setup, an offset S-meter, an overrunning TX ISR.

**Before you install it**
- After flashing, power on **while holding the encoder button** to restore factory defaults. This is required when coming from any other version.
- Figures are **measured in simulation**. Hardware validation is still in progress: always test TX into a 50 Ω dummy load and please share your results.
- Open source, MIT licence: https://github.com/ea7ljy73/usdx_plus_orange

73 de EA7LJY
