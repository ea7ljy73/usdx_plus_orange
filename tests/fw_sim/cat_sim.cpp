// cat_sim.cpp - simulacion host del cat.h REAL de produccion (copiado por
// run_fw_sim.sh). Verifica la paridad CAT TS-480 con usdx-legazy (4284-4620)
// y, sobre todo, que FA/MD NUNCA tocan SI5351/LPF durante TX (legacy 5676:
// "only change if TX is OFF, prevent simultaneous I2C bus access").
#include "sim_common.h"
#include <Arduino.h>

uint32_t   sim_millis = 1000;
uint8_t    sim_pin[32];
void       sim_on_ms(void) {}
SerialStub Serial;
LcdStub    lcd;

#include "menu.h"
#include "si5351.h"
MenuStub menu;
SI5351   si5351;

// ---- globales del firmware que cat.h espera ----
volatile int32_t  freq = 7074000;
volatile uint8_t  mode = 1; // USB
volatile uint8_t  prev_mode = 0, changedModeCAT = 0;
volatile int32_t  rit = 0;
volatile uint8_t  vfosel = 0;
uint8_t           vfomode[2] = {1, 1};
volatile uint32_t semi_qsk_timeout = 0;
volatile uint8_t  smode = 1;
volatile uint32_t rxend_event = 0;
volatile uint8_t  bandval = 3;
int32_t           vfo[2] = {7074000, 14074000};
volatile uint32_t save_event_time = 0;
volatile uint8_t  tx = 0, vox_tx = 0;

void switch_rxtx(uint8_t e) { tx = e; sim_ev(e ? "TXON;" : "TXOFF;"); }
void vfo_apply(void) { sim_ev("VFOAP;"); }
void vfo_save_current(void) { sim_ev("VFOSV;"); }
void display_vfo(void) { sim_ev("DISP;"); }
void display_vfo_line1(void) { sim_ev("DISP1;"); }
void set_lpf(uint8_t f) { char b[16]; snprintf(b, sizeof b, "LPF%u;", f); sim_ev(b); }
void bandval_align(void) { sim_ev("BAL;"); }

#include "cat_under_test.h"

// envia una trama CAT byte a byte como hace serialEvent() y devuelve la respuesta
static const char* cat(const char* cmd) {
  Serial.reset();
  Serial.feed(cmd);
  while(Serial.available())
    cat_serial_event();
  return Serial.out;
}
static void fresh() {
  freq = 7074000; mode = 1; rit = 0; vfosel = 0; tx = 0; vox_tx = 0;
  vfo[0] = 7074000; vfomode[0] = 1; save_event_time = 0; cat_change = 0;
  menu.state = MENU_MAIN; si5351.iqmsa = 1; cat_active = 0; smode = 1;
  evlog[0] = 0;
}
// true si el log contiene cualquier acceso a hardware RF (SI5351 / reles LPF)
static bool rf_touched() { return HAS("VFOAP;") || HAS("LPF") || HAS("FAST;") || HAS("BULK;"); }

