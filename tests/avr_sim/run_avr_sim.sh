#!/bin/bash
# run_avr_sim.sh — radio completa en simulador: el .elf REAL del firmware en un
# ATmega328P simulado (simavr) a 20 MHz + SI5351, reles LPF, LCD, micro y PTT.
# Mide la interrupcion de muestreo ciclo a ciclo y reconstruye la emision SSB.
#   ./run_avr_sim.sh           produccion: emision SSB (dos tonos, peor caso de CPU, EnvPre),
#                              recepcion, CW, y lab
#   ./run_avr_sim.sh --legacy  ademas pasa usdx-legazy (sin DIAG) como referencia
# simavr no necesita instalarse: si falta, se descarga el .deb y se extrae en
# ~/.cache/usdx-simavr (apt-get download, sin root).
cd "$(dirname "$0")"
SIMROOT="$HOME/.cache/usdx-simavr"
INC=/usr/include/simavr; LIB=
if [ ! -f $INC/sim_avr.h ]; then
  if [ ! -f "$SIMROOT/root/usr/include/simavr/sim_avr.h" ]; then
    echo "descargando simavr a $SIMROOT (sin root)..."
    mkdir -p "$SIMROOT" && (cd "$SIMROOT" && apt-get download libsimavr-dev libsimavr2 >/dev/null 2>&1 && for d in *.deb; do dpkg -x "$d" root; done)
  fi
  INC="$SIMROOT/root/usr/include/simavr"; LIB="$SIMROOT/root/usr/lib/x86_64-linux-gnu"
  [ -f "$INC/sim_avr.h" ] || { echo "AVR_SIM: no hay simavr (instala libsimavr-dev) -> OMITIDO"; exit 77; }
  export LD_LIBRARY_PATH="$LIB:$LD_LIBRARY_PATH"
  SO="$LIB/libsimavr.so.2"
else
  SO=-lsimavr
fi
g++ -O2 -std=gnu++17 -I"$INC" -o usdx_avr_sim usdx_avr_sim.cpp $SO -l:libelf.so.1 -lm || exit 1
NM=$(ls "$HOME"/.arduino15/packages/arduino/tools/avr-gcc/*/bin/avr-nm 2>/dev/null | head -1)
B=$(mktemp -d); trap 'rm -rf "$B"' EXIT
build() { arduino-cli compile --fqbn arduino:avr:uno --build-path "$B/$1" "$2" >"$B/$1.log" 2>&1 || { cat "$B/$1.log"; exit 1; }
          [ -n "$NM" ] && "$NM" -n -C "$B/$1"/*.elf > "$B/$1.syms"; }
rc=0
sim() { echo "=================== $1"; shift; ./usdx_avr_sim "$@" 2>&1 | grep -v "^Loaded"; [ "${PIPESTATUS[0]}" = 0 ] || rc=1; }

build prod ../..
sim "PRODUCCION: dos tonos 700+1100 Hz" "$B"/prod/*.elf --syms "$B/prod.syms"
sim "PRODUCCION: peor caso de CPU (habla fuerte + TX Comp 3 + low-cut)" "$B"/prod/*.elf --syms "$B/prod.syms" --speech --win 22 --tx 5 --mic 120 \
    --poke comp_enable=3 --poke tx_lowcut=2 --quiet
sim "PRODUCCION: preenfasis de envolvente (TX EnvPre=2) con el conformador RC del esquema" "$B"/prod/*.elf --syms "$B/prod.syms" \
    --poke tx_envpre=2 --expect-imd3 -28 --quiet
sim "PRODUCCION: recepcion (senal conocida en I/Q -> audio, medidor S, AGC, respuesta)" "$B"/prod/*.elf --syms "$B/prod.syms" --scenario rx --quiet
sim "PRODUCCION: CW (boton de modo real + llave)" "$B"/prod/*.elf --syms "$B/prod.syms" --scenario cw --quiet
build lab ../../usdx-lab
sim "LAB: dos tonos 700+1100 Hz" "$B"/lab/*.elf --syms "$B/lab.syms" --quiet
if [ "$1" = "--legacy" ]; then # referencia: legacy con DIAG desactivado (sus chequeos de placa no aplican al simulador)
  mkdir -p "$B/src/usdx-legazy" && cp ../../usdx-legazy/* "$B/src/usdx-legazy/"
  sed -i 's|^#define DIAG |//#define DIAG |' "$B/src/usdx-legazy/usdx_settings.h"
  build leg "$B/src/usdx-legazy"
  echo "=================== LEGACY (referencia, no cuenta para el resultado)"
  # estado del hardware tras el arranque: registros AVR, variables de RX/TX, SI5351 y reles deben coincidir con legacy
  ./usdx_avr_sim "$B"/prod/*.elf --syms "$B/prod.syms" --quiet --tx 0.05 --dump-state 2>&1 | grep -E "^REG|^VAR|^SI5351|^RELES" > "$B/st_prod.txt"
  ./usdx_avr_sim "$B"/leg/*.elf --syms "$B/leg.syms" --quiet --tx 0.05 --dump-state 2>&1 | grep -E "^REG|^VAR|^SI5351|^RELES" > "$B/st_leg.txt"
  echo "=================== ESTADO TRAS EL ARRANQUE: produccion vs legacy ($(wc -l < "$B/st_prod.txt") registros y variables)"
  # diferencias admitidas: ganancia AGC instantanea, variables que solo existen en uno
  D=$(diff <(grep -vE "^VAR16 gain |^VAR txdelay |^VAR menumode |^SI5351" "$B/st_prod.txt") <(grep -vE "^VAR16 gain |^VAR txdelay |^VAR menumode |^SI5351" "$B/st_leg.txt"))
  S1=$(grep "^SI5351" "$B/st_prod.txt"); S2=$(grep "^SI5351" "$B/st_leg.txt")
  if [ -z "$D" ] && [ "$S1" = "$S2" ]; then echo "   identico a legacy"; else echo "$D"; [ "$S1" = "$S2" ] || echo "   SI5351 distinto"; echo "   ESTADO DISTINTO DE LEGACY"; rc=1; fi
  for sc in ssb rx cw; do ./usdx_avr_sim "$B"/leg/*.elf --syms "$B/leg.syms" --scenario $sc --quiet 2>&1 | grep -v "^Loaded"; done
fi
[ $rc = 0 ] && echo "AVR_SIM: PASS" || echo "AVR_SIM: FAIL"
exit $rc
