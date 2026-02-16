# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

uSDX Plus Orange v1.12 is a refactored firmware for ATMEGA328P-based SDR transceivers. It implements SSB/CW/AM/FM transmission and reception with DSP features including filters, AGC, noise reduction, and VOX.

**Platform:** ATMEGA328P @ 20MHz (Arduino Uno compatible)
**Base:** uSDX Legacy 1.02x
**Author:** EA7LJY - Julian
**License:** MIT

## Build Commands

### Arduino CLI (Recommended)

```bash
# Compile for Arduino Uno
arduino-cli compile -b arduino:avr:uno

# Compile with hex file generation
arduino-cli compile -b arduino:avr:uno -e

# Upload (adjust port as needed)
arduino-cli upload -b arduino:avr:uno -p /dev/ttyUSB0
```

### Arduino IDE

1. Open `usdx_plus_orange.ino`
2. Select **Tools > Board > Arduino Uno**
3. Select **Tools > Port** (your programmer)
4. **Sketch > Upload** (Ctrl+U) to compile and flash

### Firmware Upload via ISP

Use `avrdude` with ISP programmer:

```bash
avrdude -c avrisp -b 19200 -P /dev/ttyACM0 -p m328p -U flash:w:firmware.hex:i
```

**Important:** Do not modify fuse settings during ISP programming. Default fuses are: E=FD H=D6 L=FF

## Architecture

### File Structure

```
usdx_plus_orange/
├── usdx_plus_orange.ino     # Main firmware (~56K lines, monolithic)
├── usdx_settings.h          # Compile-time configuration
├── usdx_filter.h            # DSP filter implementations (IIR)
├── ina219.h                 # INA219 power meter definitions
└── usdx-legazy/             # REFERENCE ONLY - DO NOT MODIFY
    ├── usdx.ino             # Original firmware for comparison
    ├── usdx_settings.h
    └── usdx_filter.h
```

### Main Firmware Architecture (usdx_plus_orange.ino)

The firmware is organized into sections:

1. **Hardware Pin Definitions** (lines 1-100)
   - LCD/OLED display pins
   - Rotary encoder pins
   - Audio I/O pins
   - SI5351 I2C configuration

2. **SI5351 Clock Synthesis**
   - Generates RF carriers for TX/RX
   - Phase control for SSB generation
   - I2C communication @ 800kbit/s

3. **DSP Processing Chain**
   - **RX Path:** Quadrature sampling → Hilbert transform → Sideband selection → Filter → AGC → Noise reduction → Audio out
   - **TX Path:** Audio in → Hilbert transform → Phase/amplitude extraction → SI5351 control + PWM envelope → PA

4. **Key DSP Algorithms**
   - **Hilbert Transform:** 90-degree phase shift for I/Q processing (15-element shift register)
   - **IIR Filters:** SSB (300-2900Hz) and CW (50-1000Hz) bandpass filters in `usdx_filter.h`
   - **AGC:** Automatic Gain Control with configurable attack/decay thresholds
   - **Noise Reduction:** Adaptive spectral noise gate with quadratic threshold
   - **Noise Blanker:** 4-sample suppression with linear interpolation

5. **TX Signal Generation**
   - Software-based SSB generation (no analog mixer)
   - Phase modulation via SI5351 frequency changes @ 4800 Hz update rate
   - Amplitude modulation via PWM envelope to PA
   - TX power ramping (32 steps, S-curve) to eliminate clicks

6. **CAT Interface**
   - TS-480 subset protocol over serial (38400 baud)
   - Supports frequency control, mode switching, PTT
   - Optional Bluetooth HC-05 support for wireless operation

7. **Menu System**
   - Rotary encoder + buttons for navigation
   - Settings stored in EEPROM (1KB)
   - 3-click exit strategy: exits to frequency and saves changes

### Configuration System (usdx_settings.h)

All compile-time features are controlled via `#define` switches:

**Core Features:**
- `DIAG` - Hardware diagnostics on startup (+1308 bytes)
- `CAT` - CAT interface for rig control (+4150 bytes)
- `KEYER` - CW keyer support
- `CW_DECODER` - Morse code decoder (+1468 bytes)
- `SEMI_QSK` - Receive during CW key-up
- `RIT_ENABLE` - Receive Incremental Tuning (+200 bytes)
- `VOX_ENABLE` - Voice-activated transmit
- `CW_MESSAGE` - Predefined CW messages

**Hardware Configuration:**
- `F_XTAL` - SI5351 crystal frequency (default: 27MHz)
- `SI5351_ADDR` - I2C address (0x60 or 0x62)
- `OLED_SSD1306` / `OLED_SH1106` - OLED display support
- `LCD_I2C` - I2C LCD support (PCF8574)
- `LPF_SWITCHING_*` - Multi-band filter switching via I2C GPIO extenders
- `BLUETOOTH_HC05` - Bluetooth module for CAT control

**AGC Parameters:**
- `DEFAULT_AGC_MODE` - AGC mode (0=OFF, 1=FAST, 2=MEDIUM, 3=SLOW)
- `AGC_ATTACK_THRESHOLD` - 1024 (optimized for fast attack)
- `AGC_DECAY_THRESHOLD` - 768 (gentler decay)

**TX Optimization:**
- `TX_ENABLE` - Disable for RX-only operation
- `KEY_CLICK` - Envelope shaping to reduce key clicks
- `TX_COMPRESSION_GAIN` - Compression gain (1-8)

### DSP Filters (usdx_filter.h)

Implements `filt_var()` function with 7 filter modes:

**SSB Filters (filt 1-3):**
- 300Hz high-pass + configurable low-pass
- Mode 1: 300-2900Hz (full bandwidth)
- Mode 2: 300-2400Hz (standard SSB)
- Mode 3: 300-1800Hz (narrower, elliptic response)

