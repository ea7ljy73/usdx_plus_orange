#ifndef DSP_H
#define DSP_H

#include "../driver/display.h"
#include "../hardware/wire.h"
#include <Arduino.h>

// Forward declarations of dependencies in .ino
class SI5351;
extern SI5351 si5351;

// Forward declarations of local functions
void dec2();
void sdr_rx_00();
void sdr_rx_01();
void sdr_rx_02();
void sdr_rx_03();
void sdr_rx_04();
void sdr_rx_05();
void sdr_rx_06();
void sdr_rx_07();
void sdr_rx();
void sdr_rx_q();
inline int16_t sdr_rx_common_i();
inline int16_t sdr_rx_common_q();
inline int16_t sdr_rx_common();

// DSP Module extraction
// Contains Signal Processing, Filters, AGC, NR, and SSB/CW Generation logic
#ifdef DEBUG
static uint32_t sr = 0;
static uint32_t cpu_load = 0;
volatile uint16_t param_a =
    0; // registers for debugging, testing and experimental purposes
volatile int16_t param_b = 0;
volatile int16_t param_c = 0;
#endif

enum dsp_cap_t { ANALOG, DSP, SDR };
#ifdef QCX
uint8_t dsp_cap = 0;
uint8_t ssb_cap = 0;
#else
// force SSB and SDR capability
const uint8_t ssb_cap = 1;
const uint8_t dsp_cap = SDR;
#endif

enum mode_t { LSB, USB, CW, FM, AM };
volatile uint8_t mode = USB;
volatile uint16_t numSamples = 0;

volatile uint8_t tx = 0;
volatile uint8_t filt = 0;

inline void _vox(bool trigger) {
  if (trigger) {
    tx = (tx) ? 254 : 255; // hangtime = 255 / 4402 = 58ms (the time that TX at
                           // least stays on when not triggered again). tx ==
                           // 255 when triggered first, 254 follows for
                           // subsequent triggers, until tx is off.
  } else {
    if (tx)
      tx--;
  }
}

#define F_SAMP_TX                                                              \
  4800 // 4810 //4805 // 4402 // (Design) ADC sample-rate; is best a multiple of
       // _UA and fits exactly in OCR2A = ((F_CPU / 64) / F_SAMP_TX) - 1 ,
       // should not exceed CPU utilization
#if (F_MCU != 20000000)
const int16_t _F_SAMP_TX =
    (F_MCU * 4800LL /
     20000000); // Actual ADC sample-rate; used for phase calculations
#else
#define _F_SAMP_TX F_SAMP_TX
#endif
#define _UA                                                                    \
  600 //=(_FSAMP_TX)/8 //(_F_SAMP_TX)      //360  // unit angle; integer
      // representation of one full circle turn or 2pi radials or 360 degrees,
      // should be a integer divider of F_SAMP_TX and maximized to have higest
      // precision
#define MAX_DP                                                                 \
  ((filt == 0)   ? _UA                                                         \
   : (filt == 3) ? _UA / 4                                                     \
                 : _UA / 2) //(_UA/2) // the occupied SSB bandwidth can be
                            // further reduced by restricting the maximum phase
                            // change (set MAX_DP to _UA/2).
#define CARRIER_COMPLETELY_OFF_ON_LOW                                          \
  1 // disable oscillator on low amplitudes, to prevent potential unwanted
    // biasing/leakage through PA circuit
#define MULTI_ADC                                                              \
  1 // multiple ADC conversions for more sensitive (+12dB) microphone input
// #define QUAD  1       // invert TX signal for phase changes > 180

inline int16_t arctan3(int16_t q, int16_t i) // error ~ 0.8 degree
{                                            // source: [1]
  // http://www-labs.iro.umontreal.ca/~mignotte/IFT2425/Documents/EfficientApproximationArctgFunction.pdf
// #define _atan2(z)  (_UA/8 + _UA/44) * z  // very much of a
// simplification...not accurate at all, but fast
#define _atan2(z)                                                              \
  (_UA / 8 + _UA / 22 - _UA / 22 * z) *                                        \
      z // derived from (5) [1]   note that atan2 can overflow easily so keep
        // _UA low
  // #define _atan2(z)  (_UA/8 + _UA/24 - _UA/24 * z) * z  //derived from (7)
  // [1]
  int16_t r;
  if (abs(q) > abs(i))
    r = _UA / 4 - _atan2(abs(i) / abs(q)); // arctan(z) = 90-arctan(1/z)
  else
    r = (i == 0) ? 0 : _atan2(abs(q) / abs(i)); // arctan(z)
  r = (i < 0) ? _UA / 2 - r : r;                // arctan(-z) = -arctan(z)
  return (q < 0) ? -r : r;                      // arctan(-z) = -arctan(z)
}

#define magn(i, q)                                                             \
  (abs(i) > abs(q) ? abs(i) + abs(q) / 4                                       \
                   : abs(q) + abs(i) / 4) // approximation of: magnitude =
                                          // sqrt(i*i + q*q); error 0.95dB

uint8_t lut[256];
volatile uint8_t amp;
#define MORE_MIC_GAIN                                                          \
  1 // adds more microphone gain, improving overall SSB quality (when speaking
    // further away from microphone)
#ifdef MORE_MIC_GAIN
volatile uint8_t vox_thresh = (1 << 2);
#else
volatile uint8_t vox_thresh = (1 << 1); //(1 << 2);
#endif
volatile uint8_t drive = 2; // hmm.. drive>2 impacts cpu load..why?

volatile uint8_t quad = 0;
volatile bool dig_mode = false;

inline int16_t ssb(int16_t in) {
  static int16_t dc, z1;

  int16_t i, q;
  uint8_t j;
  static int16_t v[16];
  for (j = 0; j != 15; j++)
    v[j] = v[j + 1];
#ifdef MORE_MIC_GAIN
  // #define DIG_MODE  // optimization for digital modes: for super flat TX
  // spectrum, (only down < 100Hz to cut-off DC components)
  if (dig_mode) {
    int16_t ac = in;
    dc = (ac + (7) * dc) / (7 + 1); // hpf: slow average
    v[15] = (ac - dc) /
            2; // hpf (dc decoupling)  (-6dB gain to compensate for DC-noise)
  } else {
    int16_t ac = in * 2; //   6dB gain (justified since lpf/hpf is losing -3dB)
    ac = ac + z1;        // lpf
    z1 = (in - (2) * z1) / (2 + 1); // lpf: notch at Fs/2 (alias rejecting)
    dc = (ac + (2) * dc) / (2 + 1); // hpf: slow average
    v[15] = (ac - dc);              // hpf (dc decoupling)
  }
  i = v[7] *
      2; // 6dB gain for i, q  (to prevent quanitization issues in hilbert
         // transformer and phase calculation, corrected for magnitude calc)
  q = ((((v[0] - v[14]) << 1) + ((v[2] - v[12]) << 3) +
        (((v[4] - v[10]) << 4) + ((v[4] - v[10]) << 2) + (v[4] - v[10])) +
        ((v[6] - v[8]) << 4)) >>
       6) +
      (v[6] - v[8]); // Hilbert transform, 40dB side-band rejection in
                     // 400..1900Hz (@4kSPS) when used in image-rejection
                     // scenario; (Hilbert transform require 5 additional bits)

  uint16_t _amp = magn(i / 2, q / 2); // -6dB gain (correction)
#else                                 // !MORE_MIC_GAIN
  dc = (in + dc) / 2;     // average
  int16_t ac = (in - dc); // DC decoupling
  v[15] = (ac + z1); // / 2;           // low-pass filter with notch at Fs/2
  z1 = ac;

  i = v[7];
  q = ((((v[0] - v[14]) << 1) + ((v[2] - v[12]) << 3) +
        (((v[4] - v[10]) << 4) + ((v[4] - v[10]) << 2) + (v[4] - v[10])) +
        (((v[6] - v[8]) << 4) - (v[6] - v[8]))) >>
       7) +
      ((v[6] - v[8]) >>
       1); // Hilbert transform, 40dB side-band rejection in 400..1900Hz
           // (@4kSPS) when used in image-rejection scenario; (Hilbert transform
           // require 5 additional bits)

  uint16_t _amp = magn(i, q);
#endif                                // MORE_MIC_GAIN

#ifdef CARRIER_COMPLETELY_OFF_ON_LOW
  _vox(_amp > vox_thresh);
#else
  if (vox)
    _vox(_amp > vox_thresh);
#endif
  _amp = _amp << (drive);
  _amp = ((_amp > 255) || (drive == 8))
             ? 255
             : _amp; // clip or when drive=8 use max output
  amp = (tx) ? lut[_amp] : 0;

  static int16_t prev_phase;
  int16_t phase = arctan3(q, i);

  int16_t dp = phase - prev_phase; // phase difference and restriction
  prev_phase = phase;

  if (dp < 0)
    dp = dp + _UA; // make negative phase shifts positive: prevents negative
                   // frequencies and will reduce spurs on other sideband
#ifdef QUAD
  if (dp >= (_UA / 2)) {
    dp = dp - _UA / 2;
    quad = !quad;
  }
#endif

#ifdef MAX_DP
  if (dp > MAX_DP) { // dp should be less than half unit-angle in order to keep
                     // frequencies below F_SAMP_TX/2
    prev_phase = phase - (dp - MAX_DP); // substract restdp
    dp = MAX_DP;
  }
#endif
  if (mode == USB)
    return dp *
           (_F_SAMP_TX /
            _UA); // calculate frequency-difference based on phase-difference
  else
    return dp * (-_F_SAMP_TX / _UA);
}

