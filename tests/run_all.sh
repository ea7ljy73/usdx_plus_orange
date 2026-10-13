#!/bin/bash
# run_all.sh — pasa TODOS los tests host del proyecto (sin hardware).
#   ./tests/run_all.sh            tests
#   ./tests/run_all.sh --build    además compila producción y lab (arduino-cli)
#   ./tests/run_all.sh --avr      además simula la radio completa (simavr, ~1 min)
# exit 0 = todo en verde. Resumen al final.
cd "$(dirname "$0")"
declare -a NAMES RES
step() { # nombre, comando...
  local n="$1"; shift
  local out; out=$("$@" 2>&1); local r=$?
  NAMES+=("$n"); RES+=($r)
  if [ $r = 0 ]; then echo "PASS  $n"; else echo "FAIL  $n"; echo "$out" | tail -25 | sed 's/^/      /'; fi
}
parity_tx() { cd parity_tx && python3 gen_parity_tx.py >/dev/null && gcc -O0 -o parity_tx main_test_tx.c tx_legacy.c tx_v2.c -lm \
  && ./parity_tx | tee /dev/stderr | grep -q "TX PARIDAD EXACTA"; }
parity_rx() { # todo a 0 salvo NR>=2 (divergencia intencional, puerta de ruido)
  cd parity_rx && python3 gen_parity_rx.py >/dev/null && gcc -O0 -o parity_rx main_test_rx.c rx_legacy.c rx_v2.c -lm || return 1
  ./parity_rx > rx_last.txt; cat rx_last.txt >&2
  local bad; bad=$(grep "mismatch=" rx_last.txt | grep -v "mismatch=0 " | grep -vc " nr=[2-8] ")
  local n0; n0=$(grep -c "mismatch=0 " rx_last.txt)
  [ "$bad" = 0 ] && [ "$n0" -ge 14 ]; }

step "paridad TX ssb() vs legacy"            parity_tx
step "paridad RX DSP vs legacy"              parity_rx
step "A/B AGC (asserts)"                     ./ab_agc/run_ab_agc.sh
step "menu_sim produccion"                   ./menu_sim/run_menu.sh prod
step "menu_sim lab"                          ./menu_sim/run_menu.sh lab
step "fw_sim (CAT, CW+paridad decoder, UI)"  ./fw_sim/run_fw_sim.sh
step "sincronia produccion <-> lab"          python3 check_sync.py
for a in "$@"; do [ "$a" = "--avr" ] && AVR=1; [ "$a" = "--build" ] && BUILD=1; done
[ -n "$AVR" ] && step "avr_sim (firmware real en ATmega328P+SI5351 simulados)" ./avr_sim/run_avr_sim.sh
if [ -n "$BUILD" ]; then
  B=$(mktemp -d)
  build() { arduino-cli compile --fqbn arduino:avr:uno --build-path "$B/$1" "$2" 2>&1 | tee /dev/stderr | grep -q "Sketch"; }
  step "compila produccion" build prod ..
  step "compila lab"        build lab ../usdx-lab
  rm -rf "$B"
fi
fail=0; for r in "${RES[@]}"; do [ "$r" = 0 ] || fail=$((fail+1)); done
echo; [ $fail = 0 ] && echo "TODO EN VERDE (${#RES[@]} bloques)" || echo "$fail de ${#RES[@]} bloques en ROJO"
exit $fail
