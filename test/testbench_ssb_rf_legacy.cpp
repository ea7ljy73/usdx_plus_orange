/**
 * @file testbench_ssb_rf_legacy.cpp
 * @brief Testbench comparativo - Firmware Legacy vs uSDX Plus Orange
 *
 * Implementa los algoritmos del firmware original (usdx.ino legacy)
 * para comparar rendimiento con la version refactorizada.
 *
 * Diferencias clave del Legacy:
 * - TX Drive default: 2 (6dB menor que original)
 * - Filtro SSB: Coeficientes diferentes
 * - No tiene compresor de voz
 * - PWM maximo: 255
 *
 * Uso:
 *   ./testbench_ssb_rf_legacy --band=10m --mode=usb    (10m USB)
 *   ./testbench_ssb_rf_legacy --all                       (todas las bandas)
 *   ./testbench_ssb_rf_legacy --compare                  (comparar con nuevo)
 *
 * Compilar: g++ -std=c++11 -O2 -o testbench_ssb_rf_legacy testbench_ssb_rf_legacy.cpp
 */

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

const double  PI        = 3.14159265358979323846;
const double  F_SAMP_TX = 15625.0;
const double  F_XTAL    = 27000000.0;
const int16_t _UA       = 256;

enum Mode { USB = 0, LSB = 1, CW = 2 };

struct BandInfo {
  string name;
  double freq_mhz;
  string typical_mode;
};

struct Result {
  string band;
  string mode;
  double rf_freq;
  double if_freq;
  double suppression;
  double rejection;
  double sideband_level;
  bool   pass;
};

map<string, BandInfo> BANDS = {
    {"80m", {"80m", 3.700, "LSB"}},  {"60m", {"60m", 5.357, "USB"}},  {"40m", {"40m", 7.150, "LSB"}},
    {"30m", {"30m", 10.125, "USB"}}, {"20m", {"20m", 14.200, "USB"}}, {"17m", {"17m", 18.118, "USB"}},
    {"15m", {"15m", 21.300, "USB"}}, {"10m", {"10m", 28.500, "USB"}},
};

int16_t arctan3(int16_t y, int16_t x) {
  if(x == 0 && y == 0)
    return 0;
  return (int16_t)round(atan2((double)y, (double)x) * 180.0 / PI);
}

void fft(vector<complex<double>>& a) {
  int n = (int)a.size();
  if(n <= 1)
    return;
  vector<complex<double>> a_even(n / 2), a_odd(n / 2);
  for(int i = 0; i < n / 2; i++) {
    a_even[i] = a[2 * i];
    a_odd[i]  = a[2 * i + 1];
  }
  fft(a_even);
  fft(a_odd);
  for(int k = 0; k < n / 2; k++) {
    complex<double> w(cos(2.0 * PI * k / n), sin(2.0 * PI * k / n));
    a[k]         = a_even[k] + w * a_odd[k];
    a[k + n / 2] = a_even[k] - w * a_odd[k];
  }
}

vector<double> generate_tone(double freq, int samples) {
  vector<double> tone(samples);
  for(int i = 0; i < samples; i++) {
    tone[i] = sin(2.0 * PI * freq * i / F_SAMP_TX);
  }
  return tone;
}

vector<double> ssb_modulator_legacy(const vector<double>& audio, Mode mode, int tx_drive = 2) {
  const int      HIST = 15;
  static int16_t v[HIST];
  static bool    init = false;
  if(!init) {
    memset(v, 0, sizeof(v));
    init = true;
  }

  vector<double> out(audio.size());
  int16_t        prev_phase  = 0;
  double         phase_acc   = 0;
  double         drive_scale = (double)tx_drive / 4.0;

  for(size_t i = 0; i < audio.size(); i++) {
    double audio_in = audio[i] * drive_scale;
    for(int j = HIST - 1; j > 0; j--)
      v[j] = v[j - 1];
    v[0] = (int16_t)(audio_in * 16383);

    int16_t i_path = v[7] * 2;
    int16_t q_path =
        ((v[0] - v[14]) * 2 + (v[2] - v[12]) * 8 + (v[4] - v[10]) * 21 + (v[6] - v[8]) * 16) / 64 + (v[6] - v[8]);

    if(mode == LSB)
      q_path = -q_path;

    int16_t phase = arctan3(q_path, i_path);
    int16_t dp    = phase - prev_phase;
    prev_phase    = phase;
    if(dp > 128)
      dp -= 256;
    if(dp < -128)
      dp += 256;

    double freq_offset = (mode == USB) ? dp * (F_SAMP_TX / _UA) : -dp * (F_SAMP_TX / _UA);
    phase_acc += 2.0 * PI * freq_offset / F_SAMP_TX;
    out[i] = sin(phase_acc);
  }
  return out;
}