#define MIC_ATTEN                                                              \
  0 // 0*6dB attenuation (note that the LSB bits are quite noisy)
volatile int8_t mox = 0;
volatile int8_t volume = 12;

// This is the ADC ISR, issued with sample-rate via timer1 compb interrupt.
// It performs in real-time the ADC sampling, calculation of SSB
// phase-differences, calculation of SI5351 frequency registers and send the
// registers to SI5351 over I2C.
static int16_t _adc;
void dsp_tx() {  // jitter dependent things first
#ifdef MULTI_ADC // SSB with multiple ADC conversions:
  int16_t adc;   // current ADC sample 10-bits analog input, NOTE: first ADCL,
                 // then ADCH
  adc = ADC;
  ADCSRA |= (1 << ADSC);
  // OCR1BL = amp;                        // submit amplitude to PWM register
  // (actually this is done in advance (about 140us) of phase-change, so that
  // phase-delays in key-shaping circuit filter can settle)
  si5351.SendPLLRegisterBulk(); // submit frequency registers to SI5351 over
                                // 731kbit/s I2C (transfer takes 64/731 = 88us,
                                // then PLL-loopfilter probably needs 50us to
                                // stabalize)
#ifdef QUAD
#ifdef TX_CLK0_CLK1
  si5351.SendRegister(
      16, (quad)
              ? 0x1f
              : 0x0f); // Invert/non-invert CLK0 in case of a huge phase-change
  si5351.SendRegister(
      17, (quad)
              ? 0x1f
              : 0x0f); // Invert/non-invert CLK1 in case of a huge phase-change
#else
  si5351.SendRegister(
      18, (quad)
              ? 0x1f
              : 0x0f); // Invert/non-invert CLK2 in case of a huge phase-change
#endif
#endif          // QUAD
  OCR1BL = amp; // submit amplitude to PWM register (takes about 1/32125 =
                // 31us+/-31us to propagate) -> amplitude-phase-alignment error
                // is about 30-50us
  adc += ADC;
  ADCSRA |=
      (1 << ADSC); // causes RFI on QCX-SSB units (not on units with direct
                   // biasing); ENABLE this line when using direct biasing!!
  int16_t df =
      ssb(_adc >> MIC_ATTEN); // convert analog input into phase-shifts (carrier
                              // out by periodic frequency shifts)
  adc += ADC;
  ADCSRA |= (1 << ADSC);
  si5351.freq_calc_fast(df); // calculate SI5351 registers based on frequency
                             // shift and carrier frequency
  adc += ADC;
  ADCSRA |= (1 << ADSC);
  //_adc = (adc/4 - 512);
#define AF_BIAS 32
  _adc = (adc / 4 -
          (512 - AF_BIAS)); // now make sure that we keep a postive bias offset
                            // (to prevent the phase swapping 180 degrees and
                            // potentially causing negative feedback (RFI)
#else                       // SSB with single ADC conversion:
  ADCSRA |= (1 << ADSC); // start next ADC conversion (trigger ADC interrupt if
                         // ADIE flag is set)
  // OCR1BL = amp;                        // submit amplitude to PWM register
  // (actually this is done in advance (about 140us) of phase-change, so that
  // phase-delays in key-shaping circuit filter can settle)
  si5351.SendPLLRegisterBulk(); // submit frequency registers to SI5351 over
                                // 731kbit/s I2C (transfer takes 64/731 = 88us,
                                // then PLL-loopfilter probably needs 50us to
                                // stabalize)
  OCR1BL = amp; // submit amplitude to PWM register (takes about 1/32125 =
                // 31us+/-31us to propagate) -> amplitude-phase-alignment error
                // is about 30-50us
  int16_t adc = ADC - 512; // current ADC sample 10-bits analog input, NOTE:
                           // first ADCL, then ADCH
  int16_t df =
      ssb(adc >> MIC_ATTEN); // convert analog input into phase-shifts (carrier
                             // out by periodic frequency shifts)
  si5351.freq_calc_fast(df); // calculate SI5351 registers based on frequency
                             // shift and carrier frequency
#endif

#ifdef CARRIER_COMPLETELY_OFF_ON_LOW
  if (tx == 1) {
    OCR1BL = 0;
    si5351.SendRegister(SI_CLK_OE, TX0RX0);
  } // disable carrier
  if (tx == 255) {
    si5351.SendRegister(SI_CLK_OE, TX1RX0);
  } // enable carrier
#endif

#ifdef MOX_ENABLE
  if (!mox)
    return;
  OCR1AL = (adc << (mox - 1)) + 128; // TX audio monitoring
#endif
}

volatile uint16_t acc;
volatile uint32_t cw_offset;
volatile uint8_t cw_tone = 1;
const uint32_t tones[] = {F_MCU * 700ULL / 20000000, F_MCU * 600ULL / 20000000,
                          F_MCU * 700ULL / 20000000};

volatile int16_t p_sin = 0;
volatile int16_t n_cos = 20000;
inline void
process_minsky() // Minsky circle sample [source:
                 // https://www.cl.cam.ac.uk/~am21/hakmemc.html, ITEM 149]:
                 // p_sin+=n_cos*2*PI*f/fs; n_cos-=p_sin*2*PI*f/fs;
{
  int16_t alpha = (int32_t)tones[cw_tone] * 51 / _F_SAMP_TX;
  p_sin += (int32_t)alpha * n_cos >> 8;
  n_cos -= (int32_t)alpha * p_sin >> 8;
}

// CW Key-click shaping, ramping up/down amplitude with sample-interval of 60us.
// Tnx: Yves HB9EWY https://groups.io/g/ucx/message/5107
const uint8_t ramp[] PROGMEM = {
    255, 254, 252, 249, 245, 239, 233, 226, 217, 208, 198,
    187, 176, 164, 152, 139, 127, 115, 102, 90,  78,  67,
    56,  46,  37,  28,  21,  15,  9,   5,   2}; // raised-cosine(i) = 255 *
                                                // sq(cos(HALF_PI * i/32))

void dummy() {}

void dsp_tx_cw() { // jitter dependent things first
#ifdef KEY_CLICK
  if (OCR1BL < lut[255]) { // check if already ramped up: ramp up of amplitude
    for (uint16_t i = 31; i != 0; i--) { // soft rising slope against key-clicks
      OCR1BL = lut[pgm_read_byte_near(&ramp[i - 1])];
      delayMicroseconds(60);
    }
  }
#endif // KEY_CLICK
  OCR1BL = lut[255];

  process_minsky();
  OCR1AL = (p_sin >> (8 + (16 - volume))) + 128;
}

