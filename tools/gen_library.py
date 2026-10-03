#!/usr/bin/env python3
# =============================================================================
#  Gerador da biblioteca de fabrica do ATDT METAL MACHINE
#  - Cada familia de som e uma RECEITA de sound design (funcao musical definida)
#  - Variacoes deterministicas (seed fixa): o resultado e sempre o mesmo
#  - Todo ID de parametro e validado contra a lista oficial do plugin
#  - Volume: vem de tools/library_gains.json, medido no audio pelo PluginTest (QC)
#  Uso: python3 tools/gen_library.py  -> gera Source/Presets/FactoryLibrary.inc
# =============================================================================
import json, os, random, sys

ROOT = os.path.dirname (os.path.dirname (os.path.abspath (__file__)))
OUT = os.path.join (ROOT, "Source", "Presets", "FactoryLibrary.inc")
GAINS = os.path.join (ROOT, "tools", "library_gains.json")
IDS = os.path.join (ROOT, "tools", "param_ids.txt")

valid = set (l.split ("=")[0].strip() for l in open (IDS) if "=" in l)
gains = json.load (open (GAINS)) if os.path.exists (GAINS) else {}

# ---- indices (mesma ordem do plugin) ----
SINE, TRI, SAW, SQUARE, PULSE, NOISE, WT, FM = range (8)
T_MORPH, T_FORMANT, T_GRIT, T_VOCAL, T_GROWL, T_PWM, T_SYNC = range (7)
R05, R1, R15, R2, R3, R4, R5, R7 = range (8)          # razoes FM
LP12, LP24, HP12, HP24, BP, NOTCH = range (6)
POLY, MONO, LEGATO = range (3)
L_SINE, L_TRI, L_SAW, L_SQ, L_SH, L_RND, L_RSAW = range (7)
D2_1, D1_1, D1_2, D1_4, D1_8, D1_16, D1_32, D1_4T, D1_8T, D1_16T, D1_64 = range (11)
S_NONE, S_LFO1, S_LFO2, S_LFO3, S_LFO4, S_FENV, S_AENV, S_VEL, S_MW, S_AT, S_NOTE, S_M1, S_M2, S_M3, S_M4, S_RND, S_MENV = range (17)
D_NONE, D_PITCH, D_O1P, D_O2P, D_O3P, D_O1L, D_O2L, D_O3L, D_SUB, D_NOISE, D_CUT, D_RES, D_DRIVE, D_FMIX, D_AMP, D_PAN, D_UNIDET, \
    D_L1A, D_L2A, D_L3A, D_L4A, D_O1WT, D_O2WT, D_O3WT, D_O1FM, D_O2FM, D_O3FM = range (27)
DIST_SOFT, DIST_HARD, DIST_FUZZ, DIST_RECT = range (4)
DL_14, DL_18, DL_18D, DL_116, DL_14T, DL_18T, DL_12 = range (7)
SEQ_OFF, SEQ_ON, SEQ_ARP = range (3)
ARP_UP, ARP_DOWN, ARP_UPDOWN, ARP_RANDOM, ARP_ORDER, ARP_CHORD = range (6)
RATE_14, RATE_18, RATE_116, RATE_132, RATE_18T, RATE_116T = range (6)

rng = random.Random (20260923)
def pick (xs): return rng.choice (xs)
def uni (a, b, nd=3): return round (rng.uniform (a, b), nd)

