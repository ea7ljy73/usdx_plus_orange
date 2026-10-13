#!/bin/bash
# run_ab_agc.sh — test A/B + regresion de F4.16 (AGC Start) y F4.17 (AGC Rec).
# Usa el cuerpo REAL de process_agc_fast extraido de rx.h (via rx_v2.c).
# Sale 0 si pasa, 1 si falla.
set -e
cd "$(dirname "$0")"
(cd ../parity_rx && python3 gen_parity_rx.py)
gcc -O0 -o ab_agc main_ab_agc.c ../parity_rx/rx_v2.c -lm
./ab_agc | tee ab_agc_last.txt
