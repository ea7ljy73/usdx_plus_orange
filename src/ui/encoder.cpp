#include "encoder.h"
#include "../hal/gpio.h"
#include "menu.h"
#include "../state/vfo.h"

void encoder_init()
{
  gpio_input_pullup(ROT_A);
  gpio_input_pullup(ROT_B);
  gpio_input_pullup(BUTTONS);
  PCMSK2 |= (1 << PCINT22) | (1 << PCINT23);
  PCICR |= (1 << PCIE2);
  last_state = (gpio_read_direct(ROT_B) << 1) | gpio_read_direct(ROT_A);
  interrupts();
}

int8_t encoder_read()
{
  int8_t val = encoder_val;
  encoder_val = 0;
  return val;
}

void encoder_reset()
{
  encoder_val = 0;
  encoder_step = 0;
}

bool encoder_button_pressed()
{
  return !gpio_read_direct(BUTTONS);
}

void encoder_process()
{
  if(encoder_val != 0){
    if(menumode == 0){
      vfo_tune(encoder_val);
    } else {
      menu_process();
    }
    encoder_val = 0;
  }

  if(encoder_button_pressed()){
    delay(20);
    while(encoder_button_pressed());
    delay(20);
    if(menumode == 0){
      menu_enter();
    } else {
      menu_exit();
    }
  }
}

ISR(PCINT2_vect)
{
  switch(last_state = (last_state << 4) | (gpio_read_direct(ROT_B) << 1) | gpio_read_direct(ROT_A)){
    case 0x23: encoder_val++; break;
    case 0x32: encoder_val--; break;
  }
}
