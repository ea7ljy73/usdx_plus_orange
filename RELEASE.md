# uSDX Plus Orange — Release Notes

**Current version:** v2.1.0
**Base:** uSDX legacy 1.02x (PE1NNZ) / WhiteButtons (GW8RDI)
**Platform:** ATMEGA328P @ 20 MHz
**Author:** EA7LJY - Julian

Every improvement is behind a menu option and defaults to legacy behaviour.
Figures come from the host tests and the AVR simulator (`tests/`); hardware
validation is still in progress. Details and measurements: `ROADMAP.md`.

---

## v2.1.0 — 2026-10-15

**Memory:** 30788 bytes flash (95 %), 1188 bytes RAM (58 %)

> ⚠️ **EEPROM reset on first boot** (`F_VER_ID` 5). Power on holding the
> encoder button if the settings look wrong.

### Fixes (found running the real firmware in the AVR simulator)
- **`F_CPU` not redefined**: timers ran at 78125/6010 Hz instead of
  62500/4808 Hz. RX had 2 % CPU left and TX lost 18.7 % of its samples.
- **TX ISR overrun**: `freq_calc_fast` rewritten in 32 bit (bit-exact),
  `/3` and `/9` replaced by exact multiplications. Average load 83 % (legacy 91 %).
- **S-meter**: `__builtin_clz` on a 16-bit `int` made it read up to +100 dB high.
- **PB3/SIG_OUT** left floating; now an output driven LOW, like legacy.
- **Menu**: "CQ Interval" triggered the CQ message instead of "CQ Message".
- **CAT**: `FA`/`MD` received during TX are applied on return to RX (they
  never touch the SI5351 or LPF while transmitting); `RC;` answered.
- RIT and VOX always off at power-up; CW decoder identical to legacy.

### Reworked improvements
- **Peak ALC** (always on): two-tone IMD3 when overdriven −8…−10 → −28…−30 dBc,
  full PEP.
- **TX Comp 0–3**: now envelope clipping after the ALC (+3 / +5 / +6 dB
  average power, 0 ISR cycles). A saved "ON" becomes level 1.
- **NR 2–8**: noise gate relative to the tracked noise floor (−6…−24 dB of
  noise in pauses, voice untouched).
- **AGC Hang 1–8** replaces AGC Rec: hold on AGC Slow; pumping 3.9 → 0.3 dB.
- Differential I2C bulk (F1.3) removed: back to legacy full bulk.

### New
- **TX EnvPre 0–3**: envelope pre-emphasis for the RC shaper before the PA
  gates. Two-tone IMD3 −20 → −32 dBc at level 2 (simulated). Worse on boards
  without that RC: set it with a two-tone test.

### Project
- MIT `LICENSE` with PE1NNZ's original notice.
- Full radio simulator (`tests/avr_sim`) and firmware simulations
  (`tests/fw_sim`), all run by `tests/run_all.sh`.

---

## v2.0.0 — 2026-09-15

**Memory:** 30804 bytes flash (95 %), 1189 bytes RAM (58 %)

- **Modular rebuild** of the firmware in the project root (`hw.h`, `rx.h`,
  `tx.h`, `cw.h`, `cat.h`, `vfo.h`, `menu.h`, `display.h`...), replacing the
  previous monolithic sketch.
- **Parity with legacy** checked sample by sample on the host: TX `ssb()`
  bit-exact; RX core, filters and AGC with 0 mismatches.
- **Menu improvements** (off = legacy): mic attenuator, DIGI flat TX path,
  ALC, TX compressor, TX low-cut, NR, AGC Start (x8 preload), AGC Rec,
  selectable AGC Slow (M0PUB).
- Always on: fixed-point S-meter, fast exact SI5351 calculation,
  directional band change.
- CAT TS-480 subset, VFO A/B + RIT/Split, Iambic A/B/Straight keyer, CW
  decoder and CQ messages.
- `usdx-lab/`: instrumented copy (ADC/audio monitor, no CAT) for bench tests.

---

**Disclaimer:** I am not responsible for any damage this firmware may cause to
the devices it is installed on. Use it at your own risk. Always test TX into a
50 Ω dummy load.
