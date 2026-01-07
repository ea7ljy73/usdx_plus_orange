#include "rx_tx.h"
#include "../hal/gpio.h"
#include "../drivers/si5351.h"

static uint8_t tx_countdown = 0;

void rx_tx_init()
{
  gpio_output(RX);
  gpio_write(RX, LOW);
}

void rx_enable()
{
  gpio_write(RX, LOW);
  si5351.SendRegister(SI_CLK_OE, TX0RX0);
  tx = 0;
}

void tx_enable()
{
  gpio_write(RX, HIGH);
  si5351.SendRegister(SI_CLK_OE, TX1RX0);
  tx = 255;
}

void tx_disable()
{
  rx_enable();
}

void toggle_tx()
{
  if(tx) tx_disable();
  else tx_enable();
}

bool is_tx()
{
  return tx != 0;
}
