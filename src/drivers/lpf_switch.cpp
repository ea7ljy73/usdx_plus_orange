/**
 * @file lpf_switch.cpp
 * @brief Implementación del Low-Pass Filter Switch
 */

#include "lpf_switch.h"

namespace lpf {

// Tabla de frecuencias de corte (MHz)
const uint8_t lpf_frequencies[NUM_LPF] = LPF_FREQUENCIES;

void init() {
#ifdef LPF_SWITCHING_DL2MAN_USDX_REV3
  pinMode(LPF_BIT0, OUTPUT);
  pinMode(LPF_BIT1, OUTPUT);
  pinMode(LPF_BIT2, OUTPUT);
  digitalWrite(LPF_BIT0, LOW);
  digitalWrite(LPF_BIT1, LOW);
  digitalWrite(LPF_BIT2, LOW);
#elif defined(LPF_SWITCHING_SIMPLE)
  pinMode(LPF_RELAY_1, OUTPUT);
  pinMode(LPF_RELAY_2, OUTPUT);
  pinMode(LPF_RELAY_3, OUTPUT);
  digitalWrite(LPF_RELAY_1, LOW);
  digitalWrite(LPF_RELAY_2, LOW);
  digitalWrite(LPF_RELAY_3, LOW);
#endif
}

void set_by_frequency(uint32_t freq_khz) {
  uint8_t mhz = freq_khz / 1000;
  uint8_t index = 0;

  // Encontrar el filtro adecuado
  for (uint8_t i = 0; i < NUM_LPF; i++) {
    if (mhz < lpf_frequencies[i]) {
      index = i > 0 ? i - 1 : 0;
      break;
    }
    index = NUM_LPF - 1;
  }

  set_by_index(index);
}

void set_by_index(uint8_t index) {
  if (index >= NUM_LPF) index = NUM_LPF - 1;

#ifdef LPF_SWITCHING_DL2MAN_USDX_REV3
  // Configurar bits según el índice
  digitalWrite(LPF_BIT0, index & 0x01);
  digitalWrite(LPF_BIT1, index & 0x02);
  digitalWrite(LPF_BIT2, index & 0x04);
#elif defined(LPF_SWITCHING_SIMPLE)
  // Activar relés según el índice
  digitalWrite(LPF_RELAY_1, index >= 1 ? HIGH : LOW);
  digitalWrite(LPF_RELAY_2, index >= 3 ? HIGH : LOW);
  digitalWrite(LPF_RELAY_3, index >= 5 ? HIGH : LOW);
#endif
}

void off() {
#ifdef LPF_SWITCHING_DL2MAN_USDX_REV3
  digitalWrite(LPF_BIT0, LOW);
  digitalWrite(LPF_BIT1, LOW);
  digitalWrite(LPF_BIT2, LOW);
#elif defined(LPF_SWITCHING_SIMPLE)
  digitalWrite(LPF_RELAY_1, LOW);
  digitalWrite(LPF_RELAY_2, LOW);
  digitalWrite(LPF_RELAY_3, LOW);
#endif
}

} // namespace lpf