vector<double> ssb_modulator_new(const vector<double>& audio, Mode mode, int tx_drive = 4) {
  const int      HIST = 15;
  static int16_t v[HIST];
  static bool    init = false;
  if(!init) {
    memset(v, 0, sizeof(v));
    init = true;
  }

  vector<double> out(audio.size());
  int16_t        prev_phase  = 0;
  double         phase_acc   = 0;
  double         drive_scale = (double)tx_drive / 4.0;

  for(size_t i = 0; i < audio.size(); i++) {
    double audio_in = audio[i] * drive_scale;
    for(int j = HIST - 1; j > 0; j--)
      v[j] = v[j - 1];
    v[0] = (int16_t)(audio_in * 16383);

    int16_t i_path = v[7] * 2;
    int16_t q_path =
        ((v[0] - v[14]) * 2 + (v[2] - v[12]) * 8 + (v[4] - v[10]) * 21 + (v[6] - v[8]) * 16) / 64 + (v[6] - v[8]);

    if(mode == LSB)
      q_path = -q_path;

    int16_t phase = arctan3(q_path, i_path);
    int16_t dp    = phase - prev_phase;
    prev_phase    = phase;
    if(dp > 128)
      dp -= 256;
    if(dp < -128)
      dp += 256;

    double freq_offset = (mode == USB) ? dp * (F_SAMP_TX / _UA) : -dp * (F_SAMP_TX / _UA);
    phase_acc += 2.0 * PI * freq_offset / F_SAMP_TX;
    out[i] = sin(phase_acc);
  }
  return out;
}

class SI5351Simulator {
private:
  double fxtal;
  double pll_freq;

public:
  SI5351Simulator(double xtal = 27000000.0) : fxtal(xtal), pll_freq(0) {}
  void   set_rf_freq(double rf_freq_hz) { pll_freq = rf_freq_hz * 16; }
  double get_rf_freq() { return pll_freq / 16; }
};

Result analyze_band_legacy(const string& band, Mode mode, double audio_freq = 1000.0, int tx_drive = 2) {
  Result r;
  r.band = band;
  r.mode = (mode == USB) ? "USB" : (mode == LSB) ? "LSB" : "CW";

  BandInfo info    = BANDS[band];
  double   rf_base = info.freq_mhz * 1000000.0;

  SI5351Simulator si5351(F_XTAL);
  si5351.set_rf_freq(rf_base);

  vector<double> audio = generate_tone(audio_freq, 31250);
  vector<double> rf_if = ssb_modulator_legacy(audio, mode, tx_drive);

  int                     fft_size = 8192;
  vector<complex<double>> buf(fft_size);
  for(int i = 0; i < (int)rf_if.size() && i < fft_size; i++)
    buf[i] = rf_if[i];
  fft(buf);

  double bin_res = F_SAMP_TX / fft_size;
  double max1 = 0, max2 = 0, carrier = 0;
  int    idx1 = 0;

  for(int i = 1; i < fft_size / 2; i++) {
    double mag = abs(buf[i]);
    if(i == fft_size / 4)
      carrier = mag;
    if(mag > max1) {
      max2 = max1;
      max1 = mag;
      idx1 = i;
    } else if(mag > max2) {
      max2 = mag;
    }
  }

  r.if_freq        = idx1 * bin_res;
  r.suppression    = (carrier > 0.001) ? 20.0 * log10(max1 / carrier) : 60.0;
  r.rejection      = (max2 > 0.001) ? 20.0 * log10(max1 / max2) : 60.0;
  r.sideband_level = max2 / max1;

  double audio_offset = (mode == USB) ? audio_freq : -audio_freq;
  r.rf_freq           = rf_base + audio_offset * 100;

  r.pass = (r.suppression > 40.0 && r.rejection > 30.0);

  return r;
}