class P (dict):
    def s (self, k, v):
        if k not in valid: sys.exit ("ID invalido: " + k)
        self[k] = float (v); return self
    def osc (self, i, wave, level, oct=0, semi=0, fine=0.0, pan=0.0, wt=None, wtpos=None, fmr=None, fma=None):
        self.s (f"osc{i}_wave", wave).s (f"osc{i}_level", level).s (f"osc{i}_octave", oct).s (f"osc{i}_semi", semi)
        self.s (f"osc{i}_fine", fine).s (f"osc{i}_pan", pan)
        if wt is not None: self.s (f"osc{i}_wt", wt)
        if wtpos is not None: self.s (f"osc{i}_wtpos", wtpos)
        if fmr is not None: self.s (f"osc{i}_fmratio", fmr)
        if fma is not None: self.s (f"osc{i}_fmamt", fma)
        return self
    def off (self, i): return self.s (f"osc{i}_level", 0)
    def sub (self, wave, oct, lvl): return self.s ("sub_wave", wave).s ("sub_octave", oct).s ("sub_level", lvl)
    def noise (self, t, lvl): return self.s ("noise_type", t).s ("noise_level", lvl)
    def flt (self, t, cut, res, drive, env, key=0.3, mix=1.0):
        return self.s ("filter_type", t).s ("filter_cutoff", cut).s ("filter_resonance", res).s ("filter_drive", drive) \
                   .s ("filter_env_amount", env).s ("filter_keytrack", key).s ("filter_mix", mix)
    def fenv (self, a, d, s, r): return self.s ("fenv_attack", a).s ("fenv_decay", d).s ("fenv_sustain", s).s ("fenv_release", r)
    def amp (self, a, d, s, r): return self.s ("amp_attack", a).s ("amp_decay", d).s ("amp_sustain", s).s ("amp_release", r)
    def menv (self, a, d, s, r): return self.s ("menv_attack", a).s ("menv_decay", d).s ("menv_sustain", s).s ("menv_release", r)
    def voice (self, mode, count=4, glide=0.0): return self.s ("voice_mode", mode).s ("voice_count", count).s ("voice_glide", glide)
    def uni (self, n, det, spr): return self.s ("unison_voices", n).s ("unison_detune", det).s ("unison_spread", spr)
    def mono (self, hz): return self.s ("bass_mono", hz)
    def lfo (self, i, shape, sync, div_or_rate, amount=1.0, smooth=0.0):
        self.s (f"lfo{i}_shape", shape).s (f"lfo{i}_sync", 1 if sync else 0).s (f"lfo{i}_amount", amount).s (f"lfo{i}_smooth", smooth)
        return self.s (f"lfo{i}_div", div_or_rate) if sync else self.s (f"lfo{i}_rate", div_or_rate)
    def mod (self, slot, src, dst, amt): return self.s (f"mod{slot}_src", src).s (f"mod{slot}_dst", dst).s (f"mod{slot}_amt", amt)
    def macros (self, drive=0.5, dark=-0.45, space=0.45, move=1.0):
        return self.mod (5, S_M1, D_DRIVE, drive).mod (6, S_M2, D_L1A, move).mod (7, S_M3, D_CUT, dark).mod (8, S_M4, D_UNIDET, space)
    def vibrato (self): return self.lfo (4, L_SINE, False, 5.5, 0.0).mod (3, S_MW, D_L4A, 1.0).mod (4, S_LFO4, D_PITCH, 0.012)
    # ---- efeitos ----
    def dist (self, t, drive, tone=0.7, mix=1.0): return self.s ("fx_dist_on", 1).s ("fx_dist_type", t).s ("fx_dist_drive", drive).s ("fx_dist_tone", tone).s ("fx_dist_mix", mix)
    def eq (self, lo, mid, hi): return self.s ("fx_eq_on", 1).s ("fx_eq_low", lo).s ("fx_eq_mid", mid).s ("fx_eq_high", hi)
    def chorus (self, rate, depth, mix): return self.s ("fx_ch_on", 1).s ("fx_ch_rate", rate).s ("fx_ch_depth", depth).s ("fx_ch_mix", mix)
    def delay (self, div, fb, mix, ping=1): return self.s ("fx_dl_on", 1).s ("fx_dl_div", div).s ("fx_dl_fb", fb).s ("fx_dl_mix", mix).s ("fx_dl_ping", ping)
    def reverb (self, size, damp, width, mix): return self.s ("fx_rv_on", 1).s ("fx_rv_size", size).s ("fx_rv_damp", damp).s ("fx_rv_width", width).s ("fx_rv_mix", mix)
    def comp (self, th, ratio, att, rel, mk): return self.s ("fx_cp_on", 1).s ("fx_cp_thresh", th).s ("fx_cp_ratio", ratio).s ("fx_cp_attack", att).s ("fx_cp_release", rel).s ("fx_cp_makeup", mk)
    def seq (self, mode, trigger=0, rate=RATE_116, length=16, gate=0.8, swing=0.0, octave=0):
        return self.s ("seq_mode", mode).s ("seq_trigger", trigger).s ("seq_rate", rate).s ("seq_length", length) \
                   .s ("seq_gate", gate).s ("seq_swing", swing).s ("seq_octave", octave)
    def arp (self, mode, octaves): return self.s ("arp_mode", mode).s ("arp_octaves", octaves)

def clean (p):
    # sempre define os 3 osciladores, sub e noise (o INIT tem OSC2 e SUB ligados)
    for i in (1, 2, 3):
        if f"osc{i}_level" not in p: p.off (i)
    if "sub_level" not in p: p.sub (SINE, -1, 0.0)
    return p

presets = []   # (nome, categoria, P, pattern)
# nomes dos presets feitos a mao (FactoryPresets.cpp): a biblioteca nunca repete um nome
import re
names = set (re.findall (r'make \("([^"]+)"', open (os.path.join (ROOT, "Source", "Presets", "FactoryPresets.cpp")).read()))
def add (name, cat, p, pattern=""):
    base, n = name, 2
    while name in names:
        name = f"{base} {['II', 'III', 'IV', 'V', 'VI'][min (n - 2, 4)]}"; n += 1
    names.add (name)
    presets.append ((name, cat, clean (p), pattern))

# =============================================================================
#  BASS
# =============================================================================
def sub_bass (name):
    p = P().osc (1, pick ([SINE, TRI]), 0.55).sub (SINE, pick ([-1, -2]), 0.95)
    p.flt (LP12, uni (180, 420), 0.05, uni (0.0, 0.25), 0.1, 0.0).amp (0.004, 0.3, 1.0, uni (0.08, 0.2))
    p.voice (MONO, 0, pick ([0.0, 0.03, 0.06])).mono (120)
    if rng.random() < 0.5: p.dist (DIST_SOFT, uni (0.15, 0.35), 0.4)
    add (name, "BASS", p.macros (0.4, -0.3, 0.2))

