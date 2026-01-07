/**
 * @file si5351.h
 * @brief Driver para el sintetizador de frecuencia SI5351
 *
 * Controla el generador de clock SI5351 para generar las frecuencias
 * necesarias para el receptor/transmisor uSDX.
 *
 * Basado en código de Etherkit (KJ7ND) y otros proyectos open source.
 */

#ifndef DRV_SI5351_H
#define DRV_SI5351_H

#include <Arduino.h>
#include <Wire.h>

// ============================================================================
// CONSTANTES
// ============================================================================

// Dirección I2C del SI5351
#define SI5351_ADDR 0x60

// Frecuencia del cristal interno (通常 25MHz o 27MHz)
#ifndef SI5351_CRYSTAL_FREQ
#define SI5351_CRYSTAL_FREQ 25000000UL
#endif

// Carga del cristal
#define SI5351_CRYSTAL_LOAD_6PF  0b01000000
#define SI5351_CRYSTAL_LOAD_8PF  0b10000000
#define SI5351_CRYSTAL_LOAD_10PF 0b11000000

// Registros del SI5351
#define SI5351_REG_0_DEVICE_STATUS     0
#define SI5351_REG_1_INTERRUPT_STATUS  1
#define SI5351_REG_2_INTERRUPT_MASK    2
#define SI5351_REG_3_OUTPUT_ENABLE_CTRL 3
#define SI5351_REG_9_OEB_PIN_ENABLE    9
#define SI5351_REG_15_PLL_RESET       15
#define SI5351_REG_16_CLOCK0_CONTROL  16
#define SI5351_REG_17_CLOCK1_CONTROL  17
#define SI5351_REG_18_CLOCK2_CONTROL  18
#define SI5351_REG_19_CLOCK3_CONTROL  19
#define SI5351_REG_20_CLOCK4_CONTROL  20
#define SI5351_REG_21_CLOCK5_CONTROL  21
#define SI5351_REG_22_CLOCK6_CONTROL  22
#define SI5351_REG_23_CLOCK7_CONTROL  23
#define SI5351_REG_24_STABLE_FLAGS    24
#define SI5351_REG_25_CLOCK0_OUTPUT   25
#define SI5351_REG_26_CLOCK1_OUTPUT   26
#define SI5351_REG_27_CLOCK2_OUTPUT   27
#define SI5351_REG_28_CLOCK3_OUTPUT   28
#define SI5351_REG_29_CLOCK4_OUTPUT   29
#define SI5351_REG_30_CLOCK5_OUTPUT   30
#define SI5351_REG_31_CLOCK6_OUTPUT   31
#define SI5351_REG_32_CLOCK7_OUTPUT   32
#define SI5351_REG_33_SYNTH_PLL_A    33
#define SI5351_REG_34_SYNTH_PLL_A    34
#define SI5351_REG_35_SYNTH_PLL_A    35
#define SI5351_REG_36_SYNTH_PLL_A    36
#define SI5351_REG_37_SYNTH_PLL_A    37
#define SI5351_REG_38_SYNTH_PLL_A    38
#define SI5351_REG_42_SYNTH_PLL_B    42
#define SI5351_REG_43_SYNTH_PLL_B    43
#define SI5351_REG_44_SYNTH_PLL_B    44
#define SI5351_REG_45_SYNTH_PLL_B    45
#define SI5351_REG_46_SYNTH_PLL_B    46
#define SI5351_REG_47_SYNTH_PLL_B    47
#define SI5351_REG_48_SYNTH_MS0      48
#define SI5351_REG_49_SYNTH_MS0      49
#define SI5351_REG_50_SYNTH_MS0      50
#define SI5351_REG_51_SYNTH_MS0      51
#define SI5351_REG_52_SYNTH_MS0      52
#define SI5351_REG_53_SYNTH_MS1      53
#define SI5351_REG_54_SYNTH_MS1      54
#define SI5351_REG_55_SYNTH_MS1      55
#define SI5351_REG_56_SYNTH_MS1      56
#define SI5351_REG_57_SYNTH_MS1      57
#define SI5351_REG_58_SYNTH_MS2      58
#define SI5351_REG_59_SYNTH_MS2      59
#define SI5351_REG_60_SYNTH_MS2      60
#define SI5351_REG_61_SYNTH_MS2      61
#define SI5351_REG_62_SYNTH_MS2      62
#define SI5351_REG_74_FANOUT_CTRL    74