void dsp_tx_am() {       // jitter dependent things first
  ADCSRA |= (1 << ADSC); // start next ADC conversion (trigger ADC interrupt if
                         // ADIE flag is set)
  OCR1BL = amp; // submit amplitude to PWM register (actually this is done in
                // advance (about 140us) of phase-change, so that phase-delays
                // in key-shaping circuit filter can settle)
  int16_t adc = ADC - 512; // current ADC sample 10-bits analog input, NOTE:
                           // first ADCL, then ADCH
  int16_t in = (adc >> MIC_ATTEN);
  in = in << (drive - 4);
#define AM_BASE 32
  in = max(0, min(255, (in + AM_BASE)));
  amp = in; // lut[in];
}

void dsp_tx_fm() {       // jitter dependent things first
  ADCSRA |= (1 << ADSC); // start next ADC conversion (trigger ADC interrupt if
                         // ADIE flag is set)
  OCR1BL = lut[255]; // submit amplitude to PWM register (actually this is done
                     // in advance (about 140us) of phase-change, so that
                     // phase-delays in key-shaping circuit filter can settle)
  si5351.SendPLLRegisterBulk(); // submit frequency registers to SI5351 over
                                // 731kbit/s I2C (transfer takes 64/731 = 88us,
                                // then PLL-loopfilter probably needs 50us to
                                // stabalize)
  int16_t adc = ADC - 512; // current ADC sample 10-bits analog input, NOTE:
                           // first ADCL, then ADCH
  int16_t in = (adc >> MIC_ATTEN);
  in = in << (drive);
  int16_t df = in;
  si5351.freq_calc_fast(df); // calculate SI5351 registers based on frequency
                             // shift and carrier frequency
}

#define EA(y, x, one_over_alpha)                                               \
  (y) = (y) + ((x) - (y)) /                                                    \
                  (one_over_alpha); // exponental averaging [Lyons 13.33.1]
#define MLEA(y, x, L, M)                                                       \
  (y) = (y) +                                                                  \
        ((((x) - (y)) >> (L)) -                                                \
         (((x) - (y)) >> (M))); // multiplierless exponental averaging
                                // [Lyons 13.33.1], with alpha=1/2^L - 1/2^M

#ifdef SWR_METER
volatile uint8_t swrmeter = 1;
#endif

const char m2c[] PROGMEM = "~ "
                           "ETIANMSURWDKGOHVF*L*PJBXCYZQ**54S3***2**+***J16=/"
                           "***H*7*G*8*90************?_****\"**.****@***'**-***"
                           "*****;!*)*****,****:****";

#ifdef CW_MESSAGE
#define MENU_STR 1

uint8_t delayWithKeySense(uint32_t ms) {
  uint32_t event = millis() + ms;
  for (; millis() < event;) {
    wdt_reset();
    if (inv ^ digitalRead(BUTTONS) || !digitalRead(DAH) || !digitalRead(DIT)) {
      for (; inv ^ digitalRead(BUTTONS);)
        wdt_reset(); // wait until buttons released
      return 1;      // stop when button/key pressed
    }
  }
  return 0;
}
#ifdef CW_MESSAGE_EXT
char cw_msg[6][48] = {CW_MSG1, CW_MSG2, CW_MSG3, CW_MSG4, CW_MSG5, CW_MSG6};
#else
char cw_msg[1][48] = {CW_MSG1};
#endif
uint8_t cw_msg_interval = 5; // number of seconds CW message is repeated
uint32_t cw_msg_event = 0;
uint8_t cw_msg_id = 0; // selected message

int cw_tx(char ch) { // Transmit message in CW
  char sym;
  for (uint8_t j = 0; (sym = pgm_read_byte_near(m2c + j));
       j++) {        // lookup msg[i] in m2c, skip if not found
    if (sym == ch) { // found -> transmit CW character j
      wdt_reset();
      uint8_t k = 0x80;
      for (; !(j & k); k >>= 1)
        ;
      k >>= 1; // shift start of cw code to MSB
      if (k == 0)
        delay(ditTime * 4); // space -> add word space
      else {
        for (; k; k >>= 1) { // send dit/dah one by one, until everythng is sent
          switch_rxtx(1);    // key-on  tx
          if (delayWithKeySense(ditTime * ((j & k) ? 3 : 1))) {
            switch_rxtx(0);
            return 1;
          } // symbol: dah or dih length
          switch_rxtx(0); // key-off tx
          if (delayWithKeySense(ditTime))
            return 1; // add symbol space
        }
        if (delayWithKeySense(ditTime * 2))
          return 1; // add letter space
      }
      break; // next character
    }
  }
  return 0;
}

int cw_tx(char *msg) {
  for (uint8_t i = 0; msg[i]; i++) { // loop over message
    lcd.setCursor(0, 0);
    lcd.print(i);
    lcd.print("    ");
    if (cw_tx(msg[i]))
      return 1;
  }
  return 0;
}
#endif // CW_MESSAGE

volatile uint8_t menumode =
    0; // 0=not in menu, 1=selects menu item, 2=selects parameter value
volatile uint8_t ft8mode = 0;
volatile uint8_t prev_mode_ft8 = 0;
volatile uint8_t prev_filt_ft8 = 0;
volatile uint8_t prev_agc_ft8 = 0;
volatile uint8_t prev_nr_ft8 = 0;

#ifdef CW_DECODER
volatile uint8_t cwdec = 1;
static int32_t avg = 256;
static uint8_t sym;
static uint32_t amp32 = 0;
volatile uint32_t _amp32 = 0;
static char out[] = "                ";
volatile uint8_t cw_event = false;

void printsym(bool submit = true) {
  if (sym < 128) {
    char ch = pgm_read_byte_near(m2c + sym);
    if (ch != '*') {
#ifdef CW_INTERMEDIATE
      out[15] = ch;
      cw_event = true;
      if (submit) {
        for (int i = 0; i != 15; i++) {
          out[i] = out[i + 1];
        }
        out[15] = ' ';
      } // update LCD, only shift when submit is true, otherwise update last
        // char only
#else
      for (int i = 0; i != 15; i++)
        out[i] = out[i + 1];
      out[15] = ch;
      cw_event = true; // update LCD
#endif
    }
  }
  if (submit)
    sym = 1;
}

bool realstate = LOW;
bool realstatebefore = LOW;
bool filteredstate = LOW;
bool filteredstatebefore = LOW;
uint8_t nbtime = 16; // 6 // ms noise blanker
uint32_t starttimehigh;
uint32_t highduration;
uint32_t hightimesavg;
uint32_t lowtimesavg;
uint32_t startttimelow;
uint32_t lowduration;
uint32_t laststarttime = 0;
uint8_t wpm = 25;

