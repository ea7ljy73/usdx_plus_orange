// main_menu_sim.cpp - host simulation of the REAL menu.h button engine + menu
// machine (copied fresh by run_menu.sh: usdx-lab/menu.h, o el de produccion
// con -DPROD; unica diferencia esperada: produccion entra siempre por la fila
// 0 como legacy 5343, el lab recuerda la ultima fila). Scripted button/ADC/
// encoder timelines, asserts on states, values and side effects.
//
// States: MENU_MAIN=0, SELECT=1, EDIT=2. Buttons: BL=left, BR=right, BE=dial.
// ADC ladder: BL<860, BR<983, else BE (4.2V/4.8V @5Vref, uint16 truncation).
#include <stdio.h>
#include <string.h>
#define _init menu_init_flag // evita choque con _init de la libc host
#include "menu_under_test.h"

// ---- sim time/pins (stub Arduino.h externs) ----
uint32_t sim_millis = 1000;
uint8_t  sim_buttons_digital = 0;
uint16_t sim_buttons_adc = 0;
int      sim_analog_reads = 0;
LcdStub  lcd;

// ---- firmware globals owned by the driver ----
volatile uint8_t  mode = USB;
volatile int8_t   volume = 12;
volatile int32_t  rit = 0;
volatile uint8_t  nr = 0, filt = 0, bandval = 3, stepsize = 4, cwdec = 0;
volatile uint8_t  vfosel = 0, menu_init_flag = 0;
volatile int32_t  freq = 7074000;
int32_t           vfo[2] = {7074000, 14074000};
uint8_t           vfomode[2] = {USB, USB};
uint8_t           prev_stepsize[2] = {4, 4};
uint8_t           prev_filt[2] = {0, 0};
volatile uint32_t save_event_time = 0;
volatile uint8_t  pwm_min = 0, pwm_max = 128;
volatile int8_t   last_tune_dir = 1;
SI5351            si5351;
Menu              menu;

// test menu: Volume / NR / RIT(int32!) / Engineer
static volatile uint8_t  t_volume = 12;
static volatile uint8_t  t_nr = 0;
static volatile int32_t  t_rit = 0;
static volatile uint8_t  t_eng = 0;
volatile uint8_t          eng_adc_en = 0, eng_audio_en = 0;
static const char         l0[] = "Volume";
static const char         l1[] = "NR";
static const char         l2[] = "RIT";
static const char         l3[] = "Engineer";
static const char         e0[] = "OFF";
static const char         e1[] = "ON";
static const char         e2[] = "ADC";
static const char         e3[] = "AUDIO";
static const char* const  offon[] = {e0, e1};
static const char* const  eng3[] = {e0, e2, e3};
static const char* const  ML[] = {l0, l1, l2, l3};
const MenuParam MENU[] = {
  {0, (void*)&t_volume, P_T8S, -1, 16, NULL, 1, NULL},
  {1, (void*)&t_nr, P_T8, 0, 8, NULL, 9, NULL},
  {2, (void*)&t_rit, P_ENUM, 0, 1, offon, 7, NULL},
  {3, (void*)&t_eng, P_ENUM, 0, 2, eng3, 35, NULL},
  {4, (void*)&eng_audio_en, P_ENUM, 0, 1, offon, 36, NULL},
};
const int8_t            MENU_COUNT = 5;
const char* const       MENU_LABELS[] = {l0, l1, l2, l3};
void menu_print_label(uint8_t id) { lcd.print(ML[id]); }

// ---- side-effect recorders ----
static char evlog[4096];
static void ev(const char* s) {
  strncat(evlog, s, sizeof(evlog) - strlen(evlog) - 1);
}
void stepsize_change(int8_t v) {
  char b[16];
  snprintf(b, sizeof b, "STEP%+d;", (int)v);
  ev(b);
}
void display_vfo(void) { ev("VFO;"); }
void display_vfo_line1(void) { ev("VFO1;"); }
void save_menu_eslot(uint8_t e) {
  char b[16];
  snprintf(b, sizeof b, "SAVE%u;", e);
  ev(b);
}
void show_banner(void) { ev("BAN;"); }
void powerDown(void) { ev("PWR;"); }
void vfo_apply(void) { ev("VFOAP;"); }
void vfo_save_current(void) { ev("VFOSV;"); }
void vfo_recall_band(int8_t) { ev("VFORC;"); }
void bandval_align() { ev("BAL;"); }
void on_band(void) { ev("BAND;"); }
void set_lpf(uint8_t) { ev("LPF;"); }
void menu_eeprom_load(uint8_t, void*, uint8_t) {}
void menu_eeprom_save(uint8_t e, const void*, uint8_t) {
  char b[16];
  snprintf(b, sizeof b, "EE%u;", e);
  ev(b);
}

