// cat_sim.cpp - simulacion host del cat.h REAL de produccion (copiado por
// run_fw_sim.sh). Verifica la paridad CAT TS-480 con usdx-legazy (4284-4620),
// que FA/MD NUNCA tocan SI5351/LPF durante TX (legacy 5676: "only change if TX
// is OFF, prevent simultaneous I2C bus access") y los comandos TS-480
// ampliados (AG/GT/RA/NR/RL/PC/MG/KS/VX/PR/RT, FB/FR/FT, RU/RD, SM, EX) contra
// la tabla MENU[] REAL extraida del .ino (gen_cat_menu.py).
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
int16_t           dbm = 0;
uint8_t           OCR2A = 4; // timer2 de muestreo: 4 = RX (62,5 kHz), 64 = TX (4,8 kHz)

// ---- variables de la tabla MENU[] (mismos nombres y tipos que el firmware) ----
volatile int8_t  volume = 12;
volatile uint8_t filt = 0, stepsize = 5, agc = 1, nr = 0, att = 0, att2 = 2;
volatile uint8_t cwdec = 1, semi_qsk = 0, keyer_speed = 25, keyer_mode = 2, keyer_swap = 0;
volatile uint8_t practice = 0, vox = 0, vox_thresh = 4, drive = 4, mic_atten = 0;
volatile uint8_t dig_mode = 0, comp_enable = 0, tx_lowcut = 0, tx_envpre = 0;
volatile uint8_t cw_msg_interval = 5, pwm_min = 0, pwm_max = 128, rx_ph_q = 90;
volatile uint8_t backlight = 8, agc_start = 0, agc_hang = 1;
char             cw_msg[1][48] = {"CQ"};
const char* const mode_label[1] = {0}, *const filt_label[1] = {0}, *const band_label[1] = {0};
const char* const stepsize_label[1] = {0}, *const vfosel_label[1] = {0}, *const offon_label[1] = {0};
const char* const agc_label[1] = {0}, *const att_label[1] = {0}, *const smode_label[1] = {0};
const char* const keyer_mode_label[1] = {0};
void on_mode() { sim_ev("ONMODE;"); }
void on_band() { sim_ev("ONBAND;"); }
void on_vfosel() { freq = vfo[vfosel]; sim_ev("ONVFO;"); } // como el .ino: vfosel ya escrito
void on_rit() { rit = (uint8_t)rit; sim_ev("ONRIT;"); } // .ino: el menu solo escribe el byte bajo
void on_att() { sim_ev("ONATT;"); }
void on_pwm() { sim_ev("ONPWM;"); }
void on_sifxtal() { sim_ev("ONXTAL;"); }
void on_iq() { sim_ev("ONIQ;"); }
void on_keyer_speed() { sim_ev("ONWPM;"); }
void on_keyer_mode() { sim_ev("ONKMODE;"); }
void menu_eeprom_save(uint8_t eslot, const void*, uint8_t) { char b[16]; snprintf(b, sizeof b, "EE%u;", eslot); sim_ev(b); }
void save_menu_eslot(uint8_t eslot) { menu_eeprom_save(eslot, 0, 1); }

void switch_rxtx(uint8_t e) { tx = e; OCR2A = e ? 64 : 4; sim_ev(e ? "TXON;" : "TXOFF;"); }
void vfo_apply(void) { sim_ev("VFOAP;"); }
void vfo_save_current(void) { sim_ev("VFOSV;"); }
void display_vfo(void) { sim_ev("DISP;"); }
void display_vfo_line1(void) { sim_ev("DISP1;"); }
void set_lpf(uint8_t f) { char b[16]; snprintf(b, sizeof b, "LPF%u;", f); sim_ev(b); }
void bandval_align(void) { sim_ev("BAL;"); }

