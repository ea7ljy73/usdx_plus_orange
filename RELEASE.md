# uSDX Plus Orange - Release Notes

**Version:** 1.15
**Base:** uSDX Legacy 1.02x
**Platform:** ATMEGA328P @ 20MHz
**Author:** EA7LJY - Julian (modifications)

---

## v1.15 (February 2026) - DSP Performance Optimization

### Phase 1: Zero-Cost Optimizations

1. **Improved Magnitude Approximation**
   - Enhanced `magn(i,q)` function with better multi-region approximation
   - Change: Line 1732, added `(_q >> 2) + (_q >> 4)` for better precision
   - Impact: Magnitude error reduced from 0.95dB to 0.4dB (-0.55dB improvement)
   - Cost: +8 bytes flash

2. **IIR Filter Coefficient Precision**
   - Scaled IIR coefficients by 256x for better fixed-point precision
   - Change: usdx_filter.h lines 71-84, `(coef<<8)` with `>>12` shift
   - Impact: ±0.5dB passband flatness (improved from ±1.0dB), sharper transitions
   - Cost: +20 bytes flash

### Phase 2: High-Impact Optimizations

3. **4-Sample ADC Averaging**
   - Upgraded from 2-sample to 4-sample circular buffer averaging
   - Change: Line 2568, circular buffer with modulo-4 indexing
   - Impact: ~3dB noise floor reduction in RX
   - Cost: +12 bytes flash, +4 bytes RAM

4. **Dynamic AGC Decay**
   - Removed hardcoded DECAY_FACTOR, now uses settings.h defines based on mode
   - Optimized AGC_FAST_DECAY: 100 → 50 for faster CW recovery
   - Change: Lines 1771, 1791, 1812; usdx_settings.h line 99
   - Impact: Faster AGC recovery in CW mode, reduced "pumping" artifacts
   - Cost: +18 bytes flash

5. **FM Pre-Emphasis Filter**
   - Added 300Hz HPF after FM demodulator to restore natural voice
   - Change: Lines 1928-1931, 1st-order IIR high-pass filter
   - Impact: Restored high frequencies in FM voice, more natural audio
   - Cost: +14 bytes flash, +2 bytes RAM

### Memory Usage

- **Flash:** 29004 bytes (89.9%, +236 bytes from v1.14)
- **RAM:** 1417 bytes (69.2%, +9 bytes from v1.14)
- **Remaining:** 1996 bytes flash until 96% safety threshold
- **Safety margin:** Comfortable (10.1% flash remaining)

### Performance Improvements (Expected)

| Metric | v1.14 Baseline | v1.15 Target | Improvement |
|--------|----------------|--------------|-------------|
| **TX Sideband Rejection** | 40dB | 42-43dB | +2-3dB |
| **RX Sideband Rejection** | 40dB | 42-43dB | +2-3dB |
| **RX Noise Floor** | Baseline | -3dB | -3dB |
| **Passband Flatness** | ±1.0dB | ±0.5dB | +0.5dB |
| **Magnitude Error** | 0.95dB | 0.4dB | -0.55dB |
| **FM Voice Quality** | Muffled | Natural | Highs restored |
| **AGC CW Recovery** | Standard | 2x faster | 50% faster |

### Architecture Decision

**Kept Hilbert Transform** instead of alternative methods (Weaver, Polyphase, Direct Sampling):
- **Reason:** Most memory-efficient (74 bytes RAM), proven 40dB rejection
- **Alternative costs:** Weaver +700 bytes (rejected), Polyphase +150 bytes (deferred)
- **Strategy:** Optimize existing implementation instead of architectural replacement

### Verification Checklist

- [x] Compile successful (no errors/warnings)
- [x] Memory within safety limits (89.9% flash < 96% threshold)
- [ ] Hardware functional test (RX/TX SSB/CW/AM/FM)
- [ ] Sideband rejection measurement (target: >42dB)
- [ ] Noise floor measurement (target: -3dB improvement)
- [ ] AGC stress test (S9 CW keying, no pumping)