def metal_bass (name, heavy):
    p = P().osc (1, SAW, 0.6).osc (2, pick ([SQUARE, SAW]), 0.45, 0, 0, uni (-9, 9, 1)).sub (SINE, -1, 0.7)
    p.uni (pick ([1, 2, 3]), uni (0.1, 0.25), 0.3)
    p.flt (LP24, uni (500, 1400), uni (0.1, 0.35), uni (0.2, 0.5), uni (0.3, 0.6)).fenv (0.001, uni (0.12, 0.35), uni (0.1, 0.35), 0.15)
    p.amp (0.002, 0.3, uni (0.8, 1.0), uni (0.08, 0.15)).voice (MONO, 0, 0.0).mono (pick ([120, 150]))
    p.dist (pick ([DIST_SOFT, DIST_HARD]) if heavy else DIST_SOFT, uni (0.4, 0.75) if heavy else uni (0.15, 0.35), uni (0.45, 0.7))
    p.eq (uni (0, 2.5, 1), uni (1, 4, 1), uni (-3, 0, 1)).comp (-18, 4, 5, 90, 3)
    p.lfo (1, pick ([L_SINE, L_SQ, L_SAW]), True, pick ([D1_8, D1_16, D1_4]), 0.0).mod (2, S_LFO1, D_CUT, 0.3)
    add (name, "BASS", p.macros())

def fm_bass (name):
    ratio = pick ([R1, R2, R3])
    p = P().osc (1, FM, 0.7, 0, 0, 0, 0, fmr=ratio, fma=uni (0.25, 0.55)).sub (SINE, -1, 0.75)
    p.flt (LP24, uni (1800, 5000), 0.1, 0.1, 0.2).amp (0.002, 0.35, uni (0.7, 1.0), 0.12)
    p.menv (0.001, uni (0.12, 0.4), uni (0.0, 0.3), 0.1).mod (1, S_MENV, D_O1FM, uni (0.25, 0.45))   # "latido" do FM no ataque
    p.voice (MONO, 0, pick ([0.0, 0.02])).mono (120)
    p.dist (DIST_SOFT, uni (0.2, 0.5), 0.6).comp (-16, 3, 8, 100, 2)
    add (name, "BASS", p.macros (0.5, -0.4, 0.3))

def wt_bass (name):
    table = pick ([T_FORMANT, T_GRIT, T_GROWL, T_SYNC])
    p = P().osc (1, WT, 0.7, 0, 0, 0, 0, wt=table, wtpos=uni (0.1, 0.5)).osc (2, SAW, 0.3, -1).sub (SINE, -1, 0.7)
    p.uni (2, uni (0.1, 0.2), 0.3).flt (LP24, uni (900, 2500), uni (0.15, 0.4), 0.2, 0.25)
    p.amp (0.002, 0.3, 0.95, 0.12).voice (MONO, 0, pick ([0.0, 0.03])).mono (150)
    p.lfo (1, pick ([L_SINE, L_TRI, L_SAW]), True, pick ([D1_4, D1_8, D1_8T]), 1.0).mod (1, S_LFO1, D_O1WT, uni (0.2, 0.45))
    p.dist (pick ([DIST_SOFT, DIST_HARD]), uni (0.3, 0.6), 0.65).comp (-18, 4, 5, 80, 3)
    add (name, "BASS", p.macros())

def reese (name):
    f = uni (8, 18, 1)
    p = P().osc (1, SAW, 0.6, 0, 0, -f, -0.3).osc (2, SAW, 0.6, 0, 0, f, 0.3).sub (SINE, -1, 0.65)
    p.uni (pick ([3, 4, 5]), uni (0.3, 0.5), 0.5).flt (LP24, uni (400, 900), uni (0.2, 0.35), uni (0.2, 0.4), 0.15)
    p.amp (0.006, 0.3, 1.0, 0.2).voice (LEGATO, 0, uni (0.05, 0.12)).mono (150)
    p.lfo (1, L_SINE, True, pick ([D1_1, D1_2]), 1.0).mod (2, S_LFO1, D_CUT, uni (0.1, 0.2))
    if rng.random() < 0.6: p.dist (DIST_SOFT, uni (0.2, 0.45), 0.5)
    add (name, "BASS", p.macros())

def growl (name, cat):
    table = pick ([T_GROWL, T_VOCAL, T_FORMANT])
    p = P().osc (1, WT, 0.7, 0, 0, 0, 0, wt=table, wtpos=uni (0.2, 0.5)).osc (2, SQUARE, 0.35, -1).sub (SINE, -1, 0.7)
    p.flt (pick ([BP, LP24]), uni (600, 1200), uni (0.45, 0.7), uni (0.4, 0.7), 0.0, 0.2)
    p.amp (0.002, 0.3, 1.0, 0.1).voice (MONO, 0, uni (0.01, 0.03)).mono (150).uni (2, 0.15, 0.4)
    p.lfo (1, pick ([L_SINE, L_TRI, L_SAW, L_RSAW]), True, pick ([D1_8, D1_16, D1_8T, D1_4]), 1.0)
    p.mod (1, S_LFO1, D_O1WT, uni (0.3, 0.5)).mod (2, S_LFO1, D_CUT, uni (0.3, 0.6))
    p.dist (pick ([DIST_HARD, DIST_FUZZ]), uni (0.45, 0.8), 0.6).comp (-20, 5, 3, 60, 4).eq (1, 2, -2)
    add (name, cat, p.mod (5, S_M1, D_DRIVE, 0.4).mod (6, S_M2, D_L1A, 1.0).mod (7, S_M3, D_RES, 0.3).mod (8, S_M4, D_O1WT, 0.4))