inline void cw_decode() {
  int32_t in = _amp32;
  EA(avg, in, (1 << 8));
  realstate = (in > (avg * 1 / 2)); // threshold

  // here we clean up the state with a noise blanker
  if (realstate != realstatebefore) {
    laststarttime = millis();
  }
// #define NB_SCALED_TO_WPM    1   // Scales noise-blanker timing the actual CW
// speed; this should reduce errors from noise at low speeds; this may have
// side-effect with fast speed changes that fast CW will be filtered out
#ifdef NB_SCALED_TO_WPM
  if ((millis() - laststarttime) >
      min(1200 / (20 * 2), max(1200 / (40 * 2), hightimesavg / 6))) {
#else
  if ((millis() - laststarttime) > nbtime) {
#endif
    if (realstate != filteredstate) {
      filteredstate = realstate;
      // dec2();
    }
  } else
    avg +=
        avg /
        100; // keep threshold above noise spikes (increase threshold with 1%)

  dec2();
  realstatebefore = realstate;
}

// #define NEW_CW  1   // CW decoder portions from by Hjalmar Skovholm Hansen
// OZ1JHM, source: http://www.skovholm.com/decoder11.ino
#ifdef NEW_CW
void dec2() {
  // Then we do want to have some durations on high and low
  if (filteredstate != filteredstatebefore) {
    if (menumode == 0) {
      lcd.noCursor();
      lcd.setCursor(15, 1);
      lcd.print((filteredstate) ? 'R' : ' ');
      stepsize_showcursor();
    }

    if (filteredstate == HIGH) {
      starttimehigh = millis();
      lowduration = (millis() - startttimelow);
    }

    if (filteredstate == LOW) {
      startttimelow = millis();
      highduration = (millis() - starttimehigh);
      if (highduration < (2 * hightimesavg) || hightimesavg == 0) {
        hightimesavg = (highduration + hightimesavg + hightimesavg) /
                       3; // now we know avg dit time ( rolling 3 avg)
      }
      if (highduration > (5 * hightimesavg)) {
        hightimesavg = highduration / 3; // if speed decrease fast ..
        // hightimesavg = highduration+hightimesavg;     // if speed decrease
        // fast ..
      }
    }
  }

  // now we will check which kind of baud we have - dit or dah, and what kind of
  // pause we do have 1 - 3 or 7 pause, we think that hightimeavg = 1 bit
  if (filteredstate != filteredstatebefore) {
    if (filteredstate == LOW) { //// we did end a HIGH
#define FAIR_WEIGHTING 1
#ifdef FAIR_WEIGHTING
      if (highduration < (hightimesavg + hightimesavg / 2) &&
          highduration >
              (hightimesavg * 6 / 10)) { /// 0.6 filter out false dits
#else
      if (highduration < (hightimesavg * 2) &&
          highduration >
              (hightimesavg * 6 / 10)) { /// 0.6 filter out false dits
#endif
        sym = (sym << 1) | (0); // insert dit (0)
      }
#ifdef FAIR_WEIGHTING
      if (highduration > (hightimesavg + hightimesavg / 2) &&
          highduration < (hightimesavg * 6)) {
#else
      if (highduration > (hightimesavg * 2) &&
          highduration < (hightimesavg * 6)) {
#endif
        sym = (sym << 1) | (1); // insert dah (1)
        wpm = (wpm + (1200 / ((highduration) / 3) * 4 / 3)) / 2;
      }
    }

    if (filteredstate == HIGH) { // we did end a LOW
      uint16_t lacktime = 10;
      if (wpm > 25)
        lacktime = 10; // when high speeds we have to have a little more pause
                       // before new letter or new word
      if (wpm > 30)
        lacktime = 12;
      if (wpm > 35)
        lacktime = 15;

#ifdef FAIR_WEIGHTING
      if (lowduration > (hightimesavg * (lacktime * 1 / 10)) &&
          lowduration < hightimesavg * (lacktime * 5 / 10)) { // letter space
#else
      if (lowduration > (hightimesavg * (lacktime * 7 / 80)) &&
          lowduration < hightimesavg * (lacktime * 5 / 10)) { // letter space
        // if(lowduration > (hightimesavg*(lacktime*2/10)) && lowduration <
        // hightimesavg*(lacktime*5/10)){ // letter space
#endif
        printsym();
      }
      if (lowduration >= hightimesavg * (lacktime * 5 / 10)) { // word space
        printsym();
        printsym(); // print space
      }
    }
  }

  // write if no more letters
  if ((millis() - startttimelow) > (highduration * 6) && (sym > 1)) {
    printsym();
  }

  filteredstatebefore = filteredstate;
}

#else // OLD_CW

void dec2() {
  if (filteredstate != filteredstatebefore) { // then we do want to have some
                                              // durations on high and low
    if (menumode == 0) {
      lcd.noCursor();
      lcd.setCursor(15, 1);
      lcd.print((filteredstate) ? 'R' : ' ');
      stepsize_showcursor();
    }

    if (filteredstate == HIGH) {
      starttimehigh = millis();
      lowduration = (millis() - startttimelow);
      // highduration = 0;

      if ((sym > 1) &&
          lowduration >
              (hightimesavg *
               2) /* && lowduration < hightimesavg*(5*lacktime)*/) { // letter
                                                                     // space
        printsym();
        wpm = (1200 / hightimesavg * 4 / 3);
        // if(lowduration >= hightimesavg*(5)){ sym=1; printsym(); } // (print
        // additional space) word space
      }
      if (lowduration >= hightimesavg * (5)) {
        sym = 1;
        printsym();
      } // (print additional space) word space
    }

    if (filteredstate == LOW) {
      startttimelow = millis();
      highduration = (millis() - starttimehigh);
      // lowduration = 0;
      if (highduration < (2 * hightimesavg) || hightimesavg == 0) {
        hightimesavg = (highduration + hightimesavg + hightimesavg) /
                       3; // now we know avg dit time (rolling 3 avg)
      }
      if (highduration > (5 * hightimesavg)) {
        hightimesavg = highduration / 3; // if speed decrease fast ..
        // hightimesavg = highduration+hightimesavg;     // if speed decrease
        // fast ..
      }
      if (highduration > (hightimesavg / 2)) {
        sym = (sym << 1) |
              (highduration > (hightimesavg * 2)); // dit (0) or dash (1)
#if defined(CW_INTERMEDIATE) && !defined(OLED) && !defined(LCD_I2C) &&         \
    (F_MCU >= 20000000)
        printsym(false);
#endif
      }
    }
  }

  if (((millis() - startttimelow) > hightimesavg * (6)) && (sym > 1)) {
    printsym(); // write if no more letters
  }

  filteredstatebefore = filteredstate;
}
#endif // OLD_CW
#endif // CW_DECODER

#define F_SAMP_PWM (78125 / 1)
// #define F_SAMP_RX 78125  // overrun, do not use
#define F_SAMP_RX 62500
// #define F_SAMP_RX 52083
// #define F_SAMP_RX 44643
// #define F_SAMP_RX 39062
// #define F_SAMP_RX 34722
// #define F_SAMP_RX 31250
// #define F_SAMP_RX 28409
#define F_ADC_CONV                                                             \
  (192307 / 2) // was 192307/1, but as noted this produces clicks in audio
               // stream. Slower ADC clock cures this (but is a problem for VOX
               // when sampling mic-input simulatanously).

#ifdef FAST_AGC
volatile uint8_t agc = 2;
#else
volatile uint8_t agc = 1;
#endif
volatile uint8_t nr = 2;
volatile uint8_t att = 0;
volatile uint8_t att2 =
    2; // Minimum att2 increased, to prevent numeric overflow on strong signals
volatile uint8_t _init = 0;

// Old AGC algorithm which only increases gain, but does not decrease it for
// very strong signals. Maximum possible gain is x32 (in practice, x31) so AGC
// range is x1 to x31 = 30dB approx. Decay time is fine (about 1s) but attack
// time is much slower than I like. For weak/medium signals it aims to keep the
// sample value between 1024 and 2048.
static int16_t gain = 1024;
inline int16_t process_agc_fast(int16_t in) {
  int16_t out = (gain >= 1024) ? (gain >> 10) * in : in;
  int16_t accum = (1 - abs(out >> 10));
  if ((INT16_MAX - gain) > accum)
    gain = gain + accum;
  if (gain < 1)
    gain = 1;
  return out;
}

// Contribution by Alan, M0PUB: Experimental new AGC algorithm.
// ASSUMES: Input sample values are constrained to a maximum of +/-4096 to avoid
// integer overflow in earlier calculations.
//
// This algorithm aims to keep signals between a peak sample value of 1024 -
// 1536, with fast attack but slow decay.
//
// The variable centiGain actually represents the applied gain x 128 - i.e. the
// numeric gain applied is centiGain/128
//
// Since the largest valid input sample has a value of +/- 4096, centiGain
// should never be less than 32 (i.e. a 'gain' of 0.25). The maximum value for
// centiGain is 32767, and hence a gain of 255. So the AGC range is 0.25:255, or
// approx. 60dB.
//
// Variable 'slowdown' allows the decay time to be slowed down so that it is not
// directly related to the value of centiCount.

static int16_t centiGain = 128;
#define DECAY_FACTOR 400 // AGC decay occurs <DECAY_FACTOR> slower than attack.
static uint16_t decayCount = DECAY_FACTOR;
#define HI(x) ((x) >> 8)
#define LO(x) ((x) & 0xFF)

inline int16_t process_agc(int16_t in) {
  static bool small = true;
  int16_t out;

  if (centiGain >= 128)
    out = (centiGain >> 5) * in; // net gain >= 1
  else
    out = (centiGain >> 2) * (in >> 3); // net gain < 1
  out >>= 2;

  if (HI(abs(out)) > HI(1536)) {
    centiGain -= (centiGain >> 4); // Fast attack time when big signal
                                   // encountered (relies on CentiGain >= 16)
  } else {
    if (HI(abs(out)) > HI(1024))
      small = false;
    if (--decayCount == 0) { // But slow ramp up of gain when signal disappears
      if (small) { // 400 samples below lower threshold - increase gain
        if (centiGain < (INT16_MAX - (INT16_MAX >> 4)))
          centiGain += (centiGain >> 4);
        else
          centiGain = INT16_MAX;
      }
      decayCount = DECAY_FACTOR;
      small = true;
    }
  }
  return out;
}

inline int16_t process_nr_old(int16_t ac) {
  ac = ac >> (6 - abs(ac)); // non-linear below amp of 6; to reduce noise
                            // (switchoff agc and tune-up volume until noise
                            // dissapears, todo:extra volume control needed)
  ac = ac << 3;
  return ac;
}

inline int16_t process_nr_old2(int16_t ac) {
  static int16_t ea1;
  // ea1 = MLEA(ea1, ac, 5, 6); // alpha=0.0156
  ea1 = EA(ea1, ac, 64); // alpha=1/64=0.0156
  // static int16_t ea2;
  // ea2 = EA(ea2, ea1, 64); // alpha=1/64=0.0156

  return ea1;
}

inline int16_t process_nr(int16_t in) {
  static int16_t ea1;
  ea1 = EA(ea1, in, 1 << (nr - 1));
  return ea1;
}

#define N_FILT 7

uint8_t prev_filt[] = {0, 4}; // default filter for modes resp. CW, SSB

inline int16_t filt_var(int16_t za0) // filters build with www.micromodeler.com
{
  static int16_t za1, za2;
  static int16_t zb0, zb1, zb2;
  static int16_t zc0, zc1, zc2;

  if (filt < 4) { // for SSB filters
    // 1st Order (SR=8kHz) IIR in Direct Form I, 8x8:16
    // M0PUB: There was a bug here, since za1 == zz1 at this point in the code,
    // and the old algorithm for the 300Hz high-pass was:
    //    za0=(29*(za0-zz1)+50*za1)/64;
    //    zz2=zz1;
    //    zz1=za0;
    // After correction, this filter still introduced almost 6dB attenuation, so
    // I adjusted the coefficients
    static int16_t zz1, zz2;
    zz2 = zz1;
    zz1 = za0;
    za0 = ((((za0 - zz2) << 5) - ((za0 - zz2) << 1)) +
           (((zz1) << 4) + ((zz1) << 3) + zz1)) >>
          5; // 300-Hz

    switch (filt) {
    case 1:
      zb0 = ((za0 + 2 * za1 + za2) >> 1) - ((((zb1 << 3) + (zb1 << 2) + zb1) +
                                             ((zb2 << 3) + (zb2 << 1) + zb2)) >>
                                            4);
      break; // 0-2900Hz filter, first biquad section
    case 2:
      zb0 = ((za0 + 2 * za1 + za2) >> 1) - (((zb1 << 1) + (zb2 << 3)) >> 4);
      break; // 0-2400Hz filter, first biquad section
    case 3:
      zb0 = ((za0 + 2 * za1 + za2) >> 1) - ((zb2 << 2) >> 4);
      break; // 0-1800Hz  elliptic
    }

    switch (filt) {
    case 1:
      zc0 =
          ((zb0 + 2 * zb1 + zb2) >> 1) -
          ((((zc1 << 4) + (zc1 << 1)) + ((zc2 << 3) + (zc2 << 1) + zc2)) >> 4);
      break; // 0-2900Hz filter, second biquad section
    case 2:
      zc0 = ((zb0 + 2 * zb1 + zb2) >> 2) - (((zc1 << 2) + (zc2 << 3)) >> 4);
      break; // 0-2400Hz filter, second biquad section
    case 3:
      zc0 = ((zb0 + 2 * zb1 + zb2) >> 2) - ((zc2 << 2) >> 4);
      break; // 0-1800Hz  elliptic
    }

    zc2 = zc1;
    zc1 = zc0;

    zb2 = zb1;
    zb1 = zb0;

    za2 = za1;
    za1 = za0;

    return zc0;
  } else { // for CW filters
           //   (2nd Order (SR=4465Hz) IIR in Direct Form I, 8x8:16), adding 64x
           //   front-gain (to deal with later division)
// #define FILTER_700HZ   1
#ifdef FILTER_700HZ
    if (cw_tone == 0) {
      switch (filt) {
      case 4:
        zb0 = (za0 + 2 * za1 + za2) / 2 + (41L * zb1 - 23L * zb2) / 32;
        break; // 500-1000Hz
      case 5:
        zb0 = 5 * (za0 - 2 * za1 + za2) + (105L * zb1 - 58L * zb2) / 64;
        break; // 650-840Hz
      case 6:
        zb0 = 3 * (za0 - 2 * za1 + za2) + (108L * zb1 - 61L * zb2) / 64;
        break; // 650-750Hz
      case 7:
        zb0 = (2 * za0 - 3 * za1 + 2 * za2) + (111L * zb1 - 62L * zb2) / 64;
        break; // 630-680Hz
      }

      switch (filt) {
      case 4:
        zc0 = (zb0 - 2 * zb1 + zb2) / 4 + (105L * zc1 - 52L * zc2) / 64;
        break; // 500-1000Hz
      case 5:
        zc0 = ((zb0 + 2 * zb1 + zb2) + 97L * zc1 - 57L * zc2) / 64;
        break; // 650-840Hz
      case 6:
        zc0 = ((zb0 + zb1 + zb2) + 104L * zc1 - 60L * zc2) / 64;
        break; // 650-750Hz
      case 7:
        zc0 = ((zb1) + 109L * zc1 - 62L * zc2) / 64;
        break; // 630-680Hz
      }
    }
    if (cw_tone == 1)
#endif
    {
      switch (filt) {
      case 4:
        zb0 = (0 * za0 + 1 * za1 + 0 * za2) + (114L * zb1 - 57L * zb2) / 64;
        break; // 600Hz+-250Hz
      case 5:
        zb0 = (0 * za0 + 1 * za1 + 0 * za2) + (113L * zb1 - 60L * zb2) / 64;
        break; // 600Hz+-100Hz
      case 6:
        zb0 = (0 * za0 + 1 * za1 + 0 * za2) + (110L * zb1 - 62L * zb2) / 64;
        break; // 600Hz+-50Hz
      case 7:
        zb0 = (0 * za0 + 1 * za1 + 0 * za2) + (110L * zb1 - 61L * zb2) / 64;
        break; // 600Hz+-18Hz
      }

      switch (filt) {
      case 4:
        zc0 = (zb0 - 2 * zb1 + zb2) / 1 + (95L * zc1 - 52L * zc2) / 64;
        break; // 600Hz+-250Hz
      case 5:
        zc0 = (zb0 - 2 * zb1 + zb2) / 4 + (106L * zc1 - 59L * zc2) / 64;
        break; // 600Hz+-100Hz
      case 6:
        zc0 = (zb0 - 2 * zb1 + zb2) / 16 + (113L * zc1 - 62L * zc2) / 64;
        break; // 600Hz+-50Hz
      case 7:
        zc0 = (zb0 - 2 * zb1 + zb2) / 32 + (112L * zc1 - 62L * zc2) / 64;
        break; // 600Hz+-18Hz
      }
    }
    zc2 = zc1;
    zc1 = zc0;

    zb2 = zb1;
    zb1 = zb0;

    za2 = za1;
    za1 = za0;

    // return zc0 / 64; // compensate the 64x front-end gain
    return zc0 / 8; // compensate the front-end gain
  }
}

#define __UA 256
inline int16_t _arctan3(int16_t q, int16_t i) {
#define __atan2(z)                                                             \
  (__UA / 8 + __UA / 22) *                                                     \
      z // very much of a simplification...not accurate at all, but fast
  // #define __atan2(z)  (__UA/8 - __UA/22 * z + __UA/22) * z  //derived from
  // (5) [1]
  int16_t r;
  if (abs(q) > abs(i))
    r = __UA / 4 - __atan2(abs(i) / abs(q)); // arctan(z) = 90-arctan(1/z)
  else
    r = (i == 0) ? 0 : __atan2(abs(q) / abs(i)); // arctan(z)
  r = (i < 0) ? __UA / 2 - r : r;                // arctan(-z) = -arctan(z)
  return (q < 0) ? -r : r;                       // arctan(-z) = -arctan(z)
}

static uint32_t absavg256 = 0;
volatile uint32_t _absavg256 = 0;
volatile int16_t i, q;

inline int16_t slow_dsp(int16_t ac) {
  static uint8_t absavg256cnt;
  if (!(absavg256cnt--)) {
    _absavg256 = absavg256;
    absavg256 = 0;
  } else
    absavg256 += abs(ac);

  if (mode == AM) {
    ac = magn(i, q);
    {
      static int16_t dc; // DC decoupling
      dc += (ac - dc) / 2;
      ac = ac - dc;
    }
  } else if (mode == FM) {
    static int16_t zi;
    ac = ((ac + i) * zi); // -qh = ac + i
    zi = i;
  } // needs: p.12
    // https://www.veron.nl/wp-content/uploads/2014/01/FmDemodulator.pdf
  else {
    ;
  } // USB, LSB, CW

#ifdef FAST_AGC
  if (agc == 2) {
    ac = process_agc(ac);
    ac = ac >> (16 - volume);
  } else if (agc == 1) {
    ac = process_agc_fast(ac);
    ac = ac >> (16 - volume);
#else
  if (agc == 1) {
    ac = process_agc_fast(ac);
    ac = ac >> (16 - volume);
#endif //! FAST_AGC
  } else {
    // ac = ac >> (16-volume);
    if (volume <= 13) // if no AGC allow volume control to boost weak signals
      ac = ac >> (13 - volume);
    else
      ac = ac << (volume - 13);
  }
  if (nr)
    ac = process_nr(ac);

  if (filt)
    ac = filt_var(ac);

#ifdef CW_DECODER
  if (!(absavg256cnt % 64)) {
    _amp32 = amp32;
    amp32 = 0;
  } else
    amp32 += abs(ac);
#endif // CW_DECODER
  ac = min(max(ac, -512), 511);
#ifdef QCX
  if (!dsp_cap)
    return 0; // in QCX-SSB mode (no DSP), slow_dsp() should return 0 (in order
              // to prevent upsampling filter to generate audio)
#endif
  return ac;
}

#ifdef TESTBENCH
// Sine table with 72 entries results in 868Hz sine wave at effective sampling
// rate of 31250 SPS for each of I and Q, since thay are sampled alternately,
// and hence I (for example) only gets 36 samples from this table before
// looping.
const int8_t sine[] = {
    11,   22,   33,   43,   54,   64,   73,   82,   90,   97,   104,  110,
    115,  119,  123,  125,  127,  127,  127,  125,  123,  119,  115,  110,
    104,  97,   90,   82,   73,   64,   54,   43,   33,   22,   11,   0,
    -11,  -22,  -33,  -43,  -54,  -64,  -73,  -82,  -90,  -97,  -104, -110,
    -115, -119, -123, -125, -127, -127, -127, -125, -123, -119, -115, -110,
    -104, -97,  -90,  -82,  -73,  -64,  -54,  -43,  -33,  -22,  -11,  0};

uint8_t ncoIdx = 0;
int16_t NCO_Q() {
  ncoIdx++;
  if (ncoIdx >= sizeof(sine))
    ncoIdx = 0;
  return (int16_t(sine[ncoIdx])) << 2;
}

int16_t NCO_I() {
  uint8_t i;

  ncoIdx++;
  if (ncoIdx >= sizeof(sine))
    ncoIdx = 0;

  i = ncoIdx + (sizeof(sine) / 4); // Advance by 90 degrees
  if (i >= sizeof(sine))
    i -= sizeof(sine);
  return (int16_t(sine[i])) << 2;
}
#endif // TESTBENCH

volatile uint8_t cat_streaming = 0;
volatile uint8_t _cat_streaming = 0;

typedef void (*func_t)(void);
volatile func_t func_ptr;
#undef R    // Decimating 2nd Order CIC filter
#define R 4 // Rate change from 62500/2 kSPS to 7812.5SPS, providing 12dB gain

// #define SIMPLE_RX  1
#ifndef SIMPLE_RX
volatile uint8_t admux[3];
volatile int16_t ocomb, qh;
volatile uint8_t rx_state = 0;

#pragma GCC push_options
#pragma GCC optimize("Ofast") // compiler-optimization for speed

#define NEW_RX                                                                 \
  1 // Faster (3rd-order) CIC stage, with simultanuous processing capability
#ifdef NEW_RX
#define AF_OUT                                                                 \
  1 // Enables audio output stage (can be disabled in conjunction with
    // CAT_STREAMING to safe memory)

static uint8_t tc = 0;
void process(int16_t i_ac2, int16_t q_ac2) {
  static int16_t ac3;
#ifdef CAT_STREAMING
  if (cat_streaming) {
    uint8_t out = ac3 + 128;
    if (out == ';')
      out++;
    Serial.write(out);
  } // UDR0 = (uint8_t)(ac3 + 128);   // from:
    // https://www.xanthium.in/how-to-avr-atmega328p-microcontroller-usart-uart-embedded-programming-avrgcc
#endif // CAT_STREAMING
#ifdef AF_OUT
  static int16_t ozd1, ozd2; // Output stage
  if (_init) {
    ac3 = 0;
    ozd1 = 0;
    ozd2 = 0;
    _init = 0;
  } // hack: on first sample init accumlators of further stages (to prevent
    // instability)
  int16_t od1 = ac3 - ozd1; // Comb section
  ocomb = od1 - ozd2;
#endif // AF_OUT
#define OUTLET 1
#ifdef OUTLET
  if (tc++ == 0) // prevent recursion
#endif
    interrupts(); // hack, since slow_dsp process exceeds rx sample-time, allow
                  // subsequent 7 interrupts for further rx sampling while
                  // processing, prevent nested interrupts with tc
#ifdef AF_OUT
  ozd2 = od1;
  ozd1 = ac3;
#endif // AF_OUT
  int16_t qh;
  {
    q_ac2 >>= att2;       // digital gain control
    static int16_t v[14]; // Process Q (down-sampled) samples
    // Hilbert transform, BasicDSP model:  outi= fir(inl,  0, 0, 0, 0, 0,  0, 0,
    // 1,   0, 0,   0, 0,  0, 0, 0, 0); outq = fir(inr, 2, 0, 8, 0, 21, 0, 79,
    // 0, -79, 0, -21, 0, -8, 0, -2, 0) / 128;
    qh = ((v[0] - q_ac2) + (v[2] - v[12]) * 4) / 64 +
         ((v[4] - v[10]) + (v[6] - v[8])) / 8 +
         ((v[4] - v[10]) * 5 - (v[6] - v[8])) / 128 +
         (v[6] - v[8]) /
             2; // Hilbert transform, 43dB side-band rejection in 650..3400Hz
                // (@8kSPS) when used in image-rejection scenario; (Hilbert
                // transform require 4 additional bits)
    // qh = ((v[0] - q_ac2) * 2 + (v[2] - v[12]) * 8 + (v[4] - v[10]) * 21 +
    // (v[6] - v[8]) * 15) / 128 + (v[6] - v[8]) / 2; // Hilbert transform, 40dB
    // side-band rejection in 400..1900Hz (@4kSPS) when used in image-rejection
    // scenario; (Hilbert transform require 5 additional bits)
    v[0] = v[1];
    v[1] = v[2];
    v[2] = v[3];
    v[3] = v[4];
    v[4] = v[5];
    v[5] = v[6];
    v[6] = v[7];
    v[7] = v[8];
    v[8] = v[9];
    v[9] = v[10];
    v[10] = v[11];
    v[11] = v[12];
    v[12] = v[13];
    v[13] = q_ac2;
  }
  i_ac2 >>= att2;      // digital gain control
  static int16_t v[7]; // Post processing I and Q (down-sampled) results
  i = i_ac2;
  q = q_ac2; // tbd: this can be more efficient
  int16_t i = v[0];
  v[0] = v[1];
  v[1] = v[2];
  v[2] = v[3];
  v[3] = v[4];
  v[4] = v[5];
  v[5] = v[6];
  v[6] = i_ac2;            // Delay to match Hilbert transform on Q branch
  ac3 = slow_dsp(-i - qh); // inverting I and Q helps dampening a feedback-loop
                           // between PWM out and ADC inputs
#ifdef OUTLET
  tc--;
#endif
}

static int16_t i_s0za1, i_s0zb0, i_s0zb1, i_s1za1, i_s1zb0, i_s1zb1;
static int16_t q_s0za1, q_s0zb0, q_s0zb1, q_s1za1, q_s1zb0, q_s1zb1, q_ac2;

#define M_SR 1 // CIC N=3
void sdr_rx_00() {
  int16_t ac = sdr_rx_common_i();
  func_ptr = sdr_rx_01;
  int16_t i_s1za0 = (ac + (i_s0za1 + i_s0zb0) * 3 + i_s0zb1) >> M_SR;
  i_s0za1 = ac;
  int16_t ac2 = (i_s1za0 + (i_s1za1 + i_s1zb0) * 3 + i_s1zb1);
  i_s1za1 = i_s1za0;
  process(ac2, q_ac2);
}
void sdr_rx_02() {
  int16_t ac = sdr_rx_common_i();
  func_ptr = sdr_rx_03;
  i_s0zb1 = i_s0zb0;
  i_s0zb0 = ac;
}
void sdr_rx_04() {
  int16_t ac = sdr_rx_common_i();
  func_ptr = sdr_rx_05;
  i_s1zb1 = i_s1zb0;
  i_s1zb0 = (ac + (i_s0za1 + i_s0zb0) * 3 + i_s0zb1) >> M_SR;
  i_s0za1 = ac;
}
void sdr_rx_06() {
  int16_t ac = sdr_rx_common_i();
  func_ptr = sdr_rx_07;
  i_s0zb1 = i_s0zb0;
  i_s0zb0 = ac;
}
void sdr_rx_01() {
  int16_t ac = sdr_rx_common_q();
  func_ptr = sdr_rx_02;
  q_s0zb1 = q_s0zb0;
  q_s0zb0 = ac;
}
void sdr_rx_03() {
  int16_t ac = sdr_rx_common_q();
  func_ptr = sdr_rx_04;
  q_s1zb1 = q_s1zb0;
  q_s1zb0 = (ac + (q_s0za1 + q_s0zb0) * 3 + q_s0zb1) >> M_SR;
  q_s0za1 = ac;
}
void sdr_rx_05() {
  int16_t ac = sdr_rx_common_q();
  func_ptr = sdr_rx_06;
  q_s0zb1 = q_s0zb0;
  q_s0zb0 = ac;
}
void sdr_rx_07() {
  int16_t ac = sdr_rx_common_q();
  func_ptr = sdr_rx_00;
  int16_t q_s1za0 = (ac + (q_s0za1 + q_s0zb0) * 3 + q_s0zb1) >> M_SR;
  q_s0za1 = ac;
  q_ac2 = (q_s1za0 + (q_s1za1 + q_s1zb0) * 3 + q_s1zb1);
  q_s1za1 = q_s1za0;
}
// */

static int16_t ozi1, ozi2;

inline int16_t sdr_rx_common_q() {
  ADMUX = admux[0];
  ADCSRA |= (1 << ADSC);
  int16_t ac = ADC - 511;
  return ac;
}

inline int16_t sdr_rx_common_i() {
  ADMUX = admux[1];
  ADCSRA |= (1 << ADSC);
  int16_t adc = ADC - 511;
  static int16_t prev_adc;
  int16_t ac = (prev_adc + adc) / 2;
  prev_adc = adc;
#ifdef AF_OUT
  if (_init) {
    ocomb = 0;
    ozi1 = 0;
    ozi2 = 0;
  } // hack
  ozi2 = ozi1 + ozi2; // Integrator section
  ozi1 = ocomb + ozi1;
  OCR1AL = min(max((ozi2 >> 5) + 128, 0), 255);
#endif // AF_OUT
  return ac;
}


#else // OLD_RX    //Orginal 2nd-order CIC:
// #define M4  1  // Enable to enable M=4 on second-stage (better alias
// rejection)

void sdr_rx() {
  // process I for even samples  [75% CPU@R=4;Fs=62.5k] (excluding the Comb
  // branch and output stage)
  ADMUX = admux[1];        // set MUX for next conversion
  ADCSRA |= (1 << ADSC);   // start next ADC conversion
  int16_t adc = ADC - 511; // current ADC sample 10-bits analog input, NOTE:
                           // first ADCL, then ADCH
  func_ptr = sdr_rx_q;     // processing function for next conversion
  sdr_rx_common();

  // Only for I: correct I/Q sample delay by means of linear interpolation
  static int16_t prev_adc;
  int16_t corr_adc = (prev_adc + adc) / 2;
  prev_adc = adc;
  adc = corr_adc;

  int16_t ac = adc;

  int16_t ac2;
  static int16_t z1;
  if (rx_state == 0 || rx_state == 4) { // 1st stage: down-sample by 2
    static int16_t za1;
    int16_t _ac = ac + za1 + z1 * 2; // 1st stage: FA + FB
    za1 = ac;
    static int16_t _z1;
    if (rx_state == 0) { // 2nd stage: down-sample by 2
      static int16_t _za1;
      ac2 = _ac + _za1 + _z1 * 2; // 2nd stage: FA + FB
      _za1 = _ac;
      {
        ac2 >>= att2; // digital gain control
        // post processing I and Q (down-sampled) results
        static int16_t v[7];
        i = v[0];
        v[0] = v[1];
        v[1] = v[2];
        v[2] = v[3];
        v[3] = v[4];
        v[4] = v[5];
        v[5] = v[6];
        v[6] = ac2; // Delay to match Hilbert transform on Q branch

        int16_t ac = i + qh;
        ac = slow_dsp(ac);

        // Output stage
        static int16_t ozd1, ozd2;
        if (_init) {
          ac = 0;
          ozd1 = 0;
          ozd2 = 0;
          _init = 0;
        } // hack: on first sample init accumlators of further stages (to
          // prevent instability)
#define SECOND_ORDER_DUC 1
#ifdef SECOND_ORDER_DUC
        int16_t od1 = ac - ozd1; // Comb section
        ocomb = od1 - ozd2;
        ozd2 = od1;
#else
        ocomb = ac - ozd1; // Comb section
#endif
        ozd1 = ac;
      }
    } else
      _z1 = _ac;
  } else
    z1 = ac;

  rx_state++;
}

void sdr_rx_q() {
  // process Q for odd samples  [75% CPU@R=4;Fs=62.5k] (excluding the Comb
  // branch and output stage)
#ifdef TESTBENCH
  int16_t adc = NCO_Q();
#else
  ADMUX = admux[0];        // set MUX for next conversion
  ADCSRA |= (1 << ADSC);   // start next ADC conversion
  int16_t adc = ADC - 511; // current ADC sample 10-bits analog input, NOTE:
                           // first ADCL, then ADCH
#endif
  func_ptr = sdr_rx; // processing function for next conversion
#ifdef SECOND_ORDER_DUC
//  sdr_rx_common();  //necessary? YES!... Maybe NOT!
#endif

  int16_t ac = adc;

  int16_t ac2;
  static int16_t z1;
  if (rx_state == 3 || rx_state == 7) { // 1st stage: down-sample by 2
    static int16_t za1;
    int16_t _ac = ac + za1 + z1 * 2; // 1st stage: FA + FB
    za1 = ac;
    static int16_t _z1;
    if (rx_state == 7) { // 2nd stage: down-sample by 2
      static int16_t _za1;
      ac2 = _ac + _za1 + _z1 * 2; // 2nd stage: FA + FB
      _za1 = _ac;
      {
        ac2 >>= att2; // digital gain control
        // Process Q (down-sampled) samples
        static int16_t v[14];
        q = v[7];
        // Hilbert transform, BasicDSP model:  outi= fir(inl,  0, 0, 0, 0, 0, 0,
        // 0, 1,   0, 0,   0, 0,  0, 0, 0, 0); outq = fir(inr, 2, 0, 8, 0, 21,
        // 0, 79, 0, -79, 0, -21, 0, -8, 0, -2, 0) / 128;
        qh = ((v[0] - ac2) + (v[2] - v[12]) * 4) / 64 +
             ((v[4] - v[10]) + (v[6] - v[8])) / 8 +
             ((v[4] - v[10]) * 5 - (v[6] - v[8])) / 128 +
             (v[6] - v[8]) /
                 2; // Hilbert transform, 43dB side-band rejection in
                    // 650..3400Hz (@8kSPS) when used in image-rejection
                    // scenario; (Hilbert transform require 4 additional bits)
        // qh = ((v[0] - ac2) * 2 + (v[2] - v[12]) * 8 + (v[4] - v[10]) * 21 +
        // (v[6] - v[8]) * 15) / 128 + (v[6] - v[8]) / 2; // Hilbert transform,
        // 40dB side-band rejection in 400..1900Hz (@4kSPS) when used in
        // image-rejection scenario; (Hilbert transform require 5 additional
        // bits)
        for (uint8_t j = 0; j != 13; j++)
          v[j] = v[j + 1];
        v[13] = ac2;
        // v[0] = v[1]; v[1] = v[2]; v[2] = v[3]; v[3] = v[4]; v[4] = v[5]; v[5]
        // = v[6]; v[6] = v[7]; v[7] = v[8]; v[8] = v[9]; v[9] = v[10]; v[10] =
        // v[11]; v[11] = v[12]; v[12] = v[13]; v[13] = ac2;
      }
      rx_state = 0;
      return;
    } else
      _z1 = _ac;
  } else
    z1 = ac;

  rx_state++;
}

inline void sdr_rx_common() {
  static int16_t ozi1, ozi2;
  if (_init) {
    ocomb = 0;
    ozi1 = 0;
    ozi2 = 0;
  } // hack
  // Output stage [25% CPU@R=4;Fs=62.5k]
#ifdef SECOND_ORDER_DUC
  ozi2 = ozi1 + ozi2; // Integrator section
#endif
  ozi1 = ocomb + ozi1;
#ifdef SECOND_ORDER_DUC
  OCR1AL = min(max((ozi2 >> 5) + 128, 0),
               255); // OCR1AL = min(max((ozi2>>5) + ICR1L/2, 0), ICR1L);  //
                     // center and clip wrt PWM working range
#else
  OCR1AL = (ozi1 >> 5) + 128;
  OCR1AL = min(max((ozi1 >> 5) + 128, 0),
               255); // OCR1AL = min(max((ozi2>>5) + ICR1L/2, 0), ICR1L);  //
                     // center and clip wrt PWM working range
#endif
}
#endif // OLD_RX

#endif //! SIMPLE_RX

#ifdef SIMPLE_RX
volatile uint8_t admux[3];
static uint8_t rx_state = 0;

static struct rx {
  int16_t z1;
  int16_t za1;
  int16_t _z1;
  int16_t _za1;
} rx_inst[2];

void sdr_rx() {
  static int16_t ocomb;
  static int16_t qh;

  uint8_t b = !(rx_state & 0x01);
  rx *p = &rx_inst[b];
  uint8_t _rx_state;
  int16_t ac;
  if (b) {                 // rx_state == 0, 2, 4, 6 -> I-stage
    ADMUX = admux[1];      // set MUX for next conversion
    ADCSRA |= (1 << ADSC); // start next ADC conversion
    ac = ADC - 512; // current ADC sample 10-bits analog input, NOTE: first
                    // ADCL, then ADCH

    // sdr_common
    static int16_t ozi1, ozi2;
    if (_init) {
      ocomb = 0;
      ozi1 = 0;
      ozi2 = 0;
    } // hack
// Output stage [25% CPU@R=4;Fs=62.5k]
#define SECOND_ORDER_DUC 1
#ifdef SECOND_ORDER_DUC
    ozi2 = ozi1 + ozi2; // Integrator section
#endif
    ozi1 = ocomb + ozi1;
#ifdef SECOND_ORDER_DUC
    OCR1AL = min(max((ozi2 >> 5) + 128, 0),
                 255); // OCR1AL = min(max((ozi2>>5) + ICR1L/2, 0), ICR1L);  //
                       // center and clip wrt PWM working range
#else
    OCR1AL = (ozi1 >> 5) + 128;
#endif
    // Only for I: correct I/Q sample delay by means of linear interpolation
    static int16_t prev_adc;
    int16_t corr_adc = (prev_adc + ac) / 2;
    prev_adc = ac;
    ac = corr_adc;
    _rx_state = ~rx_state;
  } else {
    ADMUX = admux[0];      // set MUX for next conversion
    ADCSRA |= (1 << ADSC); // start next ADC conversion
    ac = ADC - 512; // current ADC sample 10-bits analog input, NOTE: first
                    // ADCL, then ADCH
    _rx_state = rx_state;
  }

  if (_rx_state &
      0x02) { // rx_state == I: 0, 4  Q: 3, 7  1st stage: down-sample by 2
    int16_t _ac = ac + p->za1 + p->z1 * 2; // 1st stage: FA + FB
    p->za1 = ac;
    if (_rx_state &
        0x04) { // rx_state == I: 0  Q:7   2nd stage: down-sample by 2
      int16_t ac2 = _ac + p->_za1 + p->_z1 * 2; // 2nd stage: FA + FB
      p->_za1 = _ac;
      if (b) {
        // post processing I and Q (down-sampled) results
        ac2 >>= att2; // digital gain control
        // post processing I and Q (down-sampled) results
        static int16_t v[7];
        i = v[0];
        v[0] = v[1];
        v[1] = v[2];
        v[2] = v[3];
        v[3] = v[4];
        v[4] = v[5];
        v[5] = v[6];
        v[6] = ac2; // Delay to match Hilbert transform on Q branch

        int16_t ac = i + qh;
        ac = slow_dsp(ac);

        // Output stage
        static int16_t ozd1, ozd2;
        if (_init) {
          ac = 0;
          ozd1 = 0;
          ozd2 = 0;
          _init = 0;
        } // hack: on first sample init accumlators of further stages (to
          // prevent instability)
#ifdef SECOND_ORDER_DUC
        int16_t od1 = ac - ozd1; // Comb section
        ocomb = od1 - ozd2;
        ozd2 = od1;
#else
        ocomb = ac - ozd1; // Comb section
#endif
        ozd1 = ac;
      } else {
        ac2 >>= att2; // digital gain control
        // Process Q (down-sampled) samples
        static int16_t v[14];
        q = v[7];
        qh = ((v[0] - ac2) * 2 + (v[2] - v[12]) * 8 + (v[4] - v[10]) * 21 +
              (v[6] - v[8]) * 15) /
                 128 +
             (v[6] - v[8]) /
                 2; // Hilbert transform, 40dB side-band rejection in
                    // 400..1900Hz (@4kSPS) when used in image-rejection
                    // scenario; (Hilbert transform require 5 additional bits)
        for (uint8_t j = 0; j != 13; j++)
          v[j] = v[j + 1];
        v[13] = ac2;
      }
    } else
      p->_z1 = _ac;
  } else
    p->z1 = ac; // rx_state == I: 2, 6  Q: 1, 5

  rx_state++;
}
// #pragma GCC push_options
// #pragma GCC optimize ("Ofast")  // compiler-optimization for speed
// #pragma GCC pop_options  // end of DSP section
//  */
#endif // SIMPLE_RX

#endif // DSP_H
