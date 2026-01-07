#include "rx_tx.h"
#include "../hal/gpio.h"
#include "../hal/adc.h"
#include "../hal/timer.h"
#include "../drivers/si5351.h"
#include "../dsp/ssb.h"

static uint8_t tx_countdown = 0;
uint8_t rx_state = 0;

extern uint8_t ramp[];
extern uint8_t txdelay;
extern uint8_t semi_qsk;
extern uint32_t semi_qsk_timeout;
extern volatile uint8_t practice;
extern volatile uint32_t cw_offset;
extern volatile int8_t mox;
extern volatile bool quad;
extern volatile uint8_t swrmeter;
extern volatile uint8_t agc;
extern int16_t centiGain;
extern int16_t _centiGain;

void dummy() {}

void rx_tx_init()
{
  gpio_output(RX);
  gpio_write(RX, LOW);
  gpio_output(KEY_OUT);
  gpio_write(KEY_OUT, LOW);
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

void start_rx()
{
  _init = 1;
  rx_state = 0;
  func_ptr = sdr_rx_00;

  adc_start(2, true, F_ADC_CONV * 4);
  admux[2] = ADMUX;

  adc_start(0, !(att == 1), F_ADC_CONV);
  admux[0] = ADMUX;
  adc_start(1, !(att == 1), F_ADC_CONV);
  admux[1] = ADMUX;

  timer1_start(F_SAMP_PWM);
  timer2_start(F_SAMP_RX);

  gpio_write(KEY_OUT, LOW);
}

void switch_rxtx(uint8_t tx_enable)
{
  TIMSK2 &= ~(1 << OCIE2A);
  delayMicroseconds(20);
  noInterrupts();

  tx = tx_enable;

#ifdef TX_DELAY
  if((txdelay) && (tx_enable) && (!(tx)) && (!(practice))) {
    gpio_write(RX, LOW);
    lcd.setCursor(15, 1); lcd.print('D');
    interrupts();
    delay(10);
    noInterrupts();
  }
#endif

  if(tx_enable) {
    _centiGain = centiGain;
    func_ptr = dsp_tx;

    if(practice) {
      gpio_write(RX, LOW);
      lcd.setCursor(15, 1); lcd.print('P');
      si5351.SendRegister(SI_CLK_OE, TX0RX0);
    } else {
      gpio_write(RX, LOW);
      lcd.setCursor(15, 1); lcd.print('T');

      if(mode == CW) {
        si5351.freq_calc_fast(-cw_offset);
        si5351.SendPLLRegisterBulk();
      }
#ifdef RIT_ENABLE
      else if(rit) {
        si5351.freq_calc_fast(0);
        si5351.SendPLLRegisterBulk();
      }
#endif

      si5351.SendRegister(SI_CLK_OE, TX1RX0);
      OCR1AL = 0x80;
      if((!mox) && (mode != CW)) TCCR1A &= ~(1 << COM1A1);
      TCCR1A |= (1 << COM1B1);

      if(mode == CW) {
        func_ptr = dsp_tx_cw;
      } else if(mode == AM) {
        func_ptr = dsp_tx_am;
      } else if(mode == FM) {
        func_ptr = dsp_tx_fm;
      }
    }
  } else {
    if((mode == CW) && (!(semi_qsk_timeout))){
#ifdef SEMI_QSK
      semi_qsk_timeout = millis() + 480;
#endif
      if(semi_qsk) func_ptr = dummy; else func_ptr = sdr_rx_00;
    } else {
      _centiGain = centiGain;
#ifdef SEMI_QSK
      semi_qsk_timeout = 0;
#endif
      func_ptr = sdr_rx_00;
    }

    if(OCR1BL != 0) {
      for(uint16_t i = 0; i != 31; i++) {
        OCR1BL = lut[pgm_read_byte_near(&ramp[i])];
        delayMicroseconds(60);
      }
    }
    TCCR1A |= (1 << COM1A1);
    TCCR1A &= ~(1 << COM1B1);
    gpio_write(KEY_OUT, LOW);
    OCR1BL = 0;

    si5351.SendRegister(SI_CLK_OE, TX0RX1);
    gpio_write(RX, !(att == 2));

#ifdef RIT_ENABLE
    si5351.freq_calc_fast(rit);
#else
    si5351.freq_calc_fast(0);
#endif
    si5351.SendPLLRegisterBulk();
  }

  if((!dsp_cap) && (!tx_enable) && vox) func_ptr = dummy;

  interrupts();
  if(tx_enable) ADMUX = admux[2];
  else _init = 1;
  rx_state = 0;
}