// ---- timeline helpers (1ms ticks) ----
static void tick(int ms) {
  for(int i = 0; i < ms; i++) {
    menu.process();
    sim_millis++;
  }
}
// button press: adc level, hold ms, then release + settle
static void press(uint16_t adc, int hold_ms, int settle_ms = 60) {
  sim_buttons_digital = 1;
  sim_buttons_adc     = adc;
  tick(hold_ms);
  sim_buttons_digital = 0;
  tick(settle_ms);
}
#define BL 700
#define BR 920
#define BE 1023
static void click(uint16_t b) { press(b, 120); }
// bouncy press: 1ms chatter x6, hold, release
static void bouncy_press(uint16_t adc) {
  for(int i = 0; i < 6; i++) {
    sim_buttons_digital = 1;
    sim_buttons_adc     = adc;
    tick(1);
    sim_buttons_digital = 0;
    tick(1);
  }
  sim_buttons_digital = 1;
  tick(100);
  sim_buttons_digital = 0;
  tick(60);
}
static void enc(int32_t n) {
  encoder_val += (int16_t)n;
  tick(10);
}
static void fresh() {
  menu.begin();
  menu.dc_cancel = false;
  mode = USB;
  volume = 12;
  t_volume = 12;
  t_nr = 0;
  t_rit = 0;
  t_eng = 0;
  evlog[0]  = 0;
  lcd.clear_lines();
  encoder_val = 0;
  tick(400); // settle DC windows / debounce
  evlog[0] = 0;
}

// entra al menu (BL) y se situa en la fila idx
static void enter_at(int8_t idx) {
  menu.index = idx; // el lab recuerda la fila
  click(BL);
#ifdef PROD
  menu.index = idx; // produccion entra por la fila 0: se navega hasta idx
#endif
}

static int fails = 0;
static int ntest = 0;
#define CHECK(c, name) \
  do { \
    ntest++; \
    if(!(c)) { \
      fails++; \
      printf("FAIL %s\n", name); \
    } else \
      printf("ok %s\n", name); \
  } while(0)
#define HAS(s) (strstr(evlog, s) != NULL)

