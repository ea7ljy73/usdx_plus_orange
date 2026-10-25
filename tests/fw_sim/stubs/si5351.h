// Stub si5351.h: graba las llamadas que cat.h hace al sintetizador.
#pragma once
#include <stdint.h>
void sim_ev(const char* s);
class SI5351 {
public:
  int     iqmsa = 1;
  volatile uint32_t fxtal = 27000000; // Ref freq (menu 25, P_T32)
  int32_t last_fast = 0;
  void freq_calc_fast(int32_t df) { last_fast = df; sim_ev("FAST;"); }
  void SendPLLRegisterBulk() { sim_ev("BULK;"); }
};
extern SI5351 si5351;
