#!/usr/bin/env python3
"""size_report.py — Fase 0: tracking de tamaño flash/RAM por build.

Compila el sketch, extrae totales + top símbolos (avr-nm sobre el .elf) y
añade una fila a tools/size_log.csv: fecha, git hash, flash, ram, top texto.

Uso:
  python3 tools/size_report.py [--note "texto"] [--extra-flags "-DPERF_METER"]
"""
import csv
import glob
import os
import re
import subprocess
import sys
from datetime import date

HERE = os.path.dirname(os.path.abspath(__file__))
SKETCH = os.path.dirname(HERE)
CSV = os.path.join(HERE, "size_log.csv")
BUILD = "/tmp/usdx_v2_sizebuild"


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def find_avr_nm():
    cands = sorted(glob.glob(os.path.expanduser(
        "~/.arduino15/packages/arduino/tools/avr-gcc/*/bin/avr-nm")))
    if not cands:
        return None
    return cands[-1]


def main():
    note = ""
    extra = []
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] == "--note":
            note = args[i + 1]
            i += 2
        elif args[i] == "--extra-flags":
            extra = ["--build-property",
                     "compiler.cpp.extra_flags=" + args[i + 1]]
            i += 2
        else:
            i += 1
    r = run(["arduino-cli", "compile", "--fqbn", "arduino:avr:uno",
             "--build-path", BUILD] + extra + ["."], cwd=SKETCH)
    out = r.stdout + r.stderr
    m = re.search(r"[Ss]ketch usa (\d+) bytes", out)
    m2 = re.search(r"[Gg]lobales usan (\d+) bytes", out)
    if not m or not m2:
        print(out[-2000:])
        sys.exit("no se pudo parsear la salida de arduino-cli")
    flash, ram = int(m.group(1)), int(m2.group(1))

    top = ""
    nm = find_avr_nm()
    elfs = glob.glob(os.path.join(BUILD, "*.elf"))
    if nm and elfs:
        n = run([nm, "--print-size", "--size-sort", elfs[0]])
        syms = []
        for ln in n.stdout.splitlines():
            p = ln.split()
            # formato: addr size type name
            if len(p) == 4 and p[2] in "tT" and p[1] != "0":
                try:
                    syms.append((int(p[1], 16), p[3]))
                except ValueError:
                    pass
        syms.sort(reverse=True)
        top = ";".join(f"{nm_}:{sz}" for sz, nm_ in syms[:12])

    gh = run(["git", "rev-parse", "--short", "HEAD"],
             cwd=os.path.dirname(SKETCH))
    ghash = gh.stdout.strip() if gh.returncode == 0 else "?"
    new = not os.path.exists(CSV)
    with open(CSV, "a", newline="") as f:
        w = csv.writer(f)
        if new:
            w.writerow(["fecha", "git", "flash", "ram", "nota", "top_text"])
        w.writerow([str(date.today()), ghash, flash, ram, note, top])
    print(f"flash={flash} ram={ram} git={ghash} nota={note!r}")
    if top:
        print("top text:", top)


if __name__ == "__main__":
    main()