def bass808 (name, distorted):
    p = P().osc (1, SINE, 0.9).sub (SINE, -1, 0.0)
    p.flt (LP12, 8000, 0.0, 0.0, 0.0, 0.0).fenv (0.001, uni (0.08, 0.16), 0.0, 0.1)
    p.amp (0.001, uni (1.0, 2.2), pick ([0.0, 0.0, 0.5]), uni (0.2, 0.4)).voice (LEGATO, 0, uni (0.05, 0.12)).mono (150)
    p.mod (1, S_FENV, D_PITCH, uni (0.18, 0.35))
    if distorted: p.dist (pick ([DIST_SOFT, DIST_HARD]), uni (0.35, 0.7), uni (0.5, 0.8))
    else:         p.dist (DIST_SOFT, uni (0.1, 0.25), 0.5)
    add (name, "TRAP", p.mod (5, S_M1, D_DRIVE, 0.4).mod (6, S_M2, D_AMP, -0.3).mod (7, S_M3, D_CUT, -0.4))

def breakdown_bass (name):
    p = P().osc (1, SAW, 0.6).osc (2, SQUARE, 0.5, 0, 0, uni (-7, 7, 1)).sub (SINE, -1, 0.8)
    p.flt (LP24, uni (250, 500), uni (0.2, 0.35), 0.4, uni (0.7, 0.9)).fenv (0.001, uni (0.08, 0.14), 0.0, 0.08)
    p.amp (0.001, uni (0.18, 0.3), uni (0.3, 0.6), 0.06).voice (MONO, 0, 0.0).mono (150)
    p.dist (DIST_HARD, uni (0.5, 0.8), 0.6).comp (-20, 6, 1, 60, 4)
    add (name, "BREAKDOWN", p.macros (0.4, -0.3, 0.3))

# ---- familias com variedade DELIBERADA (cada indice muda forma de onda, distorcao, envelope e filtro) ----
SUBS = [  # (nome, osc1, sub_oct, dist, cutoff)
    ("Deep Sub",     SINE, -2, None,        220),
    ("Round Sub",    TRI,  -1, None,        300),
    ("Pure Sub",     SINE, -1, None,        160),
    ("Heavy Sub",    SINE, -1, DIST_HARD,   260),
    ("Warm Sub",     TRI,  -2, DIST_SOFT,   380),
]
for n, w, so, d, cut in SUBS:
    p = P().osc (1, w, 0.55).sub (SINE, so, 0.95).flt (LP12, cut, 0.05, 0.1, 0.1, 0.0).amp (0.004, 0.3, 1.0, 0.12)
    p.voice (MONO, 0, 0.03 if w == TRI else 0.0).mono (120)
    if d is not None: p.dist (d, 0.35 if d == DIST_HARD else 0.25, 0.35)
    add (n, "BASS", p.macros (0.4, -0.3, 0.2))

METAL = [  # (nome, osc1, osc2, osc2_oct, dist, drive, filtro, pluck?, unison)
    ("Modern Metal Bass",   SAW,    SQUARE, 0,  DIST_SOFT, 0.35, LP24, False, 2),
    ("Grinding Metal Bass", SAW,    SAW,    -1, DIST_HARD, 0.65, LP24, False, 3),
    ("Crushing Metal Bass", SQUARE, SAW,    0,  DIST_FUZZ, 0.60, LP12, False, 1),
    ("Iron Metal Bass",     PULSE,  SQUARE, -1, DIST_HARD, 0.50, LP24, True,  1),
    ("Brutal Metal Bass",   SAW,    PULSE,  1,  DIST_RECT, 0.45, LP24, False, 2),
    ("Tight Metal Bass",    SQUARE, SQUARE, 0,  DIST_SOFT, 0.25, LP24, True,  1),
    ("Driven Metal Bass",   SAW,    SQUARE, -1, DIST_FUZZ, 0.40, BP,   False, 2),
    ("Steel Metal Bass",    PULSE,  SAW,    0,  DIST_HARD, 0.75, LP12, True,  3),
]
for n, w1, w2, o2, d, drv, ft, pl, un in METAL:
    # RECTIFY/FUZZ/BP transformam estereo do unison em diferenca no grave: nesses, unison sem spread
    wide = 0.0 if (d in (DIST_RECT, DIST_FUZZ) or ft == BP) else 0.3
    p = P().osc (1, w1, 0.6).osc (2, w2, 0.45, o2, 0, 6).sub (SINE, -1, 0.7).uni (un, 0.18, wide)
    p.flt (ft, 1100 if ft == BP else (500 if pl else 1200), 0.3, 0.3, 0.8 if pl else 0.35)
    p.fenv (0.001, 0.12 if pl else 0.35, 0.0 if pl else 0.3, 0.12).amp (0.002, 0.25 if pl else 0.3, 0.55 if pl else 0.95, 0.1)
    p.voice (MONO, 0, 0.0).mono (200 if ft == BP else 150).dist (d, drv, 0.6).eq (2, 3, -2).comp (-18, 4, 5, 90, 3)
    p.lfo (1, L_SQ, True, D1_16, 0.0).mod (2, S_LFO1, D_CUT, 0.3)
    add (n, "BASS", p.macros())