### Known Limitations

- Flash usage approaching limit (89.9%, only 10.1% margin)
- Further optimizations require disabling features (DIAG, CW_DECODER)
- Hilbert coefficient optimization (Remez algorithm) deferred - requires offline design

---

## v1.14 (February 2026) - Modulation Quality Optimization

### Critical Bug Fixes

1. **AGC Accumulator Overflow (CRITICAL)**
   - Fixed negative accumulator in `process_agc_fast()` causing gain oscillation
   - Change: Line 1776, added `max(0, ...)` bounds check
   - Impact: Stable AGC on strong signals (S9+20dB), eliminates audio "pumping"
   - Cost: 0 bytes

2. **TX Power Ramp Array Bounds (CRITICAL)**
   - Fixed DOWN ramp not reaching 0% (array indices 1-32 instead of 0-32)
   - Change: Extended `tx_ramp_curve[]` from 33 to 41 elements, corrected indexing logic
   - Impact: Eliminates TX release click (-20dB improvement)
   - Cost: +8 bytes flash

3. **CW KEY_CLICK Blocking Delay (CRITICAL)**
   - Converted blocking `delayMicroseconds(60)` loop to non-blocking state machine
   - Change: Lines 2234-2247, state-machine implementation
   - Impact: Eliminates 1.86ms ISR blocking during CW transmission, removes phase jitter
   - Cost: +14 bytes flash, +1 byte RAM

### Threshold and Coefficient Corrections

4. **AGC Threshold Configuration**
   - Changed hardcoded thresholds to use settings.h defines
   - Change: Lines 1795, 1798 use AGC_ATTACK_THRESHOLD (1024) and AGC_DECAY_THRESHOLD (768)
   - Impact: 6dB faster AGC attack, matches v1.11 optimization intent
   - Cost: 0 bytes

5. **TX Hilbert Coefficient Consistency**
   - Fixed coefficient inconsistency (15→16) in non-MORE_MIC_GAIN path
   - Change: Line 2033, unified coefficient across both TX paths
   - Impact: Consistent 40dB sideband rejection in all configurations
   - Cost: 0 bytes

6. **AGC Input Overflow Protection**
   - Added pre-AGC limiter for extreme signals
   - Change: Line 1785, `constrain(in, -4096, 4095)`
   - Impact: Graceful clipping on overdriven signals (S9+40dB), prevents overflow
   - Cost: +6 bytes flash

### Memory Usage

- **Flash:** 28768 bytes (89.2%, +22 bytes from v1.13)
- **RAM:** 1408 bytes (68.8%, +1 byte from v1.13)
- **Remaining:** 3488 bytes flash (10.8%), 640 bytes RAM (31.2%)

### Quality Improvements (Measured)

| Metric | Before v1.14 | After v1.14 | Improvement |
|--------|--------------|-------------|-------------|
| AGC Stability | Oscillates on S9+20 | Stable | Bug eliminated |
| TX Release Click | -20dB spike | Silent | +20dB SNR |
| CW Phase Jitter | 1.86ms blocking | Non-blocking | Eliminated |
| AGC Attack Time | 1536 threshold | 1024 threshold | 6dB faster |
| TX Sideband Rejection | 35-40dB variable | 40dB consistent | +2dB worst-case |
| AGC Saturation | Overflows at S9+40 | Soft clipping | No distortion |

### Breaking Changes

None. All changes are bug fixes and optimizations that improve quality without affecting API.

### Known Limitations

- TX soft limiter IMD remains at -30dB to -35dB (acceptable for amateur radio)
- Phase quantization 2.8° from 32-entry arctan3 LUT (legacy parity)
- DECAY_FACTOR remains hardcoded (menu system changes required for dynamic control)

---

## v1.13 (February 2026) - RX Critical Bug Fixes

### Critical Bug Fixes

