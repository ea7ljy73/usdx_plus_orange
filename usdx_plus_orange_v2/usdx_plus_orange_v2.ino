// usdx_plus_orange_v2.ino - uSDX Plus Orange v2 (modular, en construccion)
// Paso 2: DSP TX + RX extraidos (ssb, dsp_tx, slow_dsp, CIC) compilando verde.
// (aun no es un firmware completo; se construye incrementalmente)

#include "i2c.h"
#include "rx.h"
#include "si5351.h"
#include "tx.h"
#include "usdx_settings.h"

SI5351 si5351;

volatile uint8_t mode   = USB;
volatile int8_t  volume = 12;
volatile uint8_t agc    = 2;

void setup() {
  i2c.begin();
  Wire.begin();
  si5351.powerDown();
  si5351.freq(7100000, 0, 0);
  for(uint16_t i = 0; i != 256; i++)
    lut[i] = (i * (220 - 115)) / 255 + 115;
}

void loop() {
  // placeholder: exercise RX demod + TX mod
  static uint16_t ph = 0;
  ph                 = (ph + 511) & 1023;
  int16_t tone       = 300 - (ph >> 5); // small synthetic I/Q
  int16_t out        = slow_dsp(tone, tone >> 1);
  si5351.freq_calc_fast(ssb(tone - 150));
}