for i, (n, r, amt, dec) in enumerate ([("Barking FM Bass", R2, 0.45, 0.15), ("Metallic FM Bass", R3, 0.55, 0.3),
                                       ("Digital FM Bass", R1, 0.35, 0.5), ("Snappy FM Bass", R4, 0.6, 0.08),
                                       ("Hollow FM Bass", R05, 0.3, 0.4), ("Screech FM Bass", R7, 0.7, 0.2)]):
    p = P().osc (1, FM, 0.7, 0, 0, 0, 0, fmr=r, fma=amt).sub (SINE, -1, 0.75)
    p.flt (LP24, 3500, 0.1, 0.1, 0.2).amp (0.002, 0.35, 0.85, 0.12)
    p.menv (0.001, dec, 0.1, 0.1).mod (1, S_MENV, D_O1FM, 0.35).voice (MONO, 0, 0.0).mono (120)
    p.dist ([DIST_SOFT, DIST_HARD][i % 2], 0.35, 0.6).comp (-16, 3, 8, 100, 2)
    add (n, "BASS", p.macros (0.5, -0.4, 0.3))

WT_NAMES = ["Morphing", "Formant", "Grit", "Vocal", "Growling", "PWM", "Sync"]
DIVS = [D1_4, D1_8, D1_8T, D1_16, D1_2, D1_4T, D1_8]
for t in range (7):
    p = P().osc (1, WT, 0.7, 0, 0, 0, 0, wt=t, wtpos=0.3).osc (2, SAW, 0.3, -1).sub (SINE, -1, 0.7)
    p.uni (2, 0.15, 0.3).flt (LP24, 1600, 0.3, 0.2, 0.25).amp (0.002, 0.3, 0.95, 0.12).voice (MONO, 0, 0.02).mono (150)
    p.lfo (1, [L_SINE, L_TRI, L_SAW, L_RSAW][t % 4], True, DIVS[t], 1.0).mod (1, S_LFO1, D_O1WT, 0.35)
    p.dist ([DIST_SOFT, DIST_HARD, DIST_FUZZ][t % 3], 0.45, 0.65).comp (-18, 4, 5, 80, 3)
    add (WT_NAMES[t] + " Wavetable Bass", "BASS", p.macros())

for n, f, cut, div, d in [("Wide Reese", 14, 700, D1_1, DIST_SOFT), ("Dark Reese", 9, 380, D1_2, None),
                          ("Rolling Reese", 18, 900, D1_4, DIST_HARD), ("Menacing Reese", 22, 520, D1_1, DIST_FUZZ)]:
    p = P().osc (1, SAW, 0.6, 0, 0, -f, -0.3).osc (2, SAW, 0.6, 0, 0, f, 0.3).sub (SINE, -1, 0.65)
    p.uni (4, 0.4, 0.5).flt (LP24, cut, 0.3, 0.3, 0.15).amp (0.006, 0.3, 1.0, 0.2).voice (LEGATO, 0, 0.08).mono (150)
    p.lfo (1, L_SINE, True, div, 1.0).mod (2, S_LFO1, D_CUT, 0.18)
    if d is not None: p.dist (d, 0.35, 0.5)
    add (n, "BASS", p.macros())

for i, (n, t, cat) in enumerate ([("Snarling Growl", T_GROWL, "DUBSTEP"), ("Talking Growl", T_VOCAL, "DUBSTEP"),
                                  ("Chewing Growl", T_FORMANT, "DUBSTEP"), ("Roaring Growl", T_GROWL, "DUBSTEP"),
                                  ("Filthy Growl", T_GRIT, "DUBSTEP"), ("Neuro Growl", T_SYNC, "BASS")]):
    p = P().osc (1, WT, 0.7, 0, 0, 0, 0, wt=t, wtpos=0.35).osc (2, SQUARE, 0.35, -1).sub (SINE, -1, 0.7)
    p.flt ([BP, LP24][i % 2], 800, 0.6, 0.5, 0.0, 0.2).amp (0.002, 0.3, 1.0, 0.1).voice (MONO, 0, 0.02).mono (200).uni (2, 0.15, 0.0 if i % 3 == 2 else 0.4)
    p.lfo (1, [L_SINE, L_TRI, L_SAW, L_RSAW, L_SQ, L_SH][i], True, [D1_8, D1_16, D1_8T, D1_4, D1_16, D1_32][i], 1.0)
    p.mod (1, S_LFO1, D_O1WT, 0.4).mod (2, S_LFO1, D_CUT, 0.45)
    p.dist ([DIST_HARD, DIST_FUZZ, DIST_RECT][i % 3], 0.6, 0.6).comp (-20, 5, 3, 60, 4).eq (1, 2, -2)
    add (n, cat, p.mod (5, S_M1, D_DRIVE, 0.4).mod (6, S_M2, D_L1A, 1.0).mod (7, S_M3, D_RES, 0.3).mod (8, S_M4, D_O1WT, 0.4))

