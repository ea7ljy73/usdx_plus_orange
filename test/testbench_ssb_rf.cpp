/**
 * @file testbench_ssb_rf.cpp
 * @brief Testbench completo con simulacion SI5351 para frecuencias RF reales
 *
 * Simula la cadena completa TX: Audio -> IF -> RF (SI5351)
 * Bandas HF: 160m, 80m, 60m, 40m, 30m, 20m, 15m, 12m, 10m, 6m
 *
 * Uso:
 *   ./testbench_ssb_rf --band=10m --mode=usb    (10m USB)
 *   ./testbench_ssb_rf --band=40m --mode=lsb    (40m LSB)
 *   ./testbench_ssb_rf --band=20m --mode=cw     (20m CW)
 *   ./testbench_ssb_rf --all                       (todas las bandas)
 *   ./testbench_ssb_rf --compare                   (USB vs LSB)
 *
 * Compilar: g++ -std=c++11 -O2 -o testbench_ssb_rf testbench_ssb_rf.cpp
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

vector<double> ssb_modulator(const vector<double>& audio, Mode mode) {
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
  double         drive_scale = 2.0 / 4.0;

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

Result analyze_band(const string& band, Mode mode, double audio_freq = 1000.0) {
  Result r;
  r.band = band;
  r.mode = (mode == USB) ? "USB" : (mode == LSB) ? "LSB" : "CW";

  BandInfo info    = BANDS[band];
  double   rf_base = info.freq_mhz * 1000000.0;

  SI5351Simulator si5351(F_XTAL);
  si5351.set_rf_freq(rf_base);

  vector<double> audio = generate_tone(audio_freq, 31250);
  vector<double> rf_if = ssb_modulator(audio, mode);

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

  r.if_freq     = idx1 * bin_res;
  r.suppression = (carrier > 0.001) ? 20.0 * log10(max1 / carrier) : 60.0;
  r.rejection   = (max2 > 0.001) ? 20.0 * log10(max1 / max2) : 60.0;

  double audio_offset = (mode == USB) ? audio_freq : -audio_freq;
  r.rf_freq           = rf_base + audio_offset * 100;

  r.pass = (r.suppression > 40.0 && r.rejection > 30.0);

  return r;
}

void print_header() {
  cout << "\n";
  cout << "==========================================================\n";
  cout << "   uSDX Plus Orange - Testbench RF con SI5351 Simulator\n";
  cout << "                    Version 5.14\n";
  cout << "==========================================================\n";
  cout << "\n";
  cout << "  Cristal SI5351: " << fixed << setprecision(3) << F_XTAL / 1000000.0 << " MHz\n";
  cout << "  F_SAMP_TX: " << fixed << setprecision(0) << F_SAMP_TX << " Hz\n";
}

void print_result(const Result& r) {
  BandInfo info = BANDS[r.band];

  cout << "\n";
  cout << "  " << r.band << " (" << fixed << setprecision(3) << info.freq_mhz << " MHz) - " << r.mode << "\n";
  cout << "  -------------------------------------------------------\n";
  cout << "  Frecuencia RF salida: " << fixed << setprecision(3) << r.rf_freq / 1000000.0 << " MHz\n";
  cout << "  Offset IF: " << fixed << setprecision(1) << r.if_freq << " Hz\n";
  cout << "\n";
  cout << "  Supresion portadora: " << fixed << setprecision(1) << r.suppression << " dB\n";
  cout << "  Rechazo banda lat:  " << fixed << setprecision(1) << r.rejection << " dB\n";
  cout << "\n";

  if(r.pass) {
    cout << "  [PASS] Modulacion CORRECTA\n";
  } else {
    cout << "  [WARN] Revisar parametros\n";
  }
}

void run_all_bands() {
  cout << "\n";
  cout << "==========================================================\n";
  cout << "          ANALISIS COMPLETO DE BANDAS HF\n";
  cout << "==========================================================\n";

  vector<string> band_order = {"80m", "60m", "40m", "30m", "20m", "17m", "15m", "10m"};

  cout << "\n";
  cout << "  Banda   | Frec(MHz) | Modo  | Sup.Port | Rechazo | Status\n";
  cout << "  --------|------------|--------|----------|---------|--------\n";

  int pass_count = 0, warn_count = 0;

  for(const string& band : band_order) {
    BandInfo info = BANDS[band];
    Mode     mode = (info.typical_mode == "USB") ? USB : LSB;

    Result r = analyze_band(band, mode, 1000.0);

    string status = r.pass ? "[OK]" : "[WARN]";
    if(r.pass)
      pass_count++;
    else
      warn_count++;

    cout << "  " << left << setw(8) << band << " | " << fixed << setprecision(3) << setw(10) << info.freq_mhz << " | "
         << left << setw(6) << info.typical_mode << " | " << fixed << setprecision(1) << setw(8) << r.suppression
         << " | " << fixed << setprecision(1) << setw(7) << r.rejection << " | " << status << "\n";
  }

  cout << "\n";
  cout << "  Resumen: " << pass_count << " OK, " << warn_count << " WARN\n";
  cout << "\n";
}

void run_band(const string& band, const string& mode_str) {
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

  Result r = analyze_band(band, mode, 1000.0);
  print_result(r);
}

void run_compare() {
  cout << "\n";
  cout << "==========================================================\n";
  cout << "          COMPARACION USB vs LSB\n";
  cout << "==========================================================\n";

  vector<string> bands = {"40m", "20m", "10m", "80m", "15m"};

  for(const string& band : bands) {
    BandInfo info = BANDS[band];
    cout << "\n  " << band << " (" << fixed << setprecision(3) << info.freq_mhz << " MHz):\n";

    Result usb = analyze_band(band, USB, 1000.0);
    Result lsb = analyze_band(band, LSB, 1000.0);

    cout << "    USB: Sup=" << fixed << setprecision(1) << usb.suppression << " dB, Rechazo=" << fixed
         << setprecision(1) << usb.rejection << " dB\n";
    cout << "    LSB: Sup=" << fixed << setprecision(1) << lsb.suppression << " dB, Rechazo=" << fixed
         << setprecision(1) << lsb.rejection << " dB\n";

    double ratio   = usb.suppression / lsb.suppression;
    double diff_db = 20.0 * log10(ratio);
    cout << "    Diferencia: " << fixed << setprecision(2) << diff_db << " dB\n";
  }
  cout << "\n";
}

void print_help() {
  cout << "\n";
  cout << "  testbench_ssb_rf - Testbench RF con simulacion SI5351\n";
  cout << "\n";
  cout << "  Uso:\n";
  cout << "    ./testbench_ssb_rf --band=10m --mode=usb   (10m USB)\n";
  cout << "    ./testbench_ssb_rf --band=40m --mode=lsb   (40m LSB)\n";
  cout << "    ./testbench_ssb_rf --band=20m --mode=cw    (20m CW)\n";
  cout << "    ./testbench_ssb_rf --all                   (todas las bandas)\n";
  cout << "    ./testbench_ssb_rf --compare               (USB vs LSB)\n";
  cout << "    ./testbench_ssb_rf --help                 (ayuda)\n";
  cout << "\n";
  cout << "  Bandas: 160m, 80m, 60m, 40m, 30m, 20m, 17m, 15m, 12m, 10m, 6m\n";
  cout << "\n";
}

int main(int argc, char** argv) {
  string band      = "40m";
  string mode_str  = "auto";
  bool   all_bands = false;
  bool   compare   = false;
  bool   help      = false;

  for(int i = 1; i < argc; i++) {
    if(strncmp(argv[i], "--band=", 7) == 0) {
      band = string(argv[i] + 7);
    } else if(strncmp(argv[i], "--mode=", 7) == 0) {
      mode_str = string(argv[i] + 7);
    } else if(strcmp(argv[i], "--all") == 0) {
      all_bands = true;
    } else if(strcmp(argv[i], "--compare") == 0) {
      compare = true;
    } else if(strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      help = true;
    }
  }

  print_header();

  if(help) {
    print_help();
  } else if(all_bands) {
    run_all_bands();
  } else if(compare) {
    run_compare();
  } else {
    run_band(band, mode_str);
  }

  cout << "\n";
  return 0;
}
