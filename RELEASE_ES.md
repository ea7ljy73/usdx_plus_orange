# uSDX Plus Orange — Notas de Release

**Versión actual:** v2.1.0
**Base:** uSDX legacy 1.02x (PE1NNZ) / WhiteButtons (GW8RDI)
**Plataforma:** ATMEGA328P @ 20 MHz
**Autor:** EA7LJY - Julian

Cada mejora va en una opción de menú y por defecto se comporta como legacy.
Las cifras salen de los tests host y del simulador AVR (`tests/`); la
validación en hardware sigue en curso. Detalle y medidas: `ROADMAP.md`.

---

## v2.1.0 — 2026-10-15

**Memoria:** 30788 bytes flash (95 %), 1188 bytes RAM (58 %)

> ⚠️ **Reset de EEPROM en el primer arranque** (`F_VER_ID` 5). Enciende con
> el encoder pulsado si los ajustes se ven raros.

### Correcciones (halladas ejecutando el firmware real en el simulador AVR)
- **`F_CPU` sin redefinir**: los timers iban a 78125/6010 Hz en vez de
  62500/4808 Hz. En RX quedaba un 2 % de CPU libre y en TX se perdía el 18,7 %
  de las muestras.
- **ISR de TX desbordada**: `freq_calc_fast` reescrito en 32 bit
  (bit-exacto) y `/3` y `/9` cambiados por multiplicaciones exactas. Carga
  media 83 % (legacy 91 %).
- **S-meter**: `__builtin_clz` sobre un `int` de 16 bit hacía marcar hasta
  +100 dB de más.
- **PB3/SIG_OUT** quedaba flotante; ahora es salida a LOW, como legacy.
- **Menú**: "CQ Interval" lanzaba el mensaje CQ en lugar de "CQ Message".
- **CAT**: los `FA`/`MD` recibidos en TX se aplican al volver a RX (nunca
  tocan el SI5351 ni el LPF transmitiendo); `RC;` responde.
- RIT y VOX siempre apagados al encender; decodificador CW idéntico a legacy.

### Mejoras rehechas
- **ALC de pico** (siempre activo): IMD3 de dos tonos saturando
  −8…−10 → −28…−30 dBc, con la PEP completa.
- **TX Comp 0–3**: ahora recorte de envolvente tras el ALC (+3 / +5 / +6 dB
  de potencia media, 0 ciclos de ISR). Un "ON" guardado pasa a nivel 1.
- **NR 2–8**: puerta de ruido relativa al suelo de ruido medido (−6…−24 dB
  de ruido en las pausas, la voz intacta).
- **AGC Hang 1–8** sustituye a AGC Rec: retención en AGC Slow; bombeo
  3,9 → 0,3 dB.
- Retirado el I2C bulk diferencial (F1.3): vuelve el bulk completo de legacy.

### Nuevo
- **TX EnvPre 0–3**: preénfasis de envolvente para el RC que hay antes de
  las puertas del PA. IMD3 de dos tonos −20 → −32 dBc con nivel 2
  (simulado). Empeora en placas sin ese RC: ajústalo con dos tonos.

### Proyecto
- `LICENSE` MIT con el aviso original de PE1NNZ.
- Simulador de la radio completa (`tests/avr_sim`) y simulaciones del
  firmware (`tests/fw_sim`), todo desde `tests/run_all.sh`.

---

## v2.0.0 — 2026-09-15

**Memoria:** 30804 bytes flash (95 %), 1189 bytes RAM (58 %)

- **Reconstrucción modular** del firmware en la raíz del proyecto (`hw.h`,
  `rx.h`, `tx.h`, `cw.h`, `cat.h`, `vfo.h`, `menu.h`, `display.h`...), en lugar
  del sketch monolítico anterior.
- **Paridad con legacy** comprobada muestra a muestra en host: TX `ssb()`
  bit-exacto; core RX, filtros y AGC con 0 mismatches.
- **Mejoras por menú** (apagadas = legacy): atenuador de micro, path TX plano
  DIGI, ALC, compresor TX, low-cut TX, NR, AGC Start (precarga x8), AGC Rec,
  AGC Slow (M0PUB) seleccionable.
- Siempre activas: S-meter en punto fijo, cálculo SI5351 rápido y exacto,
  cambio de banda direccional.
- Subconjunto CAT TS-480, VFO A/B + RIT/Split, keyer Iambic A/B/Straight,
  decodificador CW y mensajes CQ.
- `usdx-lab/`: copia instrumentada (monitor ADC/audio, sin CAT) para banco.

---

**Descargo de responsabilidad:** no me responsabilizo de los daños que este
firmware pueda causar en los equipos donde se instale. Úsalo bajo tu propia
responsabilidad. Prueba siempre el TX contra una carga ficticia de 50 Ω.
