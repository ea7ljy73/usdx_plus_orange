// usdx_plus_orange_v2.ino - uSDX Plus Orange v2 (modular)
// Paso 4: firware operativo mínimo - RX/TX real con el DSP extraído.

#include "hw.h"
#include "i2c.h"
#include "rx.h"
#include "si5351.h"
#include "tx.h"
#include "usdx_settings.h"

SI5351 si5351;

// --- Operador / control ------------------------------------------------
volatile uint8_t mode   = USB;
volatile int8_t  volume = 12;
volatile uint8_t agc    = 2;

volatile uint8_t txdelay               = 1; // TX relay/delay in ms (TX_DELAY)
volatile uint8_t practice              = 0; // TX disabled for practice
volatile uint8_t vox_tx                = 0; // VOX currently transmitting
volatile uint8_t basic_state_backlight = 1;

// Error / init
const char VERSION[] = "2.0.0";

void build_lut() {
  int16_t pwm_min = 115; // PA bias min (as v1 default)
  int16_t pwm_max = 220; // PA bias max
  for(uint16_t i = 0; i != 256; i++)
    lut[i] = (i * (pwm_max - pwm_min)) / 255 + pwm_min;
}

void setup() {
  digitalWrite(KEY_OUT, LOW); // safety: no PA bias on boot
  si5351.powerDown();

  MCUSR = 0;
  wdt_enable(WDTO_4S); // watchdog 4s

  ADMUX  = (1 << REFS0); // AREF 5V
  PCICR  = 0;
  PCMSK0 = 0;
  PCMSK1 = 0;
  PCMSK2 = 0;

  initPins();
  delay(100);

  // --- SI5351 ---
  delay(100);

  // --- PWM (audio out + key-shaping) ---
  timer1_start(78125);

  // --- ADC: I (AUDIO2=ADC0), Q (AUDIO1=ADC1), MIC (DVM=ADC2) ---
  admux[0] = 0;                         // CLK0 I-sample (AUDIO2 / ADC0)
  admux[1] = 1;                         // CLK1 Q-sample (AUDIO1 / ADC1)
  admux[2] = 2;                         // MIC (DVM / ADC2)
  adc_start(admux[0], true, F_SAMP_RX); // 1.1V ref for RX I/Q
  adc_stop();
  adc_start(admux[2], true, 192307); // mic channel

  build_lut();

  // --- RX begin ---
  func_ptr = sdr_rx_00;
  rx_state = 0;
  timer2_start(F_SAMP_RX);
  si5351.freq(7100000, 0, 0); // start on 40m USB
}

void loop() {
  wdt_reset();
  delay(1);

  // --- VOX based RX/TX (SSB) ---
  if(vox && (mode == LSB || mode == USB || mode == AM || mode == FM)) {
    if(!vox_tx) { // VOX idle: sample mic to detect audio
      static uint8_t  vox_sample;
      static uint16_t vox_adc;
      if(vox_sample++ == 16) {
        ssb(((int16_t)(vox_adc / 16) - (512 - AF_BIAS)) >> MIC_ATTEN);
        vox_sample = 0;
        vox_adc    = 0;
      } else {
        vox_adc += analogSampleMic();
      }
      if(tx) { // audio detected -> TX
        vox_tx = 1;
        switch_rxtx(255);
      }
    } else if(!tx) { // no audio -> back to RX
      switch_rxtx(0);
      vox_tx = 0;
    }
  }
}