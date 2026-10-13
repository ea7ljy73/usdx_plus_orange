#!/bin/bash
# run_menu.sh [lab|prod] — simula en host el motor botones+menú REAL.
#   lab  (por defecto): usdx-lab/menu.h
#   prod              : menu.h de producción (raíz)
set -e
cd "$(dirname "$0")"
T="${1:-lab}"
if [ "$T" = prod ]; then
  cp ../../menu.h ./menu_under_test.h
  DEF=-DPROD
else
  cp ../../usdx-lab/menu.h ./menu_under_test.h
  DEF=
fi
g++ -O0 -std=gnu++17 $DEF -Istubs -o menu_sim main_menu_sim.cpp
./menu_sim | tee "menu_last_$T.txt"
exit "${PIPESTATUS[0]}"
