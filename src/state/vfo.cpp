#include "vfo.h"
#include "../state/state.h"

static uint8_t step_index = STEP_1k;

void vfo_init()
{
  vfosel = VFOA;
  step_index = STEP_1k;
}

void vfo_tune(int8_t direction)
{
  int32_t step = stepsizes[step_index];
  int32_t new_freq = freq + (direction * step);

  if(new_freq < 100000UL) new_freq = 100000UL;
  if(new_freq > 30000000UL) new_freq = 30000000UL;

  freq = new_freq;
  frequency_update(freq);
}

void vfo_step_size(uint8_t step)
{
  step_index = step;
  if(step_index >= N_STEPSIZES) step_index = N_STEPSIZES - 1;
}

uint8_t vfo_get_step_size()
{
  return step_index;
}

void vfo_swap()
{
  if(vfosel == VFOA){
    vfosel = VFOB;
    vfo[0] = freq;
    vfomode[0] = mode;
  } else {
    vfosel = VFOA;
    vfo[1] = freq;
    vfomode[1] = mode;
  }
  vfo_update();
}

void vfo_sel(uint8_t sel)
{
  vfosel = sel;
  vfo_update();
}

void rit_increment(int8_t delta)
{
#ifdef RIT_ENABLE
  rit += delta * 10;
  if(rit < -1000) rit = -1000;
  if(rit > 1000) rit = 1000;
  frequency_update(freq);
#endif
}

void rit_clear()
{
#ifdef RIT_ENABLE
  rit = 0;
  frequency_update(freq);
#endif
}