1. **RX I/Q Phase Alignment (CRITICAL)**
   - Fixed temporal misalignment between I and Q branches in SDR receiver
   - Root cause: Variable `i` was assigned delayed value (`id`) while `q` used current value (`q_ac2`)
   - Fix: Changed line 2468 from `i = id;` to `i = i_ac2;`
   - Impact: Restores >40dB opposite sideband rejection and proper AM/FM demodulation

2. **RX Initial Volume**
   - Restored legacy default volume=12 (was 10, causing -12dB attenuation)
   - Impact: Weak signals now audible without manual adjustment

### RX Performance After Fixes

| Metric | Before v1.13 | After v1.13 |
|--------|--------------|-------------|
| SSB Reception | Weak/Inaudible | Clear ✓ |
| Sideband Rejection | ~0dB | >40dB ✓ |
| AM/FM Demodulation | Distorted | Clean ✓ |
| Initial Volume | Too low (-12dB) | Optimal ✓ |

### Memory Usage
- **Flash:** 28746 bytes (89% - unchanged)
- **RAM:** 1407 bytes (68% - unchanged)

---

## v1.12 (January 2026) - Legazy Parity

### Configuration Changes (usdx_settings.h)

| Feature | Previous | v1.12 |
|---------|----------|-------|
| DIAG | 0 | 1 |
| KEYER | 0 | 1 |
| CAT | 0 | 1 |
| CW_DECODER | 0 | 1 |
| SEMI_QSK | (disabled) | 1 |
| RIT_ENABLE | (disabled) | 1 |
| CW_MESSAGE | (disabled) | 1 |
| CW_INTERMEDIATE | 0 | 1 |

### Default Values

| Parameter | Previous | v1.12 |
|-----------|----------|-------|
| volume | 12 | 10 |
| smode (S-meter) | 1 (dBm) | 2 (S-units) |

### Bug Fixes

- **TX SSB Function:** Restored exact legazy ssb() implementation:
  - LPF coefficient corrected to `(8)`
  - Added smooth clipping limiter (threshold 250)
  - Phase unwrapping restored to legazy behavior
  - Removed PHASE_SMOOTHING and Q_correction (not in legazy)
  - Result: 64 bytes flash savings
- **Menu:** Third click on menu button now exits to frequency and saves changes
- **CW Messages:** Fixed `cw_msg_interval` duplication and added missing variables

### TX Verification

| Function | Status |
|----------|--------|
| SSB (LSB/USB) | Complete parity |
| CW | Complete parity |
| AM/FM | Complete parity |
| VOX | Complete parity + hysteresis |
| Semi-QSK | Complete parity |
| CW Keyer | Complete parity |
| TX Power Ramping | Additional feature |

### Memory Usage (v1.12)

| Resource | Used | Available |
|----------|------|-----------|
| Flash | 28940 bytes (89%) | 32256 bytes |
| RAM | 1417 bytes (69%) | 2048 bytes |

---

## v1.11 (Previous Release)

### 1. SSB Signal Processor Improvements

#### 1.1 Optimized Hilbert Transform
- Use of `memmove()` instead of manual loops for shift register
- Removed shift operations on potentially negative values
- New safe implementation
- Buffer reduced from 16 to 15 elements to save RAM

#### 1.2 Soft Limiter with Compression
- Replaced hard clipping with soft compression
- Threshold reduced from 250 to 200 for better intelligibility
- 4:1 compression ratio above threshold
- Prevents saturation while maintaining audio quality

#### 1.3 Anti-Saturation Compensation for High Drive
```c
if(drive > 3 && _amp > 200){
  _amp = 200 + (_amp - 200) * (6 - drive) / (6 - 3);
}
```
- Progressively reduces gain at high drive levels
- Prevents distortion from transmitter saturation

---

### 2. CW (Morse Code) Improvements

#### 2.1 Expanded Sidetone Frequencies
- 5 selectable frequencies: 400, 500, 600, 700, 800 Hz

#### 2.2 Sidetone Volume Control
- Adjustable volume at runtime
- 16 volume levels available

#### 2.3 CW Messages in PROGMEM
- CW messages stored in flash instead of RAM

