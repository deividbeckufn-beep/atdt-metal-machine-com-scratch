#!/usr/bin/env python3
# =============================================================================
#  Controle de qualidade da biblioteca (le o relatorio do PluginTest: MM_QC)
#  - nivela o volume: alvo RMS -20 dBFS, pico maximo -6 dBFS (headroom)
#  - baixos: estereo abaixo de 120 Hz deve ser ~0 (sub mono)
#  - sinaliza silencio, clipping e presets muito parecidos
#  Uso: python3 tools/qc_library.py relatorio.txt [--apply]
# =============================================================================
import json, math, os, sys

ROOT = os.path.dirname (os.path.dirname (os.path.abspath (__file__)))
GAINS = os.path.join (ROOT, "tools", "library_gains.json")
INC = os.path.join (ROOT, "Source", "Presets", "FactoryLibrary.inc")

TARGET_RMS_DB = -20.0
MAX_PEAK_DB = -6.0
BASS_CATS = ("BASS", "DUBSTEP", "TRAP", "BREAKDOWN")

lib_names = set()
for line in open (INC):
    if line.startswith ('{ "'):
        lib_names.add (line.split ('"')[1])

gains = json.load (open (GAINS)) if os.path.exists (GAINS) else {}
rows = []
for line in open (sys.argv[1]):
    parts = line.strip().split ("|")
    if len (parts) != 5: continue
    full, peak, rms, side, zcr = parts[0], float (parts[1]), float (parts[2]), float (parts[3]), float (parts[4])
    cat, _, name = full.partition (" - ")
    rows.append ((cat, name, peak, rms, side, zcr))

db = lambda x: 20.0 * math.log10 (max (x, 1e-9))
problems, new_gains, deviations = [], dict (gains), []
for cat, name, peak, rms, side, zcr in rows:
    if name not in lib_names: continue
    if rms < 1e-3: problems.append (f"SILENCIO     {cat} - {name}")
    if peak > 0.97: problems.append (f"PICO ALTO    {cat} - {name} ({db (peak):+.1f} dBFS)")
    if cat in BASS_CATS and side > 0.02: problems.append (f"SUB ESTEREO  {cat} - {name} (side/mid <120 Hz = {side:.3f})")
    corr = TARGET_RMS_DB - db (rms)
    corr = min (corr, MAX_PEAK_DB - db (peak))            # nunca passar do pico maximo
    deviations.append (abs (corr))
    old = gains.get (name, -8.0)
    new_gains[name] = round (max (-30.0, min (12.0, old + corr)), 1)

# presets quase identicos: mesma categoria, mesmo nivel e mesmo "brilho"
seen = {}
for cat, name, peak, rms, side, zcr in rows:
    if name not in lib_names: continue
    key = (cat, round (zcr / 25.0), round (db (rms)))
    if key in seen: problems.append (f"CONFERIR     {cat}: {seen[key]} ~ {name}")
    else: seen[key] = name

n = len (deviations)
print (f"presets da biblioteca medidos: {n}")
if n:
    within = sum (1 for d in deviations if d <= 1.5)
    print (f"ja nivelados (+-1.5 dB): {within}/{n}   desvio max {max (deviations):.1f} dB")
print (f"problemas: {len (problems)}")
for p in problems: print ("  " + p)
if "--apply" in sys.argv:
    json.dump (new_gains, open (GAINS, "w"), indent = 1, sort_keys = True)
    print ("ganhos atualizados em", GAINS)
