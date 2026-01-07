/**
 * @file si5351.cpp
 * @brief Implementación del driver SI5351
 */

#include "si5351.h"
#include <avr/io.h>

// Constructor
SI5351::SI5351() {
  _i2c_addr = SI5351_ADDR;
  _xtal_freq = SI5351_CRYSTAL_FREQ;
  _pll_a_freq = 0;
  _pll_b_freq = 0;
  _pll_a_mult = 0;
  _pll_b_mult = 0;
  _ms0_div = 0;
  _ms1_div = 0;
  _ms2_div = 0;
}

void SI5351::i2c_write(uint8_t reg, uint8_t data) {
  Wire.beginTransmission(_i2c_addr);
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();
}

void SI5351::calc_pll_params(uint32_t pll_freq, uint32_t *num, uint32_t *denom,
                            uint32_t *i2c_out, uint8_t *rdiv, uint32_t *s) {
  uint32_t ref_freq = _xtal_freq;

  // Calcular el divisor del sintetizador
  uint8_t div = 4;
  while (pll_freq / div >= SI5351_VCO_MIN) {
    div++;
  }
  div--;
  if (div < 4) div = 4;
  if (div > 600) div = 600;

  // Calcular los parámetros
  *num = (pll_freq / ref_freq) * 128;
  *denom = 128;

  // Reducir fracción
  uint32_t a = *num;
  uint32_t b = *denom;
  while (b > 0) {
    uint32_t t = a % b;
    a = b;
    b = t;
  }
  *num /= a;
  *denom /= a;

  // Calcular parámetros de salida
  *s = pll_freq / div;
  *i2c_out = 0;
  *rdiv = 0;
}

void SI5351::init(uint8_t xtal_load, uint32_t xtal_freq, bool int_osc) {
  (void)int_osc;
  _xtal_freq = xtal_freq;

  Wire.begin();

  // Configurar carga del cristal
  i2c_write(137, xtal_load | 0x12);

  // Reseteo del dispositivo
  i2c_write(177, 0xAC);
  i2c_write(177, 0xAD);

  // Deshabilitar todos los clocks
  i2c_write(SI5351_REG_3_OUTPUT_ENABLE_CTRL, 0xFF);

  // Configurar fanout
  i2c_write(SI5351_REG_74_FANOUT_CTRL, 0x00);

  // Habilitar clocks necesarios
  // CLK0 y CLK1 para TX/RX
  i2c_write(SI5351_REG_16_CLOCK0_CONTROL, SI5351_CLOCK_SOURCE_XTAL);
  i2c_write(SI5351_REG_17_CLOCK1_CONTROL, SI5351_CLOCK_SOURCE_XTAL);
  i2c_write(SI5351_REG_18_CLOCK2_CONTROL, SI5351_CLOCK_SOURCE_XTAL);
}

void SI5351::reset() {
  // Reset both PLLs
  i2c_write(SI5351_REG_15_PLL_RESET, SI5351_PLL_RESET_A | SI5351_PLL_RESET_B);
}

void SI5351::set_ms(uint8_t ms_reg, uint32_t num, uint32_t denom, uint8_t rdiv) {
  uint32_t s = 0x00400000UL;

  // Escribir numerador (20 bits)
  i2c_write(ms_reg + 0, (num >> 0) & 0xFF);
  i2c_write(ms_reg + 1, (num >> 8) & 0xFF);
  i2c_write(ms_reg + 2, (num >> 16) & 0x0F);

  // Escribir denominador (20 bits)
  i2c_write(ms_reg + 3, (denom >> 0) & 0xFF);
  i2c_write(ms_reg + 4, (denom >> 8) & 0xFF);
  i2c_write(ms_reg + 5, (denom >> 16) & 0x0F);

  // Escribir parámetro de salida y rdiv
  uint8_t val = ((rdiv & 0x07) << 4) | 0x00;
  i2c_write(ms_reg + 6, val);
}

void SI5351::freq(uint32_t freq, uint8_t output, uint8_t drive_strength) {
  uint32_t pll_freq;
  uint32_t num, denom, s;
  uint8_t rdiv;
  uint8_t ms_reg;

  // Usar PLL A para CLK0 y CLK1, PLL B para CLK2
  if (output == 2) {
    pll_freq = 6 * freq;
    ms_reg = SI5351_REG_58_SYNTH_MS2;
  } else {
    pll_freq = 4 * freq;
    ms_reg = (output == 0) ? SI5351_REG_48_SYNTH_MS0 : SI5351_REG_53_SYNTH_MS1;
  }

  calc_pll_params(pll_freq, &num, &denom, &s, &rdiv, &s);
  set_ms(ms_reg, num, denom, rdiv);

  // Configurar el clock de salida
  uint8_t clock_ctrl_reg = SI5351_REG_16_CLOCK0_CONTROL + output;
  uint8_t ctrl = drive_strength & 0x03;
  ctrl |= SI5351_CLOCK_SOURCE_XTAL;  // Fuente del PLL correspondiente

  i2c_write(clock_ctrl_reg, ctrl);

  // Habilitar el clock
  enable(output, true);
}

void SI5351::freq_calc_fast(uint32_t freq) {
  uint32_t num = freq << 5;
  uint32_t denom = 1000000UL;

  // Configurar MS0 para la frecuencia
  set_ms(SI5351_REG_48_SYNTH_MS0, num, denom, 0);

  // Update registers
  SendPLLRegisterBulk();
}

void SI5351::enable(uint8_t output, bool enabled) {
  uint8_t reg_val;
  Wire.beginTransmission(_i2c_addr);
  Wire.write(SI5351_REG_3_OUTPUT_ENABLE_CTRL);
  Wire.endTransmission();
  Wire.requestFrom(_i2c_addr, (uint8_t)1);
  reg_val = Wire.read();

  if (enabled) {
    reg_val &= ~(1 << output);
  } else {
    reg_val |= (1 << output);
  }

  i2c_write(SI5351_REG_3_OUTPUT_ENABLE_CTRL, reg_val);
}

void SI5351::set_clock_enable(uint8_t clk_en) {
  i2c_write(SI5351_REG_3_OUTPUT_ENABLE_CTRL, ~clk_en);
}

void SI5351::SendPLLRegisterBulk() {
  // Los registros del PLL ya fueron escritos en set_ms()
  // Solo necesitamos un pequeño delay
  delayMicroseconds(200);
}

void SI5351::SendRegister(uint8_t reg, uint8_t data) {
  i2c_write(reg, data);
}

void SI5351::powerDown() {
  // Power down all clocks
  i2c_write(SI5351_REG_16_CLOCK0_CONTROL, SI5351_CLOCK_POWER_DOWN);
  i2c_write(SI5351_REG_17_CLOCK1_CONTROL, SI5351_CLOCK_POWER_DOWN);
  i2c_write(SI5351_REG_18_CLOCK2_CONTROL, SI5351_CLOCK_POWER_DOWN);
}
