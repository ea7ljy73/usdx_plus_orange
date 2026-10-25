// ui_sim.cpp - simulacion host de display_tick(), on_rit() y on_vfosel() REALES (extraidas
// del .ino por gen_ui.py). Verifica el decoder CW en pantalla con el esquema
// de legacy (5180-5192) y la normalizacion del RIT.
#include "sim_common.h"
#include <Arduino.h>

uint32_t   sim_millis = 1000;
uint8_t    sim_pin[32];
void       sim_on_ms(void) {}
SerialStub Serial;
LcdStub    lcd;

#define MENU_MAIN 0
#define STEP_10 8
#define STEP_500 6
struct { uint8_t state = MENU_MAIN; } menu;

volatile uint8_t  mode = 2, cwdec = 1, smode = 1, tx = 0, vox_tx = 0, stepsize = 5;
volatile uint8_t  cw_event = 0;
volatile uint32_t semi_qsk_timeout = 0;
volatile int32_t  rit = 0;
char              cw_line[] = "                ";
volatile uint8_t  eng_adc_en = 0, eng_audio_en = 0; // solo lab
void              eng_tick() { sim_ev("ENG;"); }
void              vfo_apply() { sim_ev("VFOAP;"); }

// medidor: pinta en las MISMAS columnas que el smeter() real (legacy 3577-3592)
static int smeter_calls = 0;
static int16_t smeter(int16_t = 0) {
  smeter_calls++;
  if(smode == 1) { lcd.setCursor(9, 0); lcd.print("-73dBm "); }
  if(smode == 2) { lcd.setCursor(14, 0); lcd.print("S9"); }
  if(smode == 3) { lcd.setCursor(12, 0); lcd.print("####"); }
  if(smode == 4) { lcd.setCursor(14, 0); lcd.print("25"); }
  return 0;
}

// para on_vfosel() (VFO A/B)
#define STEP_1k 5
#define CW 2
volatile int32_t  freq = 7074000;
volatile uint8_t  vfosel = 0, filt = 0, nr = 0;
int32_t           vfo[2] = {7074000, 14074000};
uint8_t           vfomode[2] = {1, 2};
volatile uint32_t save_event_time = 0;
void              bandval_align() { sim_ev("BAL;"); }

#include "ui_under_test.h"

static const uint8_t legacy_off[] = {0, 7, 3, 5, 3}; // usdx-legazy 5181
static const uint8_t meter_col[] = {16, 9, 14, 12, 14};

int main() {
  const char* text = "ABCDEFGHIJKLMNOP";
  for(uint8_t sm = 0; sm <= 4; sm++) {
    char name[96];
    smode = sm;
    lcd.clear();
    strcpy(cw_line, text);
    cw_event     = 1;
    smeter_calls = 0;
    display_tick();
    uint8_t off = legacy_off[sm], n = 16 - off;
    snprintf(name, sizeof name, "U1 smode=%u: texto CW desde col 0 con offset legacy %u", sm, off);
    CHECK(!strncmp(lcd.row[0], text + off, n), name);
    snprintf(name, sizeof name, "U1 smode=%u: el texto acaba antes del medidor (col %u)", sm, meter_col[sm]);
    CHECK(n <= meter_col[sm], name);
    snprintf(name, sizeof name, "U1 smode=%u: evento consumido y el medidor vuelve a vivir", sm);
    display_tick();
    CHECK(cw_event == 0 && smeter_calls == 1, name);
    snprintf(name, sizeof name, "U1 smode=%u: el medidor no pisa el texto", sm);
    CHECK(!strncmp(lcd.row[0], text + off, n), name);
  }
  // el ultimo caracter decodificado (derecha del buffer) siempre queda visible
  smode = 1; lcd.clear(); strcpy(cw_line, "               K"); cw_event = 1; display_tick();
  CHECK(lcd.row[0][8] == 'K', "U2 el caracter mas reciente queda junto al medidor");

  // sin evento: medidor salvo en semi-QSK / VOX (legacy 5195)
  smeter_calls = 0; semi_qsk_timeout = 0; vox_tx = 0; display_tick();
  CHECK(smeter_calls == 1, "U3 sin evento CW pinta el medidor");
  smeter_calls = 0; semi_qsk_timeout = 5000; display_tick();
  CHECK(smeter_calls == 0, "U3 en semi-QSK no pinta el medidor");
  semi_qsk_timeout = 0; vox_tx = 1; smeter_calls = 0; display_tick();
  CHECK(smeter_calls == 0, "U3 con VOX transmitiendo no pinta el medidor");
  vox_tx = 0;
#if UI_PROD
  // CAT (smode 0): se calcula el medidor (SM;) pero la LCD no se toca: cada
  // escritura apaga la UART (pines compartidos) y perdia bytes de CAT
  smode = 0; smeter_calls = 0; lcd.setCursor(3, 0); display_tick();
  CHECK(smeter_calls == 1 && lcd.x == 3 && lcd.y == 0, "U5 smode=0: calcula el medidor sin escribir en la LCD");
  smode = 1;
#endif

  // RIT: el menu (P_ENUM) solo escribe el byte bajo de un int32
  rit = -500; *(uint8_t*)&rit = 1; evlog[0] = 0; on_rit();
  CHECK(rit == 1 && stepsize == STEP_10 && HAS("VFOAP;"), "U4 RIT ON desde offset negativo queda en 1 (no -511)");
  rit = -500; *(uint8_t*)&rit = 0; on_rit();
  CHECK(rit == 0 && stepsize == STEP_500, "U4 RIT OFF desde offset negativo queda en 0 (no -512)");
  rit = 0; *(uint8_t*)&rit = 1; on_rit();
  CHECK(rit == 1 && stepsize == STEP_10, "U4 RIT ON normal");

  // VFO A/B desde el menu: edit_value() escribe vfosel y llama a on_change, y
  // commit() lo llama otra vez. Antes on_vfosel() volvia a conmutar y pisaba
  // el otro VFO con la frecuencia del actual.
  mode = 1; freq = 7074000; vfosel = 1; evlog[0] = 0;
  on_vfosel(); on_vfosel(); // edicion + confirmacion
  CHECK(freq == 14074000 && mode == 2 && vfo[0] == 7074000 && vfo[1] == 14074000, "U6 menu VFO A->B: carga B y no pisa ningun VFO");
  CHECK(filt == 4 && nr == 0 && HAS("VFOAP;") && save_event_time, "U6 en CW: filtro 500, NR off, reprograma y guarda");
  vfosel = 0; on_vfosel(); on_vfosel();
  CHECK(freq == 7074000 && mode == 1 && vfo[1] == 14074000, "U6 menu VFO B->A: vuelve a A intacto");

  return SUMMARY();
}
