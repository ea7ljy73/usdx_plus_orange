#include "timer.h"

void timer1_start(uint32_t fs)
{
  TCCR1A = 0;
  TCCR1B = 0;
  TCCR1A |= (1 << COM1A1) | (1 << COM1B1) | (1 << WGM11);
  TCCR1B |= (1 << CS10) | (1 << WGM13) | (1 << WGM12);
  ICR1H = 0x00;
  ICR1L = min(255, F_CPU / fs);
  OCR1AH = 0x00;
  OCR1AL = 0x00;
  OCR1BH = 0x00;
  OCR1BL = 0x00;
}

void timer1_stop()
{
  OCR1AL = 0x00;
  OCR1BL = 0x00;
}

void timer2_start(uint32_t fs)
{
  ASSR &= ~(1 << AS2);
  TCCR2A = 0;
  TCCR2B = 0;
  TCNT2 = 0;
  TCCR2A |= (1 << WGM21);
  TCCR2B |= (1 << CS22);
  TIMSK2 |= (1 << OCIE2A);
  uint8_t ocr = ((F_CPU / 64) / fs) - 1;
  OCR2A = ocr;
}

void timer2_stop()
{
  TIMSK2 &= ~(1 << OCIE2A);
  delay(1);
}
