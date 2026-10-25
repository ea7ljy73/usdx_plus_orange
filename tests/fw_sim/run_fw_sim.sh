#!/bin/bash
# run_fw_sim.sh — simulaciones host del firmware REAL (sin hardware):
#   cat_sim : cat.h de producción (CAT TS-480, FA/MD nunca en TX, RC;,
#             comandos ampliados y EX contra la MENU[] real del .ino)
#   cw_sim  : cw.h (keyer iámbico, mensajes CW, decoder) + PARIDAD paso a
#             paso del decoder contra usdx-legazy (gen_cw_legacy.py)
#   ui_sim  : display_tick()/on_rit() extraídas del .ino (prod y lab) +
#             invariantes de setup()/loop() leídos del fuente
#   si5351_sim : si5351.h contra la clase SI5351 de legacy sobre un modelo
#             de registros del chip (sintonía, calc_fast, TX, fallo de bus)
#   rxtx_sim: switch_rxtx() de hw.h contra la de legacy (secuencia de
#             hardware y estado) + invariantes de seguridad RF
#   txamp_sim : ssb() de tx.h contra la de legacy comparando fase Y
#             amplitud (tx=1, MAX_DP) + medidas del ALC de pico
#   txtiming_sim : espectro RF de TX con la temporización real del I2C
# exit 0 = todo PASS.
cd "$(dirname "$0")"
CXX="g++ -O0 -std=gnu++17 -Istubs"
rc=0
run() { echo "== $1 =="; shift; "$@" || rc=1; }

cp ../../cat.h cat_under_test.h
python3 gen_cat_menu.py || exit 1
$CXX -o cat_sim cat_sim.cpp || exit 1
run "cat_sim (cat.h produccion)" ./cat_sim

python3 gen_cw_legacy.py || exit 1
for t in prod lab; do
  [ $t = prod ] && cp ../../cw.h cw_under_test.h || cp ../../usdx-lab/cw.h cw_under_test.h
  $CXX -o cw_sim cw_sim.cpp || exit 1
  run "cw_sim (cw.h $t)" ./cw_sim
done

for t in prod lab; do
  echo "== ui_sim ($t) =="
  python3 gen_ui.py $t || rc=1
  $CXX -o ui_sim ui_sim.cpp || exit 1
  ./ui_sim || rc=1
done

python3 gen_si5351_legacy.py || exit 1
for t in prod lab; do
  [ $t = prod ] && cp ../../si5351.h si5351_under_test.h || cp ../../usdx-lab/si5351.h si5351_under_test.h
  $CXX -o si5351_sim si5351_sim.cpp || exit 1
  run "si5351_sim (si5351.h $t vs legacy)" ./si5351_sim
done

for t in prod lab; do
  python3 gen_rxtx.py $t || exit 1
  g++ -O0 -std=gnu++17 -o rxtx_sim rxtx_sim.cpp || exit 1
  run "rxtx_sim (switch_rxtx $t vs legacy)" ./rxtx_sim
done

for t in prod lab; do
  python3 gen_txamp.py $t >/dev/null || exit 1
  gcc -O1 -c txamp_legacy.c txamp_new.c || exit 1
  g++ -O1 -std=gnu++17 -o txamp_sim txamp_sim.cpp txamp_legacy.o txamp_new.o -lm || exit 1
  run "txamp_sim (envolvente TX + ALC, tx.h $t vs legacy)" ./txamp_sim
done

# espectro de TX con la temporizacion real del I2C (usa ssb() real via tests/ab)
cp ../../si5351.h si5351_under_test.h
(cd ../ab && python3 gen_ab.py >/dev/null) && gcc -O2 -c -o ab_tx.o ../ab/ab_tx.c || exit 1
g++ -O2 -std=gnu++17 -Istubs -o txtiming_sim txtiming_sim.cpp ab_tx.o || exit 1
run "txtiming_sim (espectro TX, I2C real)" ./txtiming_sim

[ $rc = 0 ] && echo "FW_SIM: PASS" || echo "FW_SIM: FAIL"
exit $rc
