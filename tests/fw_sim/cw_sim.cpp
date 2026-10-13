// cw_sim.cpp - simulacion host del cw.h REAL de produccion (copiado por
// run_fw_sim.sh): keyer iambico A/B, mensajes CW y decoder, con reloj y
// paletas simuladas. Referencia: usdx-legazy 2243-2483 y 5201-5262.
#include "sim_common.h"
#include <Arduino.h>

uint32_t   sim_millis = 1000;
uint8_t    sim_pin[32];
SerialStub Serial;
LcdStub    lcd;

// ---- grabacion de la portadora: cada flanco de switch_rxtx con su instante ----
static uint8_t  key_now = 0;
static uint32_t key_t0  = 0;
static int      n_el    = 0;
static uint32_t el_on[64], el_gap[64]; // duracion de cada elemento y del hueco previo
static uint32_t last_off = 0;
void switch_rxtx(uint8_t e) {
  if(e && !key_now) {
    key_t0 = sim_millis;
    if(n_el < 64)
      el_gap[n_el] = last_off ? sim_millis - last_off : 0;
  } else if(!e && key_now) {
    if(n_el < 64)
      el_on[n_el++] = sim_millis - key_t0;
    last_off = sim_millis;
  }
  key_now = e;
}

// guion de paletas: se suelta/pulsa un pin en un instante dado (lo usa wdt_reset/delay)
static uint32_t script_t = 0;
static uint8_t  script_pin = 0, script_lvl = 1;
void sim_on_ms(void) {
  if(script_t && sim_millis >= script_t) {
    sim_pin[script_pin] = script_lvl;
    script_t            = 0;
  }
}

volatile uint8_t  keyer_speed = 25, keyer_mode = 0;
volatile uint32_t _amp32 = 0;
uint8_t           inv = 0;

#include "cw_under_test.h"
#include "cw_legacy.h" // decoder REAL de usdx-legazy (gen_cw_legacy.py)

static void keyer_run(int ms) { // loop() a 1 kHz
  for(int i = 0; i < ms; i++) {
    keyer_process();
    sim_millis++;
    sim_on_ms();
  }
}
static void fresh(uint8_t km) {
  memset(sim_pin, 1, sizeof sim_pin); // paletas sueltas (pull-up)
  sim_pin[BUTTONS] = 0;               // boton suelto (inv=0)
  keyer_swap = 0;
  loadWPM(25);
  keyer_set_mode(km);
  key_now = 0; n_el = 0; last_off = 0; script_t = 0;
  cw_msg_event = 0;
}
static bool near(uint32_t v, uint32_t ref) { return v + 2 >= ref && v <= ref + 2; }

// decoder: alimenta dec2() ms a ms con una secuencia de elementos ('.', '-', ' ' letra, '/' palabra)
static void dec_feed(const char* morse, uint32_t dit) {
  for(const char* p = morse; *p; p++) {
    uint32_t on = (*p == '.') ? dit : (*p == '-') ? 3 * dit : 0;
    uint32_t off = (*p == ' ') ? 2 * dit : (*p == '/') ? 6 * dit : dit;
    filteredstate = HIGH;
    for(uint32_t i = 0; i < on; i++) { dec2(); sim_millis++; }
    filteredstate = LOW;
    for(uint32_t i = 0; i < off; i++) { dec2(); sim_millis++; }
  }
  for(uint32_t i = 0; i < 10 * dit; i++) { dec2(); sim_millis++; } // silencio final
}
static void dec_reset() {
  strcpy(cw_line, "                ");
  cw_event = 0; sym = 1; hightimesavg = 0;
  filteredstate = filteredstatebefore = LOW;
  startttimelow = starttimehigh = sim_millis;
}
static bool line_ends(const char* txt) { // texto decodificado al final de la linea (ignora espacios finales)
  char b[17];
  strcpy(b, cw_line);
  int n = 16;
  while(n && b[n - 1] == ' ')
    b[--n] = 0;
  size_t l = strlen(txt);
  return (size_t)n >= l && !strcmp(b + n - l, txt);
}