for n, pitch, dec, sus, d, drv, gl in [("808 Clean", 0.25, 1.6, 0.0, DIST_SOFT, 0.15, 0.06), ("808 Long", 0.2, 2.2, 0.5, DIST_SOFT, 0.2, 0.08),
                                       ("808 Slide", 0.3, 1.4, 0.0, DIST_SOFT, 0.2, 0.14), ("808 Distorted", 0.3, 1.5, 0.0, DIST_HARD, 0.55, 0.06),
                                       ("808 Crushed", 0.35, 0.9, 0.0, DIST_RECT, 0.5, 0.04), ("808 Fuzz Glide", 0.22, 1.8, 0.3, DIST_FUZZ, 0.6, 0.16)]:
    p = P().osc (1, SINE, 0.9).sub (SINE, -1, 0.0).flt (LP12, 8000, 0.0, 0.0, 0.0, 0.0).fenv (0.001, 0.12, 0.0, 0.1)
    p.amp (0.001, dec, sus, 0.3).voice (LEGATO, 0, gl).mono (150).mod (1, S_FENV, D_PITCH, pitch).dist (d, drv, 0.6)
    add (n, "TRAP", p.mod (5, S_M1, D_DRIVE, 0.4).mod (6, S_M2, D_AMP, -0.3).mod (7, S_M3, D_CUT, -0.4))

for n, w, d, cut in [("Breakdown Sub Hit", SINE, DIST_SOFT, 300), ("Breakdown Chug Bass", SAW, DIST_HARD, 400),
                     ("Breakdown Fuzz Bass", SQUARE, DIST_FUZZ, 520), ("Breakdown FM Snap", FM, DIST_HARD, 900)]:
    p = P().osc (1, w, 0.65, 0, 0, 0, 0, fmr=R2 if w == FM else None, fma=0.5 if w == FM else None).osc (2, SQUARE, 0.4, -1).sub (SINE, -1, 0.85)
    p.flt (LP24, cut, 0.3, 0.4, 0.85).fenv (0.001, 0.1, 0.0, 0.08).amp (0.001, 0.25, 0.4, 0.06).voice (MONO, 0, 0.0).mono (150)
    p.dist (d, 0.65, 0.6).comp (-20, 6, 1, 60, 4)
    add (n, "BREAKDOWN", p.macros (0.4, -0.3, 0.3))

# =============================================================================
#  LEADS
# =============================================================================
def lead (name, kind):
    p = P()
    if kind == "saw":    p.osc (1, SAW, 0.6).osc (2, SAW, 0.45, 0, pick ([0, 7, 12]), uni (-8, 8, 1))
    elif kind == "sq":   p.osc (1, SQUARE, 0.6).osc (2, PULSE, 0.4, 1)
    elif kind == "fm":   p.osc (1, FM, 0.65, 0, 0, 0, 0, fmr=pick ([R2, R3, R4]), fma=uni (0.25, 0.5)).osc (2, SAW, 0.3, 0, 0, 5)
    else:                p.osc (1, WT, 0.65, 0, 0, 0, 0, wt=pick ([T_SYNC, T_FORMANT, T_MORPH]), wtpos=uni (0.4, 0.8)).osc (2, SAW, 0.3, 0, 0, -6)
    p.uni (pick ([1, 2, 3]), uni (0.1, 0.25), uni (0.4, 0.8))
    p.flt (pick ([LP24, LP12, BP]) if kind != "sq" else LP24, uni (1800, 5000), uni (0.15, 0.5), uni (0.1, 0.4), uni (0.15, 0.35), 0.5)
    p.fenv (0.002, uni (0.2, 0.5), 0.5, 0.2).amp (0.003, 0.3, uni (0.85, 1.0), uni (0.12, 0.3))
    p.voice (LEGATO, 0, uni (0.03, 0.08)).vibrato()
    p.dist (DIST_SOFT, uni (0.15, 0.4), 0.7)
    p.delay (pick ([DL_18D, DL_14T, DL_18]), uni (0.25, 0.4), uni (0.12, 0.22))
    p.reverb (uni (0.5, 0.75), 0.5, 1.0, uni (0.12, 0.22))
    if kind == "wt": p.lfo (1, L_TRI, True, pick ([D1_2, D1_1]), 1.0).mod (2, S_LFO1, D_O1WT, 0.2)
    add (name, "LEADS", p.macros (0.4, -0.35, 0.4))

lead_names = ["Aggressive", "Screaming", "Soaring", "Razor", "Cutting", "Anthem", "Burning", "Dark", "Digital", "Wide",
              "Cyber", "Industrial", "Siren", "Laser", "Hybrid", "Atmospheric", "Neon", "Vicious", "Electric", "Cinematic"]
kinds = ["saw", "sq", "fm", "wt"]
for i, n in enumerate (lead_names):
    lead (f"{n} Lead", kinds[i % 4])

