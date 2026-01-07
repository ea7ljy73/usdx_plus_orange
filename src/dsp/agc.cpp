#include "agc.h"

void agc_reset()
{
  centiGain = 128;
  decayCount = DECAY_FACTOR;
}
