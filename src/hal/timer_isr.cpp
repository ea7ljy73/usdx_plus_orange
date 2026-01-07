#include <Arduino.h>
#include <avr/interrupt.h>
#include "../state/rx_tx.h"

ISR(TIMER2_COMPA_vect)
{
  if(func_ptr != NULL) {
    func_ptr();
  }
}
