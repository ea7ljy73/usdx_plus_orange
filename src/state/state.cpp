#include "state.h"
#include "../hal/gpio.h"
#include "../drivers/si5351.h"
#include "../drivers/lpf_switch.h"

void state_init()
{
  _init = 1;
  vfo[0] = band_freqs[band] * 1000UL;
  vfo[1] = vfo[0];
  vfomode[0] = mode;
  vfomode[1] = mode;
  volume = DEFAULT_VOLUME;
  keyer_speed = DEFAULT_KEYER_SPEED;
  agc = DEFAULT_AGC;
  nr = DEFAULT_NR;
  drive = DEFAULT_DRIVE;
  filt = 0;
  rit = 0;
  cw_tone = 1;
  cw_offset = 700;
}

void state_reset()
{
  _init = 1;
}

void bandswitch(uint8_t _band)
{
  if((_band >= N_BANDS) && (_band != 255))
    return;

  if(_band != 255){
    band = _band;
    freq = band_freqs[band] * 1000UL;
    vfo[0] = freq;
    vfo[1] = freq;
  }
  frequency_update(freq);
  lpf::set_by_index(band);
}

void mode_switch(uint8_t _mode)
{
  mode = _mode;
  if(!ft8mode){
    if(mode == CW){
      filt = 4;
    } else {
      filt = 0;
    }
  }
  frequency_update(freq);
}

void frequency_update(int32_t _f)
{
  freq = _f;
  si5351.freq(freq + rit + cw_offset, 0, 90);
}

void vfo_update()
{
  if(vfosel == VFOA){
    freq = vfo[0];
    mode = vfomode[0];
  } else if(vfosel == VFOB){
    freq = vfo[1];
    mode = vfomode[1];
  } else if(vfosel == SPLIT){
    if(tx){
      freq = vfo[1];
      mode = vfomode[1];
    } else {
      freq = vfo[0];
      mode = vfomode[0];
    }
  }
  frequency_update(freq);
}

void state_update()
{
  if(vfosel != SPLIT){
    if(vfosel == VFOA){
      vfo[0] = freq;
      vfomode[0] = mode;
    } else {
      vfo[1] = freq;
      vfomode[1] = mode;
    }
  } else {
    if(tx){
      vfo[1] = freq;
      vfomode[1] = mode;
    } else {
      vfo[0] = freq;
      vfomode[0] = mode;
    }
  }
}

void state_save_all()
{
  EEPROM.update(EEPROM_OFFSET + VOLUME, volume);
  EEPROM.update(EEPROM_OFFSET + MODE, mode);
  EEPROM.update(EEPROM_OFFSET + FILTER, filt);
  EEPROM.update(EEPROM_OFFSET + BAND, band);
  EEPROM.update(EEPROM_OFFSET + RIT, rit);
  EEPROM.update(EEPROM_OFFSET + AGC, agc);
  EEPROM.update(EEPROM_OFFSET + NR, nr);
  EEPROM.update(EEPROM_OFFSET + CWTONE, cw_tone);
  EEPROM.update(EEPROM_OFFSET + CWSPEED, keyer_speed);
  EEPROM.update(EEPROM_OFFSET + CWSWAP, keyer_swap);
  EEPROM.update(EEPROM_OFFSET + DRIVE, drive);
  EEPROM.update(EEPROM_OFFSET + VOX, vox);
  EEPROM.update(EEPROM_OFFSET + IQ_ADJ, 0);
  EEPROM.update(EEPROM_OFFSET + BACKL, 0);
  EEPROM.update(EEPROM_OFFSET + FREQA + 0, (vfo[0] >> 0) & 0xFF);
  EEPROM.update(EEPROM_OFFSET + FREQA + 1, (vfo[0] >> 8) & 0xFF);
  EEPROM.update(EEPROM_OFFSET + FREQA + 2, (vfo[0] >> 16) & 0xFF);
  EEPROM.update(EEPROM_OFFSET + FREQB + 0, (vfo[1] >> 0) & 0xFF);
  EEPROM.update(EEPROM_OFFSET + FREQB + 1, (vfo[1] >> 8) & 0xFF);
  EEPROM.update(EEPROM_OFFSET + FREQB + 2, (vfo[1] >> 16) & 0xFF);
  EEPROM.update(EEPROM_OFFSET + MODEA, vfomode[0]);
  EEPROM.update(EEPROM_OFFSET + MODEB, vfomode[1]);
}

void state_load_all()
{
  volume = EEPROM.read(EEPROM_OFFSET + VOLUME);
  mode = EEPROM.read(EEPROM_OFFSET + MODE);
  filt = EEPROM.read(EEPROM_OFFSET + FILTER);
  band = EEPROM.read(EEPROM_OFFSET + BAND);
  rit = EEPROM.read(EEPROM_OFFSET + RIT);
  agc = EEPROM.read(EEPROM_OFFSET + AGC);
  nr = EEPROM.read(EEPROM_OFFSET + NR);
  cw_tone = EEPROM.read(EEPROM_OFFSET + CWTONE);
  keyer_speed = EEPROM.read(EEPROM_OFFSET + CWSPEED);
  keyer_swap = EEPROM.read(EEPROM_OFFSET + CWSWAP);
  drive = EEPROM.read(EEPROM_OFFSET + DRIVE);
  vox = EEPROM.read(EEPROM_OFFSET + VOX);
  uint8_t iq_adj = EEPROM.read(EEPROM_OFFSET + IQ_ADJ);
  uint8_t backl = EEPROM.read(EEPROM_OFFSET + BACKL);
  vfo[0] = ((uint32_t)EEPROM.read(EEPROM_OFFSET + FREQA + 0)) |
           ((uint32_t)EEPROM.read(EEPROM_OFFSET + FREQA + 1) << 8) |
           ((uint32_t)EEPROM.read(EEPROM_OFFSET + FREQA + 2) << 16);
  vfo[1] = ((uint32_t)EEPROM.read(EEPROM_OFFSET + FREQB + 0)) |
           ((uint32_t)EEPROM.read(EEPROM_OFFSET + FREQB + 1) << 8) |
           ((uint32_t)EEPROM.read(EEPROM_OFFSET + FREQB + 2) << 16);
  vfomode[0] = EEPROM.read(EEPROM_OFFSET + MODEA);
  vfomode[1] = EEPROM.read(EEPROM_OFFSET + MODEB);

  if(band >= N_BANDS) band = 0;
  if(mode > AM) mode = USB;
  if(filt > 7) filt = 0;

  freq = vfo[0];
  frequency_update(freq);
  lpf::set_by_index(band);
}
