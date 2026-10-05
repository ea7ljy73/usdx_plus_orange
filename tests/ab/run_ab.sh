#!/bin/bash
# run_ab.sh — Fase 0: genera TUs (ssb polar + cadena RX v2), compila y corre A/B.
set -e
cd "$(dirname "$0")"
python3 gen_ab.py
(cd ../parity_rx && python3 gen_parity_rx.py)
gcc -O0 -o ab main_ab.c ab_tx.c ../parity_rx/rx_v2.c -lm
./ab | tee ab_last.txt