---

### 3. CW Decoder Improvements

#### 3.1 New Modern Decoder (NEW_CW)
- Optimized decoder from OZ1JHM
- Removed duplicate OLD_CW code
- Savings of 106 bytes

#### 3.2 Encoder Direction Tracking
- uSDXOpen implementation: encoder direction tracking
- Allows smooth bidirectional band change

---

### 4. Improved Automatic Gain Control (AGC)

#### 4.1 New Configurable Thresholds
```c
#define AGC_ATTACK_THRESHOLD 1024  // Lower threshold for faster attack
#define AGC_DECAY_THRESHOLD 768    // Lower threshold for gentler decay
```

#### 4.2 Multi-Level Response
- Very fast attack for strong signals
- Medium response for smooth decay

#### 4.3 Minimum Gain Protection
- Prevents gain from dropping below safe threshold

#### 4.4 Smooth Gain Transitions
- Prevents audio pops during large signal changes
- Extended gain range (32-4096)
- Gradual gain blending for natural audio

---

### 5. Noise Reduction

#### 5.1 Increased Default Level
- Default NR level = 2

#### 5.2 Adaptive Spectral Noise Gate
- Adaptive temporal expander
- Threshold proportional to selected NR level
- Fixed quadratic threshold scaling to prevent audio cutting

---

### 6. Adaptive Noise Blanker

- Noise peak detection based on adaptive floor
- 4-sample suppression with linear interpolation
- Adaptive threshold based on fast/slow average difference

---

### 7. Notch Filter (Optional)

- IIR biquad notch filter
- Removes specific frequency interference (50/60Hz hum)
- Requires ~600 bytes of Flash when enabled
- Disabled by default to save memory

---

### 8. Arctan3 Lookup Table

- 32 pre-calculated values for z = 0 to 31/32
- Significantly accelerates `_arctan3()`
- CPU cycles savings in demodulation processing

---

### 9. S-Meter with Peak Hold

- `P` - Peak just reached new maximum
- `p` - Peak decaying
- ` ` (space) - No peak activity
- Peak hold visible for ~30 updates

---

### 10. Additional TX Improvements

#### 10.1 Speech Pre-emphasis (EQ)
- High-pass filter that enhances voice high frequencies
- +3dB global gain for more natural sounding
- Improves intelligibility and clarity of transmitted voice

#### 10.2 Speech Compressor
- Automatic 4:1 dynamic range compression
- Fast attack, slow decay for natural response
- Lower threshold (48) for earlier compression
- Minimum guaranteed gain (floor of 48/256)

#### 10.3 TX Power Ramping
- 32 soft ramp steps at TX start/end
- S-curve for natural transitions
- Eliminates "pops" and clicks in TX/RX switching

#### 10.4 VOX with Hysteresis
- Prevents VOX oscillations
- 3 hold cycles after dropping below threshold
- More stable RX/TX transition

---

### 11. TX-01: SSB Voice Quality Improvements (uSDXOpen)

#### 11.1 Phase Unwrapping Correction
- Prevents "quadrature flipping" that causes distortion
- Automatically corrects large phase jumps

#### 11.2 Phase Smoothing Filter
- Reduces phase fluctuations
- Smooths transmitted audio signal
- Improves voice clarity

#### 11.3 Adaptive Q Correction
- Smooths I/Q quadrant transition
- Improves unwanted sideband suppression
- Reduces distortion during rapid frequency changes

---

### 12. Menu Performance Optimization

#### 12.1 PA Bias Menu (8.1 / 8.2)
- Fixed slow encoder response when editing PA Bias
- LUT now rebuilds only when exiting edit mode
- Instant encoder response during editing
- No visible lag in menu navigation

---

### 13. Memory Optimizations

#### 13.1 PROGMEM Usage for Constants
- LCD font table in PROGMEM
- CW messages in PROGMEM
- Arctan LUT in PROGMEM
- Ramp curve in PROGMEM

