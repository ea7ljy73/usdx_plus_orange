// usdx_plus_orange_v2.ino - uSDX Plus Orange v2 (modular, en construccion)
// Paso 1: I2C + SI5351 con el build verde.
// (aun no es un firmware completo; se construye incrementalmente)

#include "i2c.h"
#include "si5351.h"
#include "usdx_settings.h"

SI5351 si5351;

void setup() {
  i2c.begin();
  Wire.begin();
  si5351.powerDown();
  si5351.freq(7100000, 0, 0);
}

void loop() {
  // placeholder: benchmark de freq_calc_fast para validar Paso 1
  static int16_t df = 0;
  df                = (df + 1) & 4095;
  si5351.freq_calc_fast(df - 2048);
  delay(1);
}