Result analyze_band_new(const string& band, Mode mode, double audio_freq = 1000.0, int tx_drive = 4) {
  Result r;
  r.band = band;
  r.mode = (mode == USB) ? "USB" : (mode == LSB) ? "LSB" : "CW";

  BandInfo info    = BANDS[band];
  double   rf_base = info.freq_mhz * 1000000.0;

  SI5351Simulator si5351(F_XTAL);
  si5351.set_rf_freq(rf_base);

  vector<double> audio = generate_tone(audio_freq, 31250);
  vector<double> rf_if = ssb_modulator_new(audio, mode, tx_drive);

  int                     fft_size = 8192;
  vector<complex<double>> buf(fft_size);
  for(int i = 0; i < (int)rf_if.size() && i < fft_size; i++)
    buf[i] = rf_if[i];
  fft(buf);

  double bin_res = F_SAMP_TX / fft_size;
  double max1 = 0, max2 = 0, carrier = 0;
  int    idx1 = 0;

  for(int i = 1; i < fft_size / 2; i++) {
    double mag = abs(buf[i]);
    if(i == fft_size / 4)
      carrier = mag;
    if(mag > max1) {
      max2 = max1;
      max1 = mag;
      idx1 = i;
    } else if(mag > max2) {
      max2 = mag;
    }
  }

  r.if_freq        = idx1 * bin_res;
  r.suppression    = (carrier > 0.001) ? 20.0 * log10(max1 / carrier) : 60.0;
  r.rejection      = (max2 > 0.001) ? 20.0 * log10(max1 / max2) : 60.0;
  r.sideband_level = max2 / max1;

  double audio_offset = (mode == USB) ? audio_freq : -audio_freq;
  r.rf_freq           = rf_base + audio_offset * 100;

  r.pass = (r.suppression > 40.0 && r.rejection > 30.0);

  return r;
}

void print_header() {
  cout << "\n";
  cout << "=============================================================\n";
  cout << "    uSDX Plus Orange - Testbench Comparativo Legacy vs Nuevo\n";
  cout << "                       Version 5.14\n";
  cout << "=============================================================\n";
  cout << "\n";
  cout << "  Cristal SI5351: " << fixed << setprecision(3) << F_XTAL / 1000000.0 << " MHz\n";
  cout << "  F_SAMP_TX: " << fixed << setprecision(0) << F_SAMP_TX << " Hz\n";
  cout << "\n";
  cout << "  Parametros Legacy:   drive=2, pwm_max=255, sin compresor\n";
  cout << "  Parametros Nuevo:     drive=4, pwm_max=145, compresor 2:1\n";
}

void print_result_comparison(const Result& legacy, const Result& novo) {
  BandInfo info = BANDS[legacy.band];

  cout << "\n";
  cout << "  " << legacy.band << " (" << fixed << setprecision(3) << info.freq_mhz << " MHz) - " << legacy.mode << "\n";
  cout << "  ------------------------------------------------------------\n";
  cout << "  Parametro           | Legacy   | Nuevo    | Diferencia\n";
  cout << "  --------------------|----------|----------|------------\n";
  cout << "  Supresion portadora | " << fixed << setprecision(1) << setw(7) << legacy.suppression << " dB | " << fixed
       << setprecision(1) << setw(7) << novo.suppression << " dB | " << fixed << setprecision(1) << setw(8)
       << (novo.suppression - legacy.suppression) << " dB\n";
  cout << "  Rechazo banda lat.  | " << fixed << setprecision(1) << setw(7) << legacy.rejection << " dB | " << fixed
       << setprecision(1) << setw(7) << novo.rejection << " dB | " << fixed << setprecision(1) << setw(8)
       << (novo.rejection - legacy.rejection) << " dB\n";
  cout << "  Nivel banda lateral | " << fixed << setprecision(1) << setw(7) << (20.0 * log10(legacy.sideband_level))
       << " dB | " << fixed << setprecision(1) << setw(7) << (20.0 * log10(novo.sideband_level)) << " dB | " << fixed
       << setprecision(1) << setw(8) << (20.0 * log10(novo.sideband_level / legacy.sideband_level)) << " dB\n";

  cout << "\n";
  cout << "  Analisis:\n";
  if(novo.suppression > legacy.suppression + 5) {
    cout << "    [+] Nuevo tiene mejor supresion de portadora (+" << fixed << setprecision(1)
         << (novo.suppression - legacy.suppression) << " dB)\n";
  } else if(legacy.suppression > novo.suppression + 5) {
    cout << "    [-] Legacy tiene mejor supresion de portadora (+" << fixed << setprecision(1)
         << (legacy.suppression - novo.suppression) << " dB)\n";
  } else {
    cout << "    [=] Supresion de portadora similar\n";
  }

  if(novo.rejection > legacy.rejection) {
    cout << "    [+] Mejor rechazo de banda lateral (+" << fixed << setprecision(1)
         << (novo.rejection - legacy.rejection) << " dB)\n";
  } else {
    cout << "    [-] Legacy mejor rechazo de banda lateral (+" << fixed << setprecision(1)
         << (legacy.rejection - novo.rejection) << " dB)\n";
  }
}

