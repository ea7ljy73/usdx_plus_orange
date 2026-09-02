// usdx_plus_orange_v2.ino - uSDX Plus Orange v2 (modular)
// Paso 4b: display + encoder + sintonizacion VFO + VOX.

#include "display.h"
#include "hw.h"
#include "i2c.h"
#include "rx.h"
#include "si5351.h"
#include "tx.h"
#include "usdx_settings.h"

SI5351 si5351;
LCD    lcd;

// --- Operador / control ------------------------------------------------
volatile uint8_t mode   = USB;
volatile int8_t  volume = 12;
volatile uint8_t agc    = 2;

volatile uint8_t txdelay  = 1; // TX relay/delay (ms)
volatile uint8_t practice = 0; // TX disabled
volatile uint8_t vox_tx   = 0; // VOX currently transmitting

// --- VFO / sintonia ---
volatile int32_t freq        = 7100000;
volatile uint8_t stepsize    = 0; // 0=10Hz 1=100 2=1k 3=10k
const int32_t    step_mult[] = {10, 100, 1000, 10000};

inline void set_freq(int32_t f) {
  if(f < 100000)
    f = 100000;
  if(f > 60000000)
    f = 60000000;
  freq              = f;
  bool prompting_tx = tx && (mode != CW);
  si5351.freq_calc_fast(0);
  si5351.SendPLLRegisterBulk(); // restore RX freq to display
  si5351.freq(freq, 0, 0);
}

inline void do_tune() {
  int32_t d = encoder_val;
  if(d) {
    encoder_val = 0;
    freq += d * step_mult[stepsize];
    if(freq < 100000)
      freq = 100000;
    if(freq > 60000000)
      freq = 60000000;
    si5351.freq(freq, 0, 0);
  }
}

void display_vfo() {
  lcd.setCursor(0, 0);
  lcd.print("F:");
  char b[11];
  ltoa(freq, b, 10);
  lcd.print(b);
  lcd.print("               "); // clear rest of row
  lcd.setCursor(0, 1);
  lcd.print(mode == 1 ? "USB" : (mode == 0 ? "LSB" : (mode == 2 ? "CW " : "X  ")));
  lcd.print("  ");
  lcd.print(tx ? 'T' : 'R');
}

void build_lut() {
  int16_t pwm_min = 115;
  int16_t pwm_max = 220;
  for(uint16_t i = 0; i != 256; i++)
    lut[i] = (i * (pwm_max - pwm_min)) / 255 + pwm_min;
}

void setup() {
  digitalWrite(KEY_OUT, LOW); // safety: no PA bias on boot
  si5351.powerDown();

  MCUSR = 0;
  wdt_enable(WDTO_4S);

  ADMUX  = (1 << REFS0);
  PCICR  = 0;
  PCMSK0 = 0;
  PCMSK1 = 0;
  PCMSK2 = 0;

  initPins();
  delay(100);

  lcd.begin(16, 2);
  lcd.print("uSDX v2");
  delay(300);

  // --- SI5351 ---
  timer1_start(78125);
  si5351.freq(freq, 0, 0);

  // --- ADC ---
  admux[0] = 0; // I (AUDIO2/ADC0)
  admux[1] = 1; // Q (AUDIO1/ADC1)
  admux[2] = 2; // MIC (DVM/ADC2)
  adc_start(admux[0], true, F_SAMP_RX);
  adc_stop();
  adc_start(admux[2], true, 192307);

  build_lut();
  encoder_setup();

  // --- RX begin ---
  func_ptr = sdr_rx_00;
  rx_state = 0;
  timer2_start(F_SAMP_RX);
  display_vfo();
}

void loop() {
  wdt_reset();

  // --- Encoder tuning ---
  do_tune();
  if(millis() & 0x1FF)
    ;
  // --- Button: cycle STEP ---
  static uint8_t btn_last = 1;
  uint8_t        btn      = !digitalRead(BUTTONS);
  if(btn && !btn_last) {
    stepsize = (stepsize + 1) % 4;
    lcd.setCursor(12, 1);
    lcd.print("S");
    lcd.print(stepsize);
    display_vfo();
  }
  btn_last = btn;

  if(!(millis() % 500))
    display_vfo(); // periodic refresh

  // --- VOX based RX/TX ---
  if(vox && (mode == LSB || mode == USB)) {
    if(!vox_tx) {
      static uint8_t  vox_sample;
      static uint16_t vox_adc;
      if(vox_sample++ == 16) {
        ssb(((int16_t)(vox_adc / 16) - (512 - AF_BIAS)) >> MIC_ATTEN);
        vox_sample = 0;
        vox_adc    = 0;
      } else {
        vox_adc += analogSampleMic();
      }
      if(tx) {
        vox_tx = 1;
        switch_rxtx(255);
        display_vfo();
      }
    } else if(!tx) {
      switch_rxtx(0);
      vox_tx = 0;
      display_vfo();
    }
  }
  delay(1);
}