int main() {
  // --- C1: respuestas de lectura identicas a legacy ---
  fresh();
  CHECK(!strcmp(cat("FA;"), "FA00007074000;"), "C1 FA; formato 11 digitos");
  { // trama IF construida con los mismos sprintf que legacy (4539-4547)
    char exp[64], *p = exp;
    p += sprintf(p, "IF%02u%03u%03u%03u", 0u, 7u, 74u, 0u);
    p += sprintf(p, "00000+000000");
    p += sprintf(p, "0000");
    p += sprintf(p, "%d", mode + 1);
    p += sprintf(p, "0000000;");
    CHECK(!strcmp(cat("IF;"), exp), "C1 IF; trama legacy");
  }
  CHECK(strlen(cat("IF;")) == 38, "C1 IF; 38 caracteres");
  CHECK(!strcmp(cat("MD;"), "MD2;"), "C1 MD; modo+1");
  CHECK(!strcmp(cat("ID;"), "ID020;"), "C1 ID; TS-480");
  CHECK(!strcmp(cat("PS;"), "PS1;"), "C1 PS;");
  CHECK(!strcmp(cat("AI;"), "AI0;") && !strcmp(cat("AI0;"), "AI0;"), "C1 AI;/AI0;");
  CHECK(!strcmp(cat("RS;"), "RS0;") && !strcmp(cat("AG0;"), "AG0;"), "C1 RS;/AG0;");
  CHECK(!strcmp(cat("XT1;"), "XT1;") && !strcmp(cat("RT1;"), "RT1;"), "C1 XT1;/RT1;");
  CHECK(!strcmp(cat("FL0;"), "FL0;") && !strcmp(cat("VX1;"), "VX1;"), "C1 FL0;/VX1;");
  CHECK(!strcmp(cat("ZZ;"), "?;"), "C1 comando desconocido -> ?;");

  // --- C2: primer comando activa cat_active y apaga el medidor (legacy 4445) ---
  fresh();
  cat("ID;");
  CHECK(cat_active == 1 && smode == 0, "C2 cat_active=1 y smode=0");

  // --- C3: RC; borra el RIT (legacy 4347/4571). Antes solo respondia a RTC ---
  fresh();
  rit = -350;
  CHECK(!strcmp(cat("RC;"), "RC;") && rit == 0, "C3 RC; pone rit=0 y responde RC;");
  rit = 120;
  cat("RTC;");
  CHECK(rit == 120, "C3 RTC; ya no borra el RIT (no es comando legacy)");

  // --- C4: FA en RX: nada de hardware dentro del comando; se aplica en loop ---
  fresh();
  cat("FA00014074000;");
  CHECK(freq == 14074000 && cat_change == 1, "C4 FA fija freq y marca cambio");
  CHECK(!rf_touched(), "C4 FA no toca RF dentro del comando");
  cat_apply();
  CHECK(HAS("LPF14;") && HAS("BAL;") && HAS("VFOAP;") && HAS("DISP;"), "C4 cat_apply: LPF+bandval+SI5351+display");
  CHECK(vfo[0] == 14074000 && save_event_time != 0 && cat_change == 0, "C4 VFO sigue y se agenda el guardado");
  evlog[0] = 0;
  cat_apply();
  CHECK(evlog[0] == 0, "C4 cat_apply sin cambio pendiente no hace nada");

  // --- C5: FA durante TX (PTT/CAT): RF intacta hasta volver a RX ---
  fresh();
  tx = 1;
  cat("FA00007100000;");
  for(int i = 0; i < 50; i++)
    cat_apply(); // el loop sigue girando en TX
  CHECK(!rf_touched() && cat_change == 1, "C5 FA en TX: sin I2C ni reles mientras tx=1");
  CHECK(freq == 7100000, "C5 FA en TX: freq queda anotada");
  tx = 0;
  cat_apply();
  CHECK(HAS("VFOAP;") && HAS("LPF7;") && cat_change == 0, "C5 al soltar TX se aplica una vez");

  // --- C6: lo mismo con VOX transmitiendo ---
  fresh();
  vox_tx = 1;
  cat("FA00003573000;");
  cat_apply();
  CHECK(!rf_touched(), "C6 FA con vox_tx: sin acceso RF");
  vox_tx = 0;
  cat_apply();
  CHECK(HAS("LPF3;") && HAS("VFOAP;"), "C6 al acabar VOX se aplica");

  // --- C7: MD en TX tampoco toca RF; en RX cambia modo y fuerza reset PLL ---
  fresh();
  tx = 1;
  cat("MD3;");
  cat_apply();
  CHECK(mode == 2 && vfomode[0] == 2 && si5351.iqmsa == 0, "C7 MD3 -> CW, vfomode, iqmsa=0");
  CHECK(!rf_touched(), "C7 MD en TX: sin acceso RF");
  tx = 0;
  cat_apply();
  CHECK(HAS("VFOAP;"), "C7 MD se aplica al volver a RX");
  fresh();
  cat("MD0;");
  CHECK(mode == 0, "C7 MD0 se limita a LSB (no corrompe mode)");
  cat("MD9;");
  CHECK(mode == 4, "C7 MD9 se limita a AM");

  // --- C8: con RIT activo el cambio reaplica el offset (legacy 5712) ---
  fresh();
  rit = 200;
  cat("FA00007050000;");
  cat_apply();
  CHECK(HAS("FAST;") && HAS("BULK;") && si5351.last_fast == 200, "C8 RIT reaplicado tras FA");
  CHECK(strstr(evlog, "VFOAP;") < strstr(evlog, "FAST;"), "C8 orden: freq completa y luego RIT");

  // --- C9: en menu no se redibuja la pantalla principal ---
  fresh();
  menu.state = MENU_SELECT;
  cat("FA00007050000;");
  cat_apply();
  CHECK(HAS("VFOAP;") && !HAS("DISP;"), "C9 en menu aplica RF pero no pinta VFO");

  // --- C10: TX/RX por CAT (legacy 4601-4620) ---
  fresh();
  cat("TX;");
  CHECK(tx == 1 && HAS("TXON;"), "C10 TX; transmite");
  semi_qsk_timeout = 1234;
  CHECK(!strcmp(cat("RX;"), "RX0;") && tx == 0 && semi_qsk_timeout == 0, "C10 RX; recibe, limpia semi-QSK, responde RX0;");
  fresh();
  cat("TX0;"); CHECK(tx == 1, "C10 TX0;");
  tx = 0; cat("TX1;"); CHECK(tx == 1, "C10 TX1;");
  tx = 0; cat("TX2;"); CHECK(tx == 1, "C10 TX2;");

  // --- C11: desbordamiento del buffer responde E; y se recupera ---
  fresh();
  char big[64];
  memset(big, 'A', 45);
  big[45] = 0;
  CHECK(strstr(cat(big), "E;") != NULL, "C11 overrun -> E;");
  CHECK(!strcmp(cat(";ID;") + strlen(Serial.out) - 6, "ID020;"), "C11 se recupera tras overrun");

  // --- C12: bloqueo de display durante la trama (legacy 4438) ---
  fresh();
  sim_millis = 5000;
  rxend_event = 0;
  Serial.reset();
  Serial.feed("I");
  cat_serial_event();
  CHECK(rxend_event == 5010, "C12 rxend_event = millis()+10");

  return SUMMARY();
}
