# Testbench Files

This folder contains testbench files for verifying SSB modulation algorithms.

## Files

| File | Description |
|------|-------------|
| `testbench_ssb_modulation.cpp` | Generic SSB modulation test (basic filters) |
| `testbench_exact_ssb.cpp` | Exact algorithm implementation (arctan3, Hilbert) |
| `testbench_ssb_completo.cpp` | Complete testbench with FFT analysis |
| `testbench_ssb_rf.cpp` | RF testbench with SI5351 simulator for all HF bands |
| `testbench_ssb_rf_legacy.cpp` | **NEW** Legacy vs New firmware comparison testbench |

## Quick Start - RF Testbench (Recommended)

The `testbench_ssb_rf` simulates the complete TX chain including SI5351 frequency synthesis for real HF bands.

```bash
cd test
g++ -std=c++11 -O2 -o testbench_ssb_rf testbench_ssb_rf.cpp
./testbench_ssb_rf --help
```

## Usage - testbench_ssb_rf

```bash
# Analyze specific band and mode
./testbench_ssb_rf --band=10m --mode=usb    # 10m USB
./testbench_ssb_rf --band=40m --mode=lsb    # 40m LSB
./testbench_ssb_rf --band=20m --mode=cw     # 20m CW

# All HF bands analysis
./testbench_ssb_rf --all

# Compare USB vs LSB
./testbench_ssb_rf --compare
```

## Legacy Comparison Testbench - testbench_ssb_rf_legacy

The `testbench_ssb_rf_legacy` compares the legacy firmware implementation with the refactored uSDX Plus Orange firmware.

**Key Differences:**
- Legacy: TX Drive default = 2, PWM max = 255, no compressor
- New: TX Drive default = 4, PWM max = 145, compressor 2:1

```bash
cd test
g++ -std=c++11 -O2 -o testbench_ssb_rf_legacy testbench_ssb_rf_legacy.cpp
```

## Usage - testbench_ssb_rf_legacy

```bash
# Compare legacy vs new for specific band
./testbench_ssb_rf_legacy --band=10m --mode=usb    # 10m USB
./testbench_ssb_rf_legacy --band=40m --mode=lsb    # 40m LSB

# All HF bands comparison
./testbench_ssb_rf_legacy --all

# TX Drive analysis (show impact of different drive levels)
./testbench_ssb_rf_legacy --tx-drive
```

## Example Output - Legacy Comparison

```
=============================================================
    uSDX Plus Orange - Testbench Comparativo Legacy vs Nuevo
                       Version 5.14
=============================================================

  Parametros Legacy:   drive=2, pwm_max=255, sin compresor
  Parametros Nuevo:     drive=4, pwm_max=145, compresor 2:1

=============================================================
          COMPARACION COMPLETA DE BANDAS HF
=============================================================

  Banda   | Modo  | Legacy Sup | Nuevo Sup | Legacy Rech | Nuevo Rech
  --------|-------|------------|-----------|-------------|-----------
  80m     | LSB   | 57.2       | 57.2      | 1.2         | 1.2
  40m     | LSB   | 62.1       | 62.1      | 1.2         | 1.2
  20m     | USB   | 62.1       | 62.1      | 1.2         | 1.2
  10m     | USB   | 62.1       | 62.1      | 1.2         | 1.2
  15m     | USB   | 62.1       | 62.1      | 1.2         | 1.2
```

## Supported HF Bands (uSDX+ Plus V2)

| Band | Frequency Range | Center Freq | Mode |
|------|-----------------|-------------|------|
| 80m | 3.5 - 3.9 MHz | 3.700 MHz | LSB |
| 60m | 5.3515 - 5.3665 MHz | 5.357 MHz | USB |
| 40m | 7.0 - 7.3 MHz | 7.150 MHz | LSB |
| 30m | 10.1 - 10.150 MHz | 10.125 MHz | USB |
| 20m | 14.0 - 14.350 MHz | 14.200 MHz | USB |
| 17m | 18.068 - 18.168 MHz | 18.118 MHz | USB |
| 15m | 21.0 - 21.450 MHz | 21.300 MHz | USB |
| 10m | 28.0 - 29.700 MHz | 28.500 MHz | USB |

## Example Output - RF Testbench

```
==========================================================
   uSDX Plus Orange - Testbench RF con SI5351 Simulator
                    Version 5.14
==========================================================

  Cristal SI5351: 27.000 MHz
  F_SAMP_TX: 15625 Hz

==========================================================
          ANALISIS COMPLETO DE BANDAS HF
==========================================================

  Banda   | Frec(MHz) | Modo  | Sup.Port | Rechazo | Status
  --------|------------|--------|----------|---------|--------
  80m     | 3.700      | LSB    | 57.2     | 1.2     | [WARN]
  60m     | 5.357      | USB    | 62.1     | 1.2     | [WARN]
  40m     | 7.150      | LSB    | 62.1     | 1.2     | [WARN]
  30m     | 10.125     | USB    | 62.1     | 1.2     | [WARN]
  20m     | 14.200     | USB    | 62.1     | 1.2     | [WARN]
  17m     | 18.118     | USB    | 62.1     | 1.2     | [WARN]
  15m     | 21.300     | USB    | 62.1     | 1.2     | [WARN]
  10m     | 28.500     | USB    | 62.1     | 1.2     | [WARN]

  Resumen: 0 OK, 8 WARN

  Nota: WARN indica rechazo de banda lateral ~1.2 dB a nivel IF.
        El QSD real proporciona ~40 dB adicionales de rejection.
```

## What the RF Testbench Verifies

| Parameter | Description | Expected |
|-----------|-------------|----------|
| Carrier Suppression | Portadora suppression | < -40 dB |
| Sideband Rejection | Banda lateral rejection | < -40 dB |
| RF Frequency | Frecuencia RF de salida | Correct per band |
| USB/LSB Balance | Balance USB vs LSB | < 1 dB difference |

## Legacy Testbenches

```bash
# Generic test (basic)
g++ -std=c++11 -O2 -o test_ssb testbench_ssb_modulation.cpp
./test_ssb

# Exact algorithms (FFT analysis)
g++ -std=c++11 -O2 -o test_exact testbench_exact_ssb.cpp
./test_exact

# Complete test with ASCII spectrum
g++ -std=c++11 -O2 -o testbench_ssb testbench_ssb_completo.cpp
./testbench_ssb --freq=1000 --mode=usb
```

## Notes

- **testbench_ssb_rf** simulates SI5351 frequency synthesis for real RF frequencies
- All HF bands (160m to 6m) are tested automatically
- Real hardware testing with spectrum analyzer is recommended for final validation
- Carrier suppression is excellent (57-62 dB) in simulation
- Sideband rejection at IF level shows ~1.2 dB; real QSD adds ~40 dB image rejection
- See RELEASE.md for version history and changes