#include "cat_under_test.h"
#include "cat_menu.h" // MENU[] real del .ino

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
  volume = 12; agc = 1; nr = 0; att = 0; drive = 4; mic_atten = 0; keyer_speed = 25;
  vox = 0; comp_enable = 0; vox_thresh = 4; si5351.fxtal = 27000000; dbm = 0;
  vfo[1] = 14074000; cat_ptr = 0; OCR2A = 4; // C12 deja un byte a medias en el buffer
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
  CHECK(!strcmp(cat("RS;"), "RS0;"), "C1 RS;");
  CHECK(!strcmp(cat("XT1;"), "XT1;") && !strcmp(cat("FL0;"), "FL0;"), "C1 XT1;/FL0;");
  // divergencia intencional: legacy hacia eco sin efecto de AG0;/RT1;/VX1;.
  // Ahora son los comandos TS-480 reales (lectura de volumen, RIT, VOX).
  CHECK(!strcmp(cat("AG0;"), "AG0192;"), "C1 AG0; lee el volumen (12 -> 192), ya no es eco");
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

  // --- C13: IF lleva offset/estado del RIT, TX/RX y VFO (TS-480 P3/P4/P8/P10) ---
  fresh();
  rit = -250; vfosel = 1; tx = 1;
  {
    const char* f = cat("IF;");
    CHECK(strlen(f) == 38, "C13 IF; sigue en 38 caracteres");
    CHECK(!strncmp(f + 18, "-0250", 5) && f[23] == '1', "C13 IF: offset -0250 y RIT on");
    CHECK(f[28] == '1' && f[29] == '2' && f[30] == '1', "C13 IF: TX=1, modo USB, VFO B");
  }
  tx = 0; vox_tx = 1;
  CHECK(cat("IF;")[28] == '1', "C13 IF: VOX transmitiendo cuenta como TX");

  // --- C14: AG volumen (esencial): 000-255 <-> 0..16 ---
  fresh();
  CHECK(!strcmp(cat("AG;"), "AG0192;"), "C14 AG; tambien lee");
  CHECK(!strcmp(cat("AG0255;"), "") && volume == 16 && HAS("EE1;"), "C14 AG0255 -> 16, persiste (eslot 1)");
  CHECK(!strcmp(cat("AG0;"), "AG0255;"), "C14 AG0; tras 16 -> 255");
  cat("AG0000;"); CHECK(volume == 0, "C14 AG0000 -> 0 (nunca -1 = apagado)");
  cat("AG0100;"); CHECK(volume == 6, "C14 AG0100 -> 6");
  tx = 1; cat("AG0200;"); CHECK(volume == 13, "C14 AG tambien en TX (sin RF)");
  tx = 0; volume = -1;
  CHECK(!strcmp(cat("AG0;"), "AG0000;"), "C14 volumen -1 se lee 000");

  // --- C15: SM0nnnn: escala TS-480 0..30, S9 = 15 ---
  fresh();
  dbm = -73; CHECK(!strcmp(cat("SM0;"), "SM00015;"), "C15 S9 (-73 dBm) = 0015");
  dbm = -200; CHECK(!strcmp(cat("SM;"), "SM00000;"), "C15 por debajo -> 0000");
  dbm = 0; CHECK(!strcmp(cat("SM0;"), "SM00030;"), "C15 tope -> 0030");
  dbm = -53; CHECK(!strcmp(cat("SM0;"), "SM00020;"), "C15 S9+20 -> 0020");

  // --- C15b: TX por CAT en una pausa de la voz: tx=0 (latch de portadora) pero
  // la ISR sigue en TX -> nada de RF y el IF dice TX
  fresh();
  cat("TX;");
  tx = 0; // ssb(): CARRIER_COMPLETELY_OFF_ON_LOW baja el latch en silencio
  evlog[0] = 0; cat("FA00007100000;"); cat_apply();
  CHECK(!rf_touched() && cat_change, "C15b FA en TX con tx=0 (pausa): RF intacta");
  CHECK(cat("IF;")[28] == '1', "C15b IF: sigue en TX aunque tx=0");
  CHECK(!strcmp(cat("EX0010000" "2;"), "?;"), "C15b EX001 (on_mode) rechazado en la pausa");
  cat("RX;"); cat_apply();
  CHECK(HAS("VFOAP;") && cat("IF;")[28] == '0', "C15b tras RX; se aplica y el IF dice RX");

  // --- C16: FB = VFO no activo, sin conmutar ni tocar RF ---
  fresh();
  CHECK(!strcmp(cat("FB;"), "FB00014074000;"), "C16 FB; lee VFO B");
  cat("FB00010100000;");
  CHECK(vfo[1] == 10100000 && freq == 7074000 && !rf_touched() && save_event_time, "C16 FB graba B sin tocar A ni RF");
  vfosel = 1; CHECK(!strcmp(cat("FB;"), "FB00007074000;"), "C16 con VFO B activo, FB es A");

  // --- C17: FR/FT ---
  fresh();
  CHECK(!strcmp(cat("FR;"), "FR0;") && !strcmp(cat("FT;"), "FT0;"), "C17 FR;/FT; leen VFO A");
  cat("FR1;");
  CHECK(vfosel == 1 && freq == 14074000 && HAS("ONVFO;") && HAS("EE6;") && cat_change, "C17 FR1 conmuta a B y persiste");
  evlog[0] = 0; cat("FR1;"); CHECK(!HAS("ONVFO;"), "C17 FR1 repetido no hace nada");
  CHECK(!strcmp(cat("FT0;"), "?;"), "C17 FT distinto de FR -> ?; (sin split)");
  CHECK(vfo[0] == 7074000 && vfo[1] == 14074000, "C17 conmutar no pisa ninguno de los dos VFO");
  tx = 1; CHECK(!strcmp(cat("FR0;"), "?;") && vfosel == 1, "C17 FR en TX -> ?;");

  // --- C18: RIT: RT0/1, RU/RD, RC ---
  fresh();
  CHECK(!strcmp(cat("RT;"), "RT0;"), "C18 RT; lee off");
  cat("RT1;"); CHECK(rit == 1 && HAS("ONRIT;"), "C18 RT1 enciende (offset 1 Hz, como el menu)");
  cat("RU00250;"); CHECK(rit == 251 && cat_change, "C18 RU00250 sube 250 Hz");
  cat("RD;"); CHECK(rit == 241, "C18 RD; baja 10 Hz");
  cat("RT1;"); CHECK(rit == 241, "C18 RT1 con RIT ya activo conserva el offset");
  CHECK(!strcmp(cat("RT;"), "RT1;"), "C18 RT; lee on");
  cat("RU99999;"); CHECK(rit == 9999, "C18 RU se limita a +9999 como el dial");
  cat("RD99999;"); cat("RD99999;"); CHECK(rit == -9999, "C18 RD se limita a -9999");
  tx = 1; cat_change = 0; evlog[0] = 0;
  cat("RU00010;"); CHECK(rit == -9989 && !rf_touched(), "C18 RU en TX: anotado, RF intacta");
  CHECK(!strcmp(cat("RT0;"), "?;"), "C18 RT0 en TX -> ?; (on_rit toca el SI5351)");
  tx = 0; cat_apply(); CHECK(HAS("VFOAP;") && HAS("FAST;"), "C18 al volver a RX se aplica el RIT");
  CHECK(!strcmp(cat("RC;"), "RC;") && rit == 0 && cat_change, "C18 RC; borra y reaplica");

  // --- C19: parametros del menu con escala TS-480 ---
  fresh();
  CHECK(!strcmp(cat("GT;"), "GT001;"), "C19 GT; AGC Fast = 001");
  cat("GT002;"); CHECK(agc == 2, "C19 GT002 -> Slow");
  CHECK(!strcmp(cat("GT003;"), "?;") && agc == 2, "C19 GT003 fuera de rango -> ?;");
  att = 3; CHECK(!strcmp(cat("RA;"), "RA0300;"), "C19 RA; -> RAnn00");
  cat("RA05;"); CHECK(att == 5 && HAS("ONATT;"), "C19 RA05 -> att 5 (on_att)");
  nr = 5;
  CHECK(!strcmp(cat("NR;"), "NR2;") && !strcmp(cat("RL;"), "RL05;"), "C19 NR; = 2 con nr>=2, RL; = nivel");
  cat("NR2;"); CHECK(nr == 5, "C19 NR2 conserva el nivel");
  cat("NR1;"); CHECK(nr == 1, "C19 NR1 -> 1");
  cat("RL08;"); CHECK(nr == 8, "C19 RL08 -> 8");
  cat("NR0;"); CHECK(nr == 0, "C19 NR0 -> off");
  CHECK(!strcmp(cat("PC;"), "PC050;"), "C19 PC; drive 4 = 050");
  cat("PC100;"); CHECK(drive == 8, "C19 PC100 -> drive 8");
  cat("PC012;"); CHECK(drive == 1, "C19 PC012 -> drive 1");
  cat("PC000;"); CHECK(drive == 0, "C19 PC000 -> drive 0");
  CHECK(!strcmp(cat("MG;"), "MG100;"), "C19 MG; sin atenuacion = 100");
  cat("MG000;"); CHECK(mic_atten == 4, "C19 MG000 -> Mic Atten 4");
  cat("MG050;"); CHECK(mic_atten == 2, "C19 MG050 -> Mic Atten 2");
  CHECK(!strcmp(cat("KS;"), "KS025;"), "C19 KS; 25 wpm");
  cat("KS030;"); CHECK(keyer_speed == 30 && HAS("ONWPM;"), "C19 KS030 -> 30 (loadWPM)");
  CHECK(!strcmp(cat("KS000;"), "?;"), "C19 KS000 fuera de rango -> ?;");
  CHECK(!strcmp(cat("VX;"), "VX0;"), "C19 VX; lee VOX");
  CHECK(!strcmp(cat("VX1;"), "") && vox == 1, "C19 VX1 activa el VOX (silencioso)");
  cat("PR1;"); CHECK(comp_enable == 1, "C19 PR1 -> TX Comp 1");
  comp_enable = 3; cat("PR1;"); CHECK(comp_enable == 3, "C19 PR1 conserva el nivel 3");
  CHECK(!strcmp(cat("PR;"), "PR1;"), "C19 PR; = 1");
  cat("PR0;"); CHECK(comp_enable == 0, "C19 PR0 -> off");

  // --- C20: EX generico sobre MENU[] (nº de menu de la LCD) ---
  fresh();
  CHECK(!strcmp(cat("EX0190000;"), "EX01900004;"), "C20 EX019 lee Noise Gate");
  CHECK(!strcmp(cat("EX0300000;"), "EX03000000;"), "C20 EX030 lee TX Comp");
  evlog[0] = 0; cat("EX03000003;");
  CHECK(comp_enable == 3 && HAS("EE24;"), "C20 EX030=3 graba y persiste (eslot 24)");
  CHECK(!strcmp(cat("EX03000004;"), "?;") && comp_enable == 3, "C20 fuera de rango -> ?; sin efecto");
  CHECK(!strcmp(cat("EX0400000;"), "?;"), "C20 nº de menu inexistente -> ?;");
  CHECK(!strcmp(cat("EX0220000;"), "?;"), "C20 CQ Message (texto) fuera de CAT");
  CHECK(!strcmp(cat("EX01;"), "?;"), "C20 trama corta -> ?;");
  CHECK(!strcmp(cat("EX0300000x;"), "?;") && comp_enable == 3, "C20 valor no numerico -> ?; sin efecto");
  CHECK(!strcmp(cat("GTx;"), "?;") && agc == 1, "C20 GTx no numerico -> ?; sin efecto");
  cat("EX0000000-1;"); CHECK(volume == -1, "C20 valor con signo (volumen -1)");
  CHECK(!strcmp(cat("EX0000000;"), "EX0000000-1;"), "C20 lectura con signo");
  CHECK(!strcmp(cat("EX0250000;"), "EX025000027000000;"), "C20 Ref freq 32 bit");
  cat("EX025000027000123;"); CHECK(si5351.fxtal == 27000123 && HAS("ONXTAL;"), "C20 EX025 recalibra (on_sifxtal)");
  CHECK(!strcmp(cat("EX0340000;"), "EX03400000;"), "C20 EX034 TX EnvPre (fuera de orden en MENU[])");
  evlog[0] = 0; cat("EX0070001;"); CHECK(!HAS("EE"), "C20 mismo valor: sin escritura de EEPROM");
  tx = 1;
  CHECK(!strcmp(cat("EX0010000;"), "EX00100001;"), "C20 lectura en TX permitida");
  CHECK(!strcmp(cat("EX0010000"  "2;"), "?;") && mode == 1, "C20 EX001 (modo, on_mode) en TX -> ?;");
  cat("EX0190000" "9;"); CHECK(vox_thresh == 9, "C20 EX019 en TX permitido (sin on_change)");
  tx = 0; evlog[0] = 0; cat("EX0030000" "5;");
  CHECK(HAS("ONBAND;") && cat_change, "C20 EX003 cambia de banda y refresca en loop");

  return SUMMARY();
}