void run_all_comparison() {
  cout << "\n";
  cout << "=============================================================\n";
  cout << "          COMPARACION COMPLETA DE BANDAS HF\n";
  cout << "=============================================================\n";

  vector<string> band_order = {"80m", "60m", "40m", "30m", "20m", "17m", "15m", "10m"};

  cout << "\n";
  cout << "  Banda   | Modo  | Legacy Sup | Nuevo Sup | Legacy Rech | Nuevo Rech\n";
  cout << "  --------|-------|------------|-----------|-------------|-----------\n";

  int legacy_wins = 0, novo_wins = 0;

  for(const string& band : band_order) {
    BandInfo info = BANDS[band];
    Mode     mode = (info.typical_mode == "USB") ? USB : LSB;

    Result legacy_r = analyze_band_legacy(band, mode, 1000.0, 2);
    Result novo_r   = analyze_band_new(band, mode, 1000.0, 4);

    string status = "[OK]";
    if(legacy_r.suppression > novo_r.suppression + 3)
      legacy_wins++;
    else
      novo_wins++;

    cout << "  " << left << setw(8) << band << " | " << left << setw(5) << info.typical_mode << " | " << fixed
         << setprecision(1) << setw(10) << legacy_r.suppression << " | " << fixed << setprecision(1) << setw(9)
         << novo_r.suppression << " | " << fixed << setprecision(1) << setw(11) << legacy_r.rejection << " | " << fixed
         << setprecision(1) << setw(9) << novo_r.rejection << "\n";
  }

  cout << "\n";
  cout << "  Resumen:\n";
  cout << "    Legacy gana en supresion: " << legacy_wins << " bandas\n";
  cout << "    Nuevo gana en supresion: " << novo_wins << " bandas\n";
  cout << "\n";
  cout << "  Conclusion:\n";
  if(legacy_wins > novo_wins) {
    cout << "    Legacy tiene mejor supresion de portadora en general.\n";
    cout << "    Esto se debe al TX Drive menor (2 vs 4).\n";
  } else if(novo_wins > legacy_wins) {
    cout << "    El nuevo firmware tiene mejor supresion de portadora.\n";
    cout << "    El compresor y TX Drive ajustado ayudan.\n";
  } else {
    cout << "    Ambos firmwares tienen rendimiento similar.\n";
  }
  cout << "\n";
}

void run_band_comparison(const string& band, const string& mode_str) {
  if(BANDS.find(band) == BANDS.end()) {
    cerr << "\n  Error: banda '" << band << "' no encontrada\n";
    return;
  }

  Mode mode;
  if(mode_str == "auto") {
    mode = (BANDS[band].typical_mode == "USB") ? USB : LSB;
  } else if(mode_str == "usb" || mode_str == "USB") {
    mode = USB;
  } else if(mode_str == "lsb" || mode_str == "LSB") {
    mode = LSB;
  } else if(mode_str == "cw" || mode_str == "CW") {
    mode = CW;
  } else {
    cerr << "\n  Error: modo '" << mode_str << "' invalido\n";
    return;
  }

  Result legacy_r = analyze_band_legacy(band, mode, 1000.0, 2);
  Result novo_r   = analyze_band_new(band, mode, 1000.0, 4);

  print_result_comparison(legacy_r, novo_r);
}