# =============================================================================
#  PADS / ATMOSPHERE
# =============================================================================
def pad (name, cat):
    p = P()
    kind = pick (["wt", "saw", "fm", "pwm"])
    if kind == "wt":   p.osc (1, WT, 0.55, 0, 0, 0, -0.3, wt=pick ([T_VOCAL, T_MORPH, T_FORMANT]), wtpos=uni (0.2, 0.6)).osc (2, WT, 0.45, 1, 0, 6, 0.3, wt=T_MORPH, wtpos=0.3)
    elif kind == "saw": p.osc (1, SAW, 0.5, 0, 0, -9, -0.3).osc (2, SAW, 0.5, 0, pick ([0, 7]), 9, 0.3)
    elif kind == "fm": p.osc (1, FM, 0.5, 0, 0, 0, 0, fmr=pick ([R05, R2, R3]), fma=uni (0.1, 0.3)).osc (2, TRI, 0.45, 1)
    else:              p.osc (1, WT, 0.55, 0, 0, 0, 0, wt=T_PWM, wtpos=0.2).osc (2, WT, 0.5, 0, 0, 7, 0.4, wt=T_PWM, wtpos=0.6)
    if cat == "ATMOSPHERE": p.noise (1, uni (0.04, 0.1))
    p.uni (pick ([4, 6, 8]), uni (0.15, 0.35), 1.0)
    p.flt (pick ([LP12, LP24]), uni (700, 2500), uni (0.1, 0.3), 0.0, 0.1)
    p.amp (uni (0.4, 2.0), 1.0, uni (0.8, 0.95), uni (1.2, 3.0))
    p.lfo (1, pick ([L_SINE, L_TRI]), False, uni (0.05, 0.25), 1.0).mod (2, S_LFO1, D_O1WT if kind in ("wt", "pwm") else D_CUT, uni (0.15, 0.3))
    p.lfo (2, L_SINE, False, uni (0.06, 0.15), 1.0).mod (3, S_LFO2, D_PAN, uni (0.15, 0.35))
    p.chorus (uni (0.2, 0.6), uni (0.3, 0.5), uni (0.3, 0.5))
    p.reverb (uni (0.72, 0.88), uni (0.45, 0.7), 1.0, uni (0.28, 0.42))
    add (name, cat, p.mod (5, S_M1, D_CUT, 0.3).mod (6, S_M2, D_L1A, 1.0).mod (7, S_M3, D_CUT, -0.45).mod (8, S_M4, D_UNIDET, 0.4))

pad_names = ["Dark Horizon", "Cold Steel", "Frozen Cathedral", "Endless Night", "Ashes", "Black Glass", "Distant Choir",
             "Nebula", "Dystopia", "Hollow Sky", "Grey Dawn", "Emotional Wide", "Tension Rise", "Slow Burn", "Midnight Drift"]
atmo_names = ["Industrial Haze", "Broken Signal", "Ghost Factory", "Machine Breath", "Toxic Fog", "Static Ruins",
              "Rusted Air", "Dead Frequency", "Intro Void", "Breakdown Smoke", "Dust Storm", "Abandoned Station",
              "Metal Rain", "Cinematic Tension", "Underworld"]
for n in pad_names:  pad (n + " Pad", "PADS")
for n in atmo_names: pad (n, "ATMOSPHERE")

# =============================================================================
#  PLUCKS / STABS / KEYS
# =============================================================================
def pluck (name, cat):
    p = P()
    kind = pick (["saw", "sq", "fm", "wt"])
    if kind == "saw":  p.osc (1, SAW, 0.6).osc (2, SQUARE, 0.4, 0, 0, 6)
    elif kind == "sq": p.osc (1, SQUARE, 0.6).osc (2, PULSE, 0.35, 1)
    elif kind == "fm": p.osc (1, FM, 0.65, 0, 0, 0, 0, fmr=pick ([R2, R3, R4]), fma=uni (0.3, 0.5)).menv (0.001, 0.12, 0.0, 0.1).mod (1, S_MENV, D_O1FM, 0.35)
    else:              p.osc (1, WT, 0.65, 0, 0, 0, 0, wt=pick ([T_GRIT, T_SYNC, T_FORMANT]), wtpos=uni (0.3, 0.7))
    stab = cat == "STABS"
    p.uni (pick ([2, 4]) if stab else 1, 0.2, 0.7)
    p.flt (LP24, uni (300, 900), uni (0.25, 0.5), uni (0.1, 0.3), uni (0.6, 0.85), 0.5).fenv (0.001, uni (0.1, 0.25), 0.0, 0.12)
    p.amp (0.001, uni (0.25, 0.45) if stab else uni (0.18, 0.4), 0.0, uni (0.15, 0.3))
    p.mod (2, S_VEL, D_CUT, 0.25)
    if stab: p.dist (pick ([DIST_HARD, DIST_SOFT]), uni (0.3, 0.6), 0.7).comp (-18, 4, 3, 80, 3)
    p.delay (pick ([DL_18, DL_18D, DL_116]), uni (0.2, 0.35), uni (0.12, 0.25)).reverb (uni (0.4, 0.7), 0.5, 1.0, uni (0.1, 0.2))
    add (name, cat, p)

def keys (name):
    p = P()
    kind = pick (["fm", "tri", "pwm"])
    if kind == "fm":  p.osc (1, FM, 0.6, 0, 0, 0, 0, fmr=pick ([R1, R2, R4]), fma=uni (0.15, 0.35)).osc (2, SINE, 0.3, 1)
    elif kind == "tri": p.osc (1, TRI, 0.65).osc (2, SAW, 0.2, 1, 0, 4)
    else:             p.osc (1, WT, 0.6, 0, 0, 0, 0, wt=T_PWM, wtpos=uni (0.1, 0.4)).osc (2, SINE, 0.25, 1)
    p.menv (0.001, uni (0.3, 0.8), 0.0, 0.3).mod (1, S_MENV, D_O1FM if kind == "fm" else D_CUT, 0.3)
    p.flt (LP12, uni (1500, 3500), 0.1, 0.0, 0.3, 0.6).amp (0.002, uni (1.2, 2.5), uni (0.05, 0.25), uni (0.4, 0.8))
    p.mod (2, S_VEL, D_CUT, 0.3).chorus (uni (0.3, 0.8), 0.3, uni (0.2, 0.35)).reverb (uni (0.5, 0.8), 0.5, 1.0, uni (0.15, 0.3))
    add (name, "KEYS", p)