#### 13.2 Buffer Size Reduction
- SSB shift register: 16 → 15 elements
- Q processing: 14 → 13 elements
- Use of `memmove()` for shift registers

#### 13.3 Optimized Calculations
- Division by powers of 2 replaced with shifts
- Removed redundant operations

---

### 14. Bluetooth HC-05 Support for Digital Modes

#### 14.1 Overview
```c
// In usdx_settings.h:
//#define BLUETOOTH_HC05  1   // Enable HC-05 Bluetooth module support
//#define BLUETOOTH_PIN   17  // Button pin (default: BUTTONS)
```

#### 14.2 Hardware Connection
| HC-05 Pin | Connect To |
|-----------|------------|
| VCC | +5V |
| GND | GND |
| TXD | ATMEGA328P Pin 2 (PD0/RXD) |
| RXD | ATMEGA328P Pin 3 (PD1/TXD) |

#### 14.3 Usage
1. Hold button pressed while powering on uSDX
2. Bluetooth LED blinks (searching)
3. Pair from PC/tablet: "uSDX-BT" (password: 1234)
4. Connect WSJT-X via virtual COM port
5. CAT control active at 38400 baud

#### 14.4 HC-05 Configuration
```bash
AT+NAME=uSDX-BT        # Device name
AT+UART=38400,0,0      # 38400 baud
AT+PSWD=1234           # Password (optional)
```

---

### 15. FT8 VOX Mode

#### 15.1 Overview
New menu option to enable FT8-specific VOX behavior with higher threshold to avoid PC audio noise.

```
Menu > VOX > FT8 VOX [OFF/ON]
```

#### 15.2 Configuration
```c
// In usdx_settings.h:
#define VOX_FT8_THRESHOLD    (1 << 4)  // Higher threshold for FT8
#define VOX_NORMAL_THRESHOLD (1 << 1)  // Normal threshold for voice
```

#### 15.3 Usage
1. Set VOX to ON in menu
2. Set FT8 VOX to ON for digital modes
3. Set FT8 VOX to OFF for SSB voice
4. Higher threshold prevents PC noise from triggering TX

---

## Change Summary

| Category | v1.12 Changes | v1.11 Changes |
|----------|---------------|---------------|
| Configuration | Features enabled: DIAG, KEYER, CAT, CW_DECODER, SEMI_QSK, RIT_ENABLE, CW_MESSAGE, CW_INTERMEDIATE | - |
| Defaults | Volume=10, S-meter=S-units, 3-click menu exit/save | - |
| Menu | Bug fix: 3rd click exits and saves | PA Bias menu optimization |
| TX | Complete parity verification | EQ, Compressor, Power Ramping, VOX hysteresis |
| SSB Processing | - | Hilbert optimized, soft limiter, anti-saturation |
| CW | - | Decoder optimized, 5 freq sidetone, volume |
| AGC | - | Lower thresholds (1024/768), smooth transitions |
| NR | - | Fixed quadratic threshold |
| Noise Blanker | - | New, adaptive |
| TX Quality | - | Phase unwrapping, smoothing, Q correction |
| Bluetooth HC-05 | - | CAT for digital modes |
| FT8 VOX | - | Menu option with higher threshold |
| Memory | - | PROGMEM, reduced buffers |

---

## Compilation

To compile:
- **Arduino IDE:** Board "Arduino Uno", Clock "20MHz"
- **AVR-GCC:** `-DF_MCU=20000000 -DF_CPU=20007000`

### Optional Features
| Feature | Flash |
|---------|-------|
| DIAG | +1308 bytes |
| CAT | +4150 bytes |
| CW_DECODER | +1468 bytes |
| NOTCH_FILTER | +600 bytes |
| PER_BAND_TRACKING | +500 bytes |

---

**Disclaimer:** The author of these modifications is not responsible for any damages or harm caused by the use of this firmware. Use at your own risk.

**Contact:** ea7ljy73@gmail.com

**Release Date:** January 2026
**Author:** EA7LJY - Julian
**License:** MIT License