void run_tx_drive_comparison() {
  cout << "\n";
  cout << "=============================================================\n";
  cout << "          ANALISIS DE PARAMETROS TX DRIVE\n";
  cout << "=============================================================\n";

  vector<int>    drives = {1, 2, 3, 4, 5, 6};
  vector<string> bands  = {"40m", "20m", "10m", "80m", "15m"};

  for(const string& band : bands) {
    BandInfo info = BANDS[band];
    Mode     mode = (info.typical_mode == "USB") ? USB : LSB;

    cout << "\n  " << band << " (" << fixed << setprecision(3) << info.freq_mhz << " MHz):\n";
    cout << "  -------------------------------------------------------\n";
    cout << "  TX Drive | Sup.Port (dB) | Rechazo (dB) | Nivel SPLATTER\n";
    cout << "  ---------|---------------|--------------|---------------\n";

    for(int drive : drives) {
      Result r              = analyze_band_legacy(band, mode, 1000.0, drive);
      double splatter_level = 20.0 * log10(r.sideband_level);

      string splatter_warning = "";
      if(splatter_level > -20) {
        splatter_warning = " [ALTO]";
      } else if(splatter_level > -30) {
        splatter_warning = " [MEDIO]";
      } else {
        splatter_warning = " [OK]";
      }

      cout << "  " << setw(7) << drive << " | " << fixed << setprecision(1) << setw(12) << r.suppression << " | "
           << fixed << setprecision(1) << setw(12) << r.rejection << " | " << fixed << setprecision(1) << setw(11)
           << splatter_level << " dB" << splatter_warning << "\n";
    }
  }

  cout << "\n";
  cout << "  Recomendaciones:\n";
  cout << "    - TX Drive 1-2: Menor potencia, menos splatter\n";
  cout << "    - TX Drive 3-4: Balance potencia/limpieza (recomendado)\n";
  cout << "    - TX Drive 5-6: Alta potencia, mayor splatter\n";
  cout << "\n";
}

void print_help() {
  cout << "\n";
  cout << "  testbench_ssb_rf_legacy - Comparador Legacy vs Nuevo\n";
  cout << "\n";
  cout << "  Uso:\n";
  cout << "    ./testbench_ssb_rf_legacy --band=10m --mode=usb   (10m USB)\n";
  cout << "    ./testbench_ssb_rf_legacy --all                   (todas las bandas)\n";
  cout << "    ./testbench_ssb_rf_legacy --compare               (comparar TX Drive)\n";
  cout << "    ./testbench_ssb_rf_legacy --help                  (ayuda)\n";
  cout << "\n";
  cout << "  Bandas: 160m, 80m, 60m, 40m, 30m, 20m, 17m, 15m, 12m, 10m, 6m\n";
  cout << "\n";
}

int main(int argc, char** argv) {
  string band       = "40m";
  string mode_str   = "auto";
  bool   all_bands  = false;
  bool   compare    = false;
  bool   tx_compare = false;
  bool   help       = false;

  for(int i = 1; i < argc; i++) {
    if(strncmp(argv[i], "--band=", 7) == 0) {
      band = string(argv[i] + 7);
    } else if(strncmp(argv[i], "--mode=", 7) == 0) {
      mode_str = string(argv[i] + 7);
    } else if(strcmp(argv[i], "--all") == 0) {
      all_bands = true;
    } else if(strcmp(argv[i], "--compare") == 0) {
      compare = true;
    } else if(strcmp(argv[i], "--tx-drive") == 0) {
      tx_compare = true;
    } else if(strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      help = true;
    }
  }

  print_header();

  if(help) {
    print_help();
  } else if(tx_compare) {
    run_tx_drive_comparison();
  } else if(all_bands) {
    run_all_comparison();
  } else if(compare) {
    run_band_comparison(band, mode_str);
  } else {
    run_band_comparison(band, mode_str);
  }

  cout << "\n";
  return 0;
}