int main() {
  // T1: clean BL click MAIN->SELECT, index kept
  fresh();
  menu.index = 2;
  click(BL);
#ifdef PROD
  CHECK(menu.state == 1 && menu.index == 0, "T1 BL entra a SELECT por la fila 0 (legacy 5343)");
#else
  CHECK(menu.state == 1 && menu.index == 2, "T1 BL entra a SELECT recordando fila");
#endif

  // T2: bouncy press = exactly one event (SELECT, not EDIT)
  fresh();
  bouncy_press(BL);
  CHECK(menu.state == 1, "T2 rebote = un solo evento");

  // T3: full Engineer flow (flujo estandar: EDIT->SELECT->MAIN)
  fresh();
  enter_at(3); // -> SELECT(3)
  CHECK(menu.state == 1 && menu.index == 3, "T3 entra en Engineer");
  click(BL); // -> EDIT
  CHECK(menu.state == 2, "T3 edita");
  enc(1);    // OFF->ADC
  CHECK(t_eng == 1, "T3 dial cambia valor");
  click(BL); // commit -> SELECT
  CHECK(menu.state == 1 && HAS("EE35;"), "T3 guarda eslot 35 y vuelve");
  click(BL); // -> MAIN
  CHECK(menu.state == 0 && HAS("VFO;"), "T3 sale a MAIN");

  // T4: rapid dial double-press in menu: 1st enters EDIT, 2nd commits
  // (DC solo se arma en MAIN: en menu ningun click se pierde)
  fresh();
  enter_at(3); // SELECT
  click(BE); // EDIT
  press(BE, 120); // 2nd press <300ms -> commit normal (sin DC en menu)
  CHECK(menu.state == 1 && !HAS("BAND;"), "T4 doble-click en menu no salta de banda");
  // T4b: doble-click de dial en MAIN si salta de banda (paridad legacy)
  fresh();
  click(BE);
  press(BE, 120);
  CHECK(HAS("BAND;"), "T4b doble-click en MAIN salta de banda");

  // T5: BE hold+turn in EDIT = edit (NO volume, NO powerdown)
  fresh();
  enter_at(3);
  click(BL); // EDIT, eng=0
  sim_buttons_digital = 1;
  sim_buttons_adc     = BE;
  tick(700); // hold past LONG_PRESS
  encoder_val += 2;
  tick(20); // turns while held
  sim_buttons_digital = 0;
  tick(80);
  CHECK(t_eng == 2 && volume == 12 && !HAS("PWR;"), "T5 hold+giro en menu edita, no volumen");

  // T6: ADC threshold knife-edge documents BL/BR/BE mapping
  fresh();
  press(859, 120);
  CHECK(menu.state == 1, "T6 adc=859 es BL");
  fresh();
  press(860, 120);
  CHECK(mode == 2 && menu.state == 0, "T6 adc=860 ya es BR (cambia modo)");
  fresh();
  press(983, 120);
  CHECK(HAS("STEP"), "T6 adc=983 es BE (dial)");

  // T7: slow 700ms press = PL fast-edit
  fresh();
  press(BL, 700);
  CHECK(menu.state == 2, "T7 pulsacion larga entra directo a EDIT");

  // T8: release bounce = single dispatch
  fresh();
  sim_buttons_digital = 1;
  sim_buttons_adc     = BL;
  tick(100);
  for(int i = 0; i < 5; i++) { // chatter on release
    sim_buttons_digital = 0;
    tick(2);
    sim_buttons_digital = 1;
    tick(2);
  }
  sim_buttons_digital = 0;
  tick(80);
  CHECK(menu.state == 1, "T8 rebote al soltar = un evento");

  // T9: move modulo with big deltas
  fresh();
  enter_at(0); // SELECT
  enc(-1);
  CHECK(menu.index == 4, "T9 wrap 0-1=4");
  enc(7);
  CHECK(menu.index == 1, "T9 modulo (4+7)%5=1");

  // T10: P_ENUM wraps both directions, no 253-walk
  fresh();
  enter_at(3);
  click(BL); // EDIT eng=0
  enc(-1);
  CHECK(t_eng == 2, "T10 izquierda 0->2");
  enc(1);
  CHECK(t_eng == 0, "T10 derecha 2->0");

  // T12: flujo estandar con dial: en EDIT el dial guarda y vuelve a SELECT;
  // solo MENU sale a MAIN. Fila Eng conserva valor al salir con dial.
  fresh();
  eng_audio_en = 0;
  enter_at(4); // SELECT
  click(BE); // EDIT (dial)
  enc(1);    // OFF->ON
  CHECK(eng_audio_en == 1, "T12 dial edita Eng");
  tick(350); // fuera de ventana DC
  click(BE); // guardar y volver a SELECT (browser)
  CHECK(menu.state == 1 && eng_audio_en == 1, "T12 dial vuelve a browser conserva");
  click(BL); // MENU sale a MAIN
  CHECK(menu.state == 0 && HAS("VFO;"), "T12 menu sale a MAIN");

  // T11: MAIN PT-volume to -1 = powerdown path exists (documents trap)
  fresh();
  volume = 0;
  sim_buttons_digital = 1;
  sim_buttons_adc     = BE;
  tick(600);
  encoder_val += -1;
  tick(20);
  sim_buttons_digital = 0;
  tick(80);
  CHECK(HAS("PWR;") && volume == 10, "T11 volumen -1 apaga (trampa documentada)");

  printf("\n%d/%d ok\n", ntest - fails, ntest);
  return fails ? 1 : 0;
}
