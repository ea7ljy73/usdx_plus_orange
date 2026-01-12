# uSDX Plus Orange - Release Notes

**Version:** 1.10x
**Base:** uSDX Legacy 1.02x
**Platform:** ATMEGA328P @ 20MHz
**Author:** EA7LJY - Julian (modifications)

---

## 1. SSB Signal Processor Improvements

### 1.1 Optimized Hilbert Transform
- Use of `memmove()` instead of manual loops for shift register
- Removed shift operations on potentially negative values
- New safe implementation
- Buffer reduced from 16 to 15 elements to save RAM

### 1.2 Soft Limiter with Compression
- Replaced hard clipping with soft compression
- Threshold reduced from 250 to 200 for better intelligibility
- 4:1 compression ratio above threshold
- Prevents saturation while maintaining audio quality

### 1.3 Anti-Saturation Compensation for High Drive
```c
if(drive > 3 && _amp > 200){
  _amp = 200 + (_amp - 200) * (6 - drive) / (6 - 3);
}
```
- Progressively reduces gain at high drive levels
- Prevents distortion from transmitter saturation

---

## 2. CW (Morse Code) Improvements

### 2.1 Expanded Sidetone Frequencies
- 5 selectable frequencies: 400, 500, 600, 700, 800 Hz

### 2.2 Sidetone Volume Control
- Adjustable volume at runtime
- 16 volume levels available

### 2.3 CW Messages in PROGMEM
- CW messages stored in flash instead of RAM

---

## 3. CW Decoder Improvements

### 3.1 New Modern Decoder (NEW_CW)
- Optimized decoder from OZ1JHM
- Removed duplicate OLD_CW code
- Savings of 106 bytes

### 3.2 Encoder Direction Tracking
- uSDXOpen implementation: encoder direction tracking
- Allows smooth bidirectional band change

---

## 4. Improved Automatic Gain Control (AGC)

### 4.1 New Configurable Thresholds
```c
#define AGC_ATTACK_THRESHOLD 1280  // Lower threshold for faster attack
#define AGC_DECAY_THRESHOLD 1024   // Threshold for decay
```

### 4.2 Multi-Level Response
- Very fast attack for strong signals
- Medium response for smooth decay

### 4.3 Minimum Gain Protection
- Prevents gain from dropping below safe threshold

---

## 5. Noise Reduction

### 5.1 Increased Default Level
- Default NR level = 2

### 5.2 Adaptive Spectral Noise Gate
- Adaptive temporal expander
- Threshold proportional to selected NR level

---

## 6. Adaptive Noise Blanker

- Noise peak detection based on adaptive floor
- 4-sample suppression with linear interpolation
- Adaptive threshold based on fast/slow average difference

---

## 7. Notch Filter (Optional)

- IIR biquad notch filter
- Removes specific frequency interference (50/60Hz hum)
- Requires ~600 bytes of Flash when enabled
- Disabled by default to save memory

---

## 8. Arctan3 Lookup Table

- 32 pre-calculated values for z = 0 to 31/32
- Significantly accelerates `_arctan3()`
- CPU cycles savings in demodulation processing

---

## 9. S-Meter with Peak Hold

- `P` - Peak just reached new maximum
- `p` - Peak decaying
- ` ` (space) - No peak activity
- Peak hold visible for ~30 updates

---

## 10. Additional TX Improvements

### 10.1 Speech Pre-emphasis (EQ)
- High-pass filter that enhances voice high frequencies
- +4dB global gain
- Improves intelligibility and clarity of transmitted voice

### 10.2 Speech Compressor
- Automatic 4:1 dynamic range compression
- Fast attack, slow decay for natural response
- Minimum guaranteed gain (floor of 64/256)

### 10.3 TX Power Ramping
- 32 soft ramp steps at TX start/end
- S-curve for natural transitions
- Eliminates "pops" and clicks in TX/RX switching

### 10.4 VOX with Hysteresis
- Prevents VOX oscillations
- 3 hold cycles after dropping below threshold
- More stable RX/TX transition

---

## 11. TX-01: SSB Voice Quality Improvements (uSDXOpen)

### 11.1 Phase Unwrapping Correction
- Prevents "quadrature flipping" that causes distortion
- Automatically corrects large phase jumps

### 11.2 Phase Smoothing Filter
- Reduces phase fluctuations
- Smooths transmitted audio signal
- Improves voice clarity

### 11.3 Adaptive Q Correction
- Smooths I/Q quadrant transition
- Improves unwanted sideband suppression
- Reduces distortion during rapid frequency changes

---

## 12. Memory Optimizations

### 12.1 PROGMEM Usage for Constants
- LCD font table in PROGMEM
- CW messages in PROGMEM
- Arctan LUT in PROGMEM
- Ramp curve in PROGMEM

### 12.2 Buffer Size Reduction
- SSB shift register: 16 → 15 elements
- Q processing: 14 → 13 elements
- Use of `memmove()` for shift registers

### 12.3 Optimized Calculations
- Division by powers of 2 replaced with shifts
- Removed redundant operations

---

## Change Summary

| Category | Changes |
|----------|---------|
| SSB Processing | Hilbert optimized, soft limiter, anti-saturation |
| CW | Optimized decoder (-106 bytes), 5 freq sidetone, volume |
| AGC | Multi-threshold, gain protection |
| NR | Adaptive noise gate |
| Noise Blanker | New, adaptive |
| TX Optimizations | Speech EQ, Compressor, Power Ramping, VOX Hysteresis |
| TX-01 SSB Quality | Phase unwrapping, smoothing, Q correction |
| Memory | Extensive PROGMEM, reduced buffers, optimized decoder |

---

## Compilation

To compile:
- **Arduino IDE:** Board "Arduino Uno", Clock "20MHz"
- **AVR-GCC:** `-DF_MCU=20000000 -DF_CPU=20007000`

### Memory Usage (full configuration)
| Resource | Used | Available |
|----------|------|-----------|
| Flash | 32026 bytes (99%) | 32256 bytes |
| RAM | 1482 bytes (72%) | 2048 bytes |

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