**CW Filters (filt 4-7):**
- Centered at 600Hz (selectable 400, 500, 600, 700, 800Hz)
- Mode 4: ±250Hz (500-1000Hz)
- Mode 5: ±100Hz (650-840Hz)
- Mode 6: ±50Hz (650-750Hz)
- Mode 7: ±18Hz (630-680Hz)

Filters use Direct Form I IIR biquad sections with bit-shift optimization.

## Critical Constraints

### Memory Budget
- **Flash:** 32KB total, current usage ~28940 bytes (89% full)
- **RAM:** 2KB total, current usage ~1417 bytes (69% full)
- **EEPROM:** 1KB for persistent settings

**Memory is critically constrained.** Always check flash/RAM usage after changes:
```bash
arduino-cli compile -b arduino:avr:uno --verbose 2>&1 | grep "bytes.*used"
```

### Performance Requirements
- **ADC Sampling:** 62kHz oversampled, decimated to ~7.8kHz
- **DSP Update Rate:** 4800 Hz for TX signal generation
- **I2C Speed:** 800 kbit/s for SI5351 control
- **Interrupt-driven:** Timer interrupts for audio processing

### Hardware Limitations
- **16MHz or 20MHz CPU clock** (configurable with fuses)
- **10-bit ADC** for audio input
- **PWM output** for PA envelope control
- **No floating-point unit** - use fixed-point arithmetic

## Development Guidelines

### Code Style
- **Memory-first mindset:** Prefer bit shifts over division/multiplication
- **Use PROGMEM:** Store constants in flash using `F()` macro or `PROGMEM` attribute
- **Fixed-width types:** Use `int8_t`, `uint8_t`, `int16_t`, `uint32_t` from `<stdint.h>`
- **Inline small functions:** Mark frequently-called DSP functions as `inline`
- **Avoid dynamic allocation:** No `malloc()` or `new`, use static buffers

### Optimization Patterns

**Prefer bit shifts:**
```cpp
// Bad
int result = value / 8;

// Good
int result = value >> 3;
```

**Use PROGMEM for constant data:**
```cpp
// Bad - wastes RAM
const char message[] = "Hello";

// Good - stores in flash
const char message[] PROGMEM = "Hello";
```

**Minimize stack usage:**
```cpp
// Bad - large stack allocation
void process() {
  int16_t buffer[256];
  // ...
}

// Good - static buffer (shared across calls)
void process() {
  static int16_t buffer[256];
  // ...
}
```

### Conditional Compilation
All hardware features use `#ifdef` guards. When modifying code:
- Maintain compatibility with all configuration combinations
- Test with features enabled/disabled
- Use `usdx_settings.h` for configuration, not inline `#define`

### Legacy Reference Code
The `usdx-legazy/` folder contains the **original firmware for reference only**.

**Rules:**
- **NEVER modify** files in `usdx-legazy/`
- Use it to understand original behavior when implementing features
- Compare implementations to ensure feature parity
- Reference algorithms and DSP techniques

### Serial Debug Output
When `CAT` or `DIAG` is enabled:
```cpp
#ifdef CAT
  Serial.println(F("Debug message"));
#endif
```

### Testing Procedure
1. **Compile:** Verify no errors/warnings
2. **Check memory:** Ensure flash/RAM within budget
3. **Upload:** Test on hardware
4. **Diagnostics:** Enable `DIAG` for startup checks
5. **Functional test:** Verify RX/TX operation

## Common Tasks

### Adding a New Menu Item
1. Add EEPROM parameter definition
2. Add menu item structure
3. Implement parameter get/set logic
4. Update menu navigation code
5. Test with rotary encoder and buttons

### Modifying DSP Filters
1. Edit `usdx_filter.h` → `filt_var()` function
2. Adjust IIR coefficients for desired frequency response
3. Use bit shifts instead of division for efficiency
4. Test with audio signals to verify behavior

### Adjusting AGC Behavior
1. Modify thresholds in `usdx_settings.h`:
   - `AGC_ATTACK_THRESHOLD` (default: 1024)
   - `AGC_DECAY_THRESHOLD` (default: 768)
2. Adjust decay rates: `AGC_FAST_DECAY`, `AGC_MEDIUM_DECAY`, `AGC_SLOW_DECAY`
3. Test with weak/strong signals

### Adding Hardware Support
1. Define pin mappings at top of `.ino` file
2. Add `#ifdef` guards for optional hardware
3. Add configuration switch to `usdx_settings.h`
4. Implement initialization in `setup()`
5. Update documentation

## Current Feature Status (v1.12)

### Enabled Features
- DIAG, KEYER, CAT, CW_DECODER, SEMI_QSK, RIT_ENABLE, CW_MESSAGE, CW_INTERMEDIATE
- LPF_SWITCHING_DL2MAN_USDX_REV3 (8-band filter switching)
- VOX with hysteresis (3-cycle hold)
- TX power ramping (32 steps, eliminates clicks)

### Default Settings
- Volume: 10 (range 0-16)
- S-meter mode: 2 (S-units)
- AGC mode: 2 (MEDIUM for SSB)

### Known Limitations
- No automated test suite (manual testing only)
- Flash memory near capacity (89% used)
- RAM constrained (69% used)
- Monolithic firmware (single large .ino file)

## References

- **Original uSDX:** https://github.com/threeme3/usdx
- **uSDX Forum:** https://groups.io/g/ucx
- **SI5351 Datasheet:** Silicon Labs AN619
- **ATMEGA328P Datasheet:** Microchip ATmega328P-DS40002061A

## Contact

**Author:** EA7LJY - Julian
**Email:** ea7ljy73@gmail.com