int main() {
  // --- K1: tiempo de punto a 20 MHz (legacy: 1200*5/4/wpm) ---
  fresh(0);
  CHECK(ditTime == 60, "K1 ditTime 25 wpm = 60 ms de reloj (48 ms reales a 20 MHz)");
  loadWPM(12);
  CHECK(ditTime == 125, "K1 ditTime 12 wpm = 125");

  // --- K2: un punto y una raya con su relacion 1:3 y hueco de 1 punto ---
  fresh(0);
  sim_pin[DIT] = 0; keyer_run(20); sim_pin[DIT] = 1; keyer_run(300);
  CHECK(n_el == 1 && near(el_on[0], 60), "K2 punto = 1 dit");
  fresh(0);
  sim_pin[DAH] = 0; keyer_run(20); sim_pin[DAH] = 1; keyer_run(400);
  CHECK(n_el == 1 && near(el_on[0], 180), "K2 raya = 3 dits");
  fresh(0);
  sim_pin[DIT] = 0; keyer_run(300); sim_pin[DIT] = 1; keyer_run(300);
  CHECK(n_el >= 2 && near(el_on[1], 60) && el_gap[1] >= 60 && el_gap[1] <= 68,
        "K2 puntos repetidos con hueco de 1 dit (+pasos de la maquina de estados a 1 ms/loop)");

  // --- K3: squeeze alterna punto/raya; Iambic B anade el elemento opuesto al soltar ---
  fresh(0);
  sim_pin[DIT] = 0; sim_pin[DAH] = 0; keyer_run(500); sim_pin[DIT] = 1; sim_pin[DAH] = 1; keyer_run(600);
  bool alt = n_el >= 4;
  for(int i = 1; i < n_el && alt; i++)
    alt = near(el_on[i], near(el_on[i - 1], 60) ? 180 : 60);
  CHECK(alt && near(el_on[0], 60), "K3 squeeze alterna punto-raya empezando por punto");
  int na, nb;
  fresh(0); // A: se suelta a mitad de la raya -> termina ahi
  sim_pin[DIT] = 0; sim_pin[DAH] = 0; keyer_run(150); sim_pin[DIT] = 1; sim_pin[DAH] = 1; keyer_run(800);
  na = n_el;
  fresh(1); // B: mismo gesto -> un elemento extra
  sim_pin[DIT] = 0; sim_pin[DAH] = 0; keyer_run(150); sim_pin[DIT] = 1; sim_pin[DAH] = 1; keyer_run(800);
  nb = n_el;
  CHECK(na == 2 && nb == 3, "K3 Iambic A=2 elementos, Iambic B=3 (latch temprano)");

  // --- K4: Keyer Swap intercambia las paletas ---
  fresh(0);
  keyer_swap = 1;
  sim_pin[DIT] = 0; keyer_run(20); sim_pin[DIT] = 1; keyer_run(400);
  CHECK(n_el == 1 && near(el_on[0], 180), "K4 swap: paleta DIT da raya");

  // --- K5: la paleta cancela el CQ periodico (legacy 5210). Bug corregido ---
  fresh(0);
  cw_msg_event = sim_millis + 5000;
  keyer_run(50);
  CHECK(cw_msg_event != 0, "K5 sin paleta el CQ sigue agendado");
  sim_pin[DIT] = 0; keyer_run(5); sim_pin[DIT] = 1; keyer_run(200);
  CHECK(cw_msg_event == 0, "K5 tocar la paleta (iambico) cancela el CQ");
  fresh(1);
  cw_msg_event = sim_millis + 5000;
  sim_pin[DAH] = 0; keyer_run(5); sim_pin[DAH] = 1; keyer_run(300);
  CHECK(cw_msg_event == 0, "K5 tambien con la raya en Iambic B");

  // --- M1: cw_tx manipula el codigo correcto ('A' = .-) ---
  fresh(2);
  CHECK(cw_tx('A') == 0 && n_el == 2 && near(el_on[0], 60) && near(el_on[1], 180) && near(el_gap[1], 60),
        "M1 cw_tx('A') = punto, hueco, raya");
  fresh(2);
  char msg[] = "CQ";
  CHECK(cw_tx(msg) == 0 && n_el == 8, "M1 cw_tx(\"CQ\") = 8 elementos (-.-. --.-)");
  bool cq = near(el_on[0],180)&&near(el_on[1],60)&&near(el_on[2],180)&&near(el_on[3],60)
          &&near(el_on[4],180)&&near(el_on[5],180)&&near(el_on[6],60)&&near(el_on[7],180);
  CHECK(cq && near(el_gap[4], 180), "M1 CQ: patron exacto y 3 dits entre letras");

  // --- M2: pulsar la paleta aborta el mensaje y deja la portadora cortada ---
  fresh(2);
  script_pin = DIT; script_lvl = 0; script_t = sim_millis + 400;
  char msg2[] = "CQ CQ CQ";
  CHECK(cw_tx(msg2) == 1 && key_now == 0, "M2 paleta aborta el mensaje con TX apagado");
  fresh(2);
  char msg3[] = "T";
  CHECK(cw_tx(msg3) == 0 && n_el == 1 && near(el_on[0], 180), "M2 sin tocar nada el elemento se completa");

  // --- D1: decoder de tiempos (dec2 OLD_CW): texto y evento de pantalla ---
  dec_reset();
  dec_feed("-.-. --.-", 60);
  CHECK(line_ends("CQ"), "D1 decodifica CQ a 20 wpm");
  CHECK(cw_event != 0, "D1 levanta cw_event para pintar");
  dec_reset();
  dec_feed("-.. ./. .- --... .-.. .--- -.--", 60);
  CHECK(line_ends("DE EA7LJY"), "D1 respeta el espacio entre palabras");
  dec_reset();
  dec_feed("-.-. --.-/... --- .../.--. .- .-. .. ...", 40);
  CHECK(line_ends("SOS PARIS"), "D1 30 wpm tras entrenar con CQ");
  dec_reset();
  dec_feed("-.-. --.-/- . ... -", 100);
  CHECK(line_ends("CQ TEST"), "D1 12 wpm");

  // --- D2: switch_rxtx alimenta el decoder con la propia manipulacion ---
  dec_reset();
  cw_set_keyed(1);
  CHECK(filteredstate == 1, "D2 cw_set_keyed fija el estado del decoder");

  // --- P: PARIDAD con el decoder de usdx-legazy, paso a paso (1 ms) ---
  // Misma envolvente de audio (_amp32) a los dos: etapa de audio (umbral +
  // noise blanker) + dec2. Se compara texto, evento, wpm y estado filtrado.
  {
    static const char* const txt[] = {
      "-.-. --.-/-.-. --.-/-.. ./. .- --... .-.. .--- -.--/.--. ... . / -.-",
      "..... ----. ----./- ..- / ..--- ...-- ----- / -... -.- / --... ...-- / .-.-.",
      ". . . . . ./- - - - -/.. -- .. --/.-.-.- --..-- ..--.. -..-.",
    };
    static const uint16_t dits[] = {24, 40, 60, 100, 150}; // 50..8 wpm
    uint32_t seed = 12345;
    long steps = 0, mism = 0, events = 0;
    // mismo estado de arranque en los dos (los tests anteriores movieron el de produccion)
    dec_reset();
    sym = 0; avg = 256; wpm = 25; laststarttime = 0;
    realstate = realstatebefore = LOW;
    lowduration = highduration = 0;
    legacy::startttimelow = legacy::starttimehigh = sim_millis;
    for(int noise = 0; noise < 3; noise++)
      for(unsigned d = 0; d < 5; d++)
        for(unsigned t = 0; t < 3; t++) {
          uint32_t dit = dits[d];
          for(const char* p = txt[t]; *p; p++) {
            uint32_t on = (*p == '.') ? dit : (*p == '-') ? 3 * dit : 0;
            uint32_t off = (*p == ' ') ? 2 * dit : (*p == '/') ? 6 * dit : dit;
            if(noise == 2) { // manipulacion a mano: +-25 % de jitter
              seed = seed * 1103515245u + 12345u;
              on = on * (75 + (seed >> 16) % 51) / 100;
              seed = seed * 1103515245u + 12345u;
              off = off * (75 + (seed >> 16) % 51) / 100;
            }
            for(uint32_t i = 0; i < on + off; i++) {
              uint32_t a = (i < on) ? 20000 : 40;
              seed = seed * 1103515245u + 12345u;
              if(noise) { // QRN: ruido de fondo + chasquidos cortos (< noise blanker)
                a += (seed >> 16) % 30;
                if(((seed >> 8) % 97) == 0)
                  a = (i < on) ? 0 : 30000;
              }
              _amp32 = a;
              legacy::_amp32 = a;
              cw_decode();
              legacy::cw_decode();
              if(cw_event) events++;
              if(strcmp(cw_line, legacy::out) || (cw_event != 0) != (legacy::cw_event != 0) || wpm != legacy::wpm ||
                 filteredstate != legacy::filteredstate || sym != legacy::sym)
                mism++;
              if((steps % 100) == 0) // la pantalla consume el evento (tick de 100 ms)
                cw_event = legacy::cw_event = 0;
              steps++;
              sim_millis++;
            }
          }
        }
    printf("   paridad decoder: %ld pasos, %ld eventos, %ld mismatches, linea final [%s]\n", steps, events, mism, cw_line);
    CHECK(steps > 300000 && events > 1000, "P la simulacion ejercita el decoder");
    CHECK(mism == 0, "P decoder identico a usdx-legazy (texto, evento, wpm, estado)");
    CHECK(!strcmp(m2c, legacy::m2c), "P tabla morse m2c identica a legacy");
  }

  return SUMMARY();
}
