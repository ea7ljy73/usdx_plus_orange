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
# CAT TS-480 por la UART real (38400 baudios, byte a byte): lo que haria un
# cliente/Hamlib. '#' = cualquier caracter; @var=valor comprueba la RAM.
TXPOS='IF##########################'  # 'IF' + 26: el siguiente es P8 (TX/RX)
CAT_SCRIPT="ID;=ID020;|FA;=FA00007074000;|IF;=IF00007074000*|AG0;=AG0192;|AG0255;=@volume=16|AG0;=AG0255;"
CAT_SCRIPT+="|AG0096;=@volume=6|SM0;=SM000##;|GT002;=@agc=2|GT;=GT002;|RA;=RA0000;|NR;=NR0;|RL04;=@nr=4|NR;=NR2;"
CAT_SCRIPT+="|PC;=PC050;|PC100;=@drive=8|MG;=MG100;|MG050;=@mic_atten=2|KS030;=@keyer_speed=30|KS;=KS030;|VX;=VX0;"
CAT_SCRIPT+="|FB;=FB00014074000;|FB00014200000;=|FB;=FB00014200000;|FR;=FR0;|FR1;=@vfosel=1|FA;=FA00014200000;|FT;=FT1;|FR0;=@vfosel=0|FA;=FA00007074000;|FB;=FB00014200000;" # conmutar no pisa el otro VFO
CAT_SCRIPT+="|EX0050000;=EX00500000;|EX00500001;=@vfosel=1|FA;=FA00014200000;|EX00500000;=@vfosel=0|FA;=FA00007074000;"
CAT_SCRIPT+="|RT;=RT0;|RU00500;=|IF;=IF0000707400000000+05001*|RD00200;=|IF;=IF0000707400000000+03001*|RC;=RC;|RT;=RT0;"
CAT_SCRIPT+="|EX0190000;=EX01900004;|EX0300000;=EX03000000;|EX03000002;=@comp_enable=2|PR;=PR1;|EX0340000;=EX03400000;"
CAT_SCRIPT+="|EX0250000;=EX025000027000000;|EX0400000;=?;|EX03000009;=?;|GTx;=?;|ZZ;=?;"
CAT_SCRIPT+="|MD3;=|MD;=MD3;|MD2;=|MD;=MD2;"
# TX por CAT: el estado lo da P8 del IF ('tx' es el latch de portadora y cae a 0
# con el micro en silencio); en TX lo que toca RF se rechaza, FA se anota (el
# SI5351 se programa al volver a RX, como legacy) y el volumen sigue funcionando
CAT_SCRIPT+="|TX;=|IF;=${TXPOS}1*|EX00100003;=?;|FA00007100000;=|IF;=IF00007100000*|AG0192;=@volume=12|RX;=RX0;|IF;=IF00007100000${TXPOS#IF###########}0*"
sim "PRODUCCION: CAT TS-480 por la UART (firmware real)" "$B"/prod/*.elf --syms "$B/prod.syms" --scenario cat --quiet --cat "$CAT_SCRIPT"
# sondeo continuo como flrig/Hamlib (FA/IF/SM/AG) con escrituras intercaladas, al
# ritmo minimo del cliente (120 ms) y a 50 ms: ni un byte perdido ni muestras de RX
POLL="FA;=FA00007074000;|IF;=IF00007074000*|SM0;=SM000##;|AG0;=AG0192;"
STRESS="$POLL|FA00007075000;=|FA;=FA00007075000;|FA00007074000;=|$POLL|$POLL|GT002;=|GT;=GT002;|GT001;=|$POLL|$POLL"
STRESS="$STRESS|$POLL|RU00100;=|IF;=IF0000707400000000+01001*|RC;=RC;|$POLL|$POLL"
sim "PRODUCCION: CAT, sondeo continuo cada 120 ms" "$B"/prod/*.elf --syms "$B/prod.syms" --scenario cat --quiet --cat-step 0.12 --cat "$STRESS"
sim "PRODUCCION: CAT, sondeo continuo cada 50 ms (mas rapido que el cliente)" "$B"/prod/*.elf --syms "$B/prod.syms" --scenario cat --quiet --cat-step 0.05 --cat "$STRESS"
# persistencia: lo cambiado por CAT sobrevive al reinicio (EEPROM real del simulador)
PERSIST="AG0080;=|GT002;=|EX03000002;=|PC025;=|EX02800003;=|FB00010120000;=|IF;=*|IF;=*|IF;=*|IF;=*|IF;=*|IF;=*"
sim "PRODUCCION: CAT, cambios y guardado en EEPROM" "$B"/prod/*.elf --syms "$B/prod.syms" --scenario cat --quiet --cat "$PERSIST" --ee-save "$B/ee_cat.bin"
sim "PRODUCCION: CAT, tras reiniciar con esa EEPROM" "$B"/prod/*.elf --syms "$B/prod.syms" --scenario cat --quiet --ee-load "$B/ee_cat.bin" \
    --cat "AG0;=AG0080;@volume=5|GT;=GT002;|EX0300000;=EX03000002;|PC;=PC025;|MG;=MG025;|FB;=FB00010120000;|FA;=FA00007074000;|VX;=VX0;"
echo "=================== PRODUCCION: mejoras de RX (AGC Start, AGC Hang, NR) medidas sobre el audio"
if python3 -c "import numpy" 2>/dev/null; then python3 rx_mejoras.py ./usdx_avr_sim "$B"/prod/*.elf "$B/prod.syms" || rc=1
else echo "   (omitido: falta numpy)"; fi
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