for n in ["Digital", "Metal", "Dark", "Glass", "Iron", "Cyber", "Neon", "Crystal", "Shadow", "Cold",
          "Bright", "Hollow", "Tight", "Snappy", "Distant"]:
    pluck (n + " Pluck", "PLUCKS")
for n in ["Industrial", "Heavy", "Glitch", "Cinematic", "Electronic Rock", "Brutal", "Digital", "Hybrid", "Power", "Dark"]:
    pluck (n + " Stab", "STABS")
for n in ["Processed Piano", "Broken Piano", "Wide Piano", "Ambient Keys", "Industrial Keys", "Cold Keys",
          "Cinematic Piano", "Dark Electric Piano", "Digital Bells", "Metal Bells", "Glass Keys", "Soft Night Keys"]:
    keys (n)

# =============================================================================
#  ARPS e SEQUENCES (usam o sequencer real, sincronizado ao REAPER)
# =============================================================================
def arp_preset (name, mode, rate, octaves):
    p = P().osc (1, pick ([SAW, SQUARE]), 0.6).osc (2, pick ([SQUARE, PULSE]), 0.3, 1).uni (2, 0.15, 0.6)
    p.flt (LP24, uni (600, 1400), uni (0.3, 0.5), 0.2, uni (0.5, 0.7), 0.4).fenv (0.001, uni (0.1, 0.2), 0.05, 0.1)
    p.amp (0.001, uni (0.15, 0.3), 0.15, 0.12)
    p.seq (SEQ_ARP, 0, rate, 16, uni (0.5, 0.8), pick ([0.0, 0.0, 0.3])).arp (mode, octaves)
    p.delay (DL_18D, 0.3, 0.18).reverb (0.6, 0.5, 1.0, 0.15)
    add (name, "ARPS", p)

arp_list = [("Up", ARP_UP), ("Down", ARP_DOWN), ("Up Down", ARP_UPDOWN), ("Random", ARP_RANDOM), ("Order", ARP_ORDER)]
rates = [("16ths", RATE_116), ("8ths", RATE_18), ("Triplet", RATE_116T), ("32nds", RATE_132)]
k = 0
for rn, rv in rates:
    for an, av in arp_list:
        arp_preset (f"Arp {an} {rn}", av, rv, 1 + (k % 3)); k += 1

seq_patterns = ["Dark Pulse", "Cinematic Steps", "Verse Chug", "Offbeat Djent", "Cyber Gate", "Build 8ths to 16ths",
                "Pitch Climb", "Anthem Octaves", "Wide Chorus Arp", "Breakdown Stabs", "Chug Gate", "Half-Time Slam",
                "Drop Slide", "Floating Bridge", "Syncopated Bridge", "Fading Pulse"]
def seq_preset (name, pattern, bassy):
    p = P()
    if bassy: p.osc (1, SAW, 0.6).osc (2, SQUARE, 0.45, 0, 0, 7).sub (SINE, -1, 0.7).voice (MONO, 0, 0.02).mono (150).dist (DIST_HARD, 0.5, 0.6)
    else:     p.osc (1, WT, 0.6, 0, 0, 0, 0, wt=pick ([T_GRIT, T_SYNC, T_FORMANT]), wtpos=0.5).osc (2, SAW, 0.3, 1).uni (2, 0.15, 0.6).reverb (0.6, 0.5, 1.0, 0.15)
    p.flt (LP24, uni (500, 1200), uni (0.3, 0.5), 0.3, uni (0.5, 0.75), 0.4).fenv (0.001, uni (0.1, 0.2), 0.1, 0.1)
    p.amp (0.001, 0.25, 0.4, 0.1).seq (SEQ_ON, 0, RATE_116, 16, 0.8)
    add (name, "SEQUENCES", p, pattern)

for i, pat in enumerate (seq_patterns):
    seq_preset (f"Seq Bass {pat}" if i % 2 == 0 else f"Seq Synth {pat}", pat, i % 2 == 0)

# =============================================================================
#  saida C++
# =============================================================================
def cstr (s): return '"' + s.replace ('"', '\\"') + '"'
lines = ["// GERADO por tools/gen_library.py - nao editar a mao. Presets: %d" % len (presets)]
for name, cat, p, pattern in presets:
    vals = dict (p)
    vals["master_gain"] = float (gains.get (name, -8.0))   # nivel medido pelo QC (tools/library_gains.json)
    def fl (v):
        t = "%.6g" % v
        return t + (".0f" if ("." not in t and "e" not in t) else "f")
    body = ", ".join ("{ %s, %s }" % (cstr (k), fl (v)) for k, v in vals.items())
    lines.append ("{ %s, %s, { %s }, {}, %s, {} }," % (cstr (name), cstr (cat), body, cstr (pattern)))
open (OUT, "w").write ("\n".join (lines) + "\n")
cats = {}
for _, c, _, _ in presets: cats[c] = cats.get (c, 0) + 1
print ("presets gerados:", len (presets), cats)
