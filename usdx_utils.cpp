#include "usdx_utils.h"

#ifdef CAT
extern void serialEvent();
#endif

extern int __bss_end;

int freeMemory() {
  char *sp = reinterpret_cast<char *>(SP);
  return sp - &__bss_end;
}

uint8_t _digitalRead(uint8_t pin) {
#ifdef CAT
  serialEvent();
#endif
  if (cat_key) {
    return (pin == BUTTONS) ? ((cat_key & 0x07) > 0)
           : (pin == DIT)   ? ~cat_key & 0x10
           : (pin == DAH)   ? ~cat_key & 0x20
                            : 0;
  }
  return digitalRead(pin);
}