// Bits de control de clock
#define SI5351_CLOCK_POWER_DOWN     0b10000000
#define SI5351_CLOCK_INTEGER_MODE   0b01000000
#define SI5351_CLOCK_INPUT_DIV2     0b00100000
#define SI5351_CLOCK_INPUT_DIV4     0b00010000
#define SI5351_CLOCK_INVERT         0b00001000
#define SI5351_CLOCK_SOURCE_XTAL    0b00000000
#define SI5351_CLOCK_SOURCE_MS0     0b00000100

// Máscaras para PLL
#define SI5351_PLL_MASK             0b01111111
#define SI5351_PLL_RESET_A          0b10100000
#define SI5351_PLL_RESET_B          0b10110000

// Frecuencia mínima del VCO (Hz)
// El SI5351 tiene un rango de VCO de 600MHz a 900MHz (oficial) o 300MHz a ~1200MHz (extendido)
#define SI5351_VCO_MIN 600000000UL

// Frecuencia objetivo del PLL (Hz)
// 432MHz = 16 * 27MHz, un valor típico para operación en HF
#define SI5351_PLL_FREQ 900000000UL

// Dirección del registro de output enable
#define SI_CLK_OE 165

// ============================================================================
// CLASE SI5351
// ============================================================================

class SI5351 {
private:
  uint8_t _i2c_addr;
  uint32_t _xtal_freq;
  uint32_t _pll_a_freq;
  uint32_t _pll_b_freq;
  uint8_t _pll_a_mult;
  uint8_t _pll_b_mult;

  uint8_t _ms0_div;
  uint8_t _ms1_div;
  uint8_t _ms2_div;

  // Variables para calculations
  uint32_t _calc_num;
  uint32_t _calc_denom;
  uint32_t _calc_i2cout;
  uint8_t _calc_rdiv;
  uint32_t _calc_s;

public:
  // PLL frequency tracking for reset detection (from legacy code)
  int16_t iqmsa;

  // I2C write
  void i2c_write(uint8_t reg, uint8_t data);

  // Calcula los parámetros del PLL
  void calc_pll_params(uint32_t pll_freq, uint32_t *num, uint32_t *denom, uint32_t *i2c_out, uint8_t *rdiv, uint32_t *s);

  // Configura el sintetizador
  void set_ms(uint8_t ms_reg, uint32_t num, uint32_t denom, uint8_t rdiv);

public:
  SI5351();

  /**
   * @brief Inicializa el SI5351
   * @param xtal_load Carga del cristal (SI5351_CRYSTAL_LOAD_10PF, etc)
   * @param xtal_freq Frecuencia del cristal en Hz
   * @param int_osc Usar oscilador interno si true
   */
  void init(uint8_t xtal_load, uint32_t xtal_freq, bool int_osc);

  /**
   * @brief Resetea los PLLs
   */
  void reset();

  /**
   * @brief Establece la frecuencia de salida
   * @param freq Frecuencia en Hz
   * @param output Salida (0, 1, o 2)
   * @param drive_strength Fuerza de salida (0-3)
   */
  void freq(uint32_t freq, uint8_t output, uint8_t drive_strength);

  /**
   * @brief Establece la frecuencia de salida (versión rápida)
   * Solo funciona si la frecuencia está dentro del rango del PLL
   * @param freq Frecuencia en Hz
   */
  void freq_calc_fast(uint32_t freq);

  /**
   * @brief Habilita o deshabilita un clock
   * @param output Salida (0, 1, o 2)
   * @param enabled true para habilitar
   */
  void enable(uint8_t output, bool enabled);

  /**
   * @brief Habilita la salida del clock
   * @param clk_en Máscara de bits para habilitar clocks
   */
  void set_clock_enable(uint8_t clk_en);

  /**
   * @brief Envía todos los registros del PLL
   */
  void SendPLLRegisterBulk();

  /**
   * @brief Envía un registro específico
   * @param reg Registro a enviar
   */
  void SendRegister(uint8_t reg, uint8_t data);

  /**
   * @brief Pone el chip en modo bajo consumo
   */
  void powerDown();
};

#endif // DRV_SI5351_H
