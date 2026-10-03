#!/usr/bin/env python3
# Gerador dos presets do SCRATCH MACHINE (70) -> Source/Presets/ScratchPresets.inc
# Cada preset = receita distinta; o gerador RECUSA duplicatas de comportamento.
import json, os, sys
ROOT = os.path.dirname (os.path.dirname (os.path.abspath (__file__)))
OUT = os.path.join (ROOT, "Source", "Presets", "ScratchPresets.inc")
GAINS = os.path.join (ROOT, "tools", "scratch_gains.json")
gains = json.load (open (GAINS)) if os.path.exists (GAINS) else {}
P = { n: i for i, n in enumerate (["Baby","Forward","Chirp","Transformer","Tear","Flare","Stab","Scribble","Backward","Orbit","Crab",
                                   "Slicer","Stutter","Reverse","Brake","Vinyl Stop","Rhythmic","Glitch","Random"]) }
S = { n: i for i, n in enumerate (["Vox AHH","Vox OHH","Vox EHH","Synth Stab","User Sample","Noise Sweep","808 Tone","Digital Chirp",
                                   "Metal Hit","Robot Vox","Vinyl Texture"]) }
R = { n: i for i, n in enumerate (["1/4","1/8","1/16","1/2","1/1","1/32","1/4T","1/8T","1/16T","1/32T"]) }
# carater de cada grupo (FX e textura)
GROUP = {
  "CLASSIC":      dict (vinyl=.2, noise=.12, tone=.85, res=.15, fx_reverb=.08),
  "HIP HOP":      dict (vinyl=.45, noise=.15, tone=.7, res=.2, fx_vinyl=.35, fx_sat=.25, fx_reverb=.06),
  "MODERN":       dict (vinyl=.05, noise=.05, tone=.95, res=.3, fx_crush=.15, fx_delay=.18, fx_width=.8),
  "METAL":        dict (vinyl=.1, noise=.2, tone=.8, res=.35, fx_drive=.55, fx_sat=.4, fx_reverb=.12),
  "METALCORE":    dict (vinyl=.08, noise=.15, tone=.9, res=.4, fx_drive=.35, fx_delay=.15, fx_reverb=.2, fx_width=.75),
  "EXPERIMENTAL": dict (vinyl=.3, noise=.3, tone=.75, res=.55, fx_crush=.4, fx_delay=.3, random=.7),
  "PERFORMANCE":  dict (vinyl=.12, noise=.1, tone=.88, res=.2, fx_reverb=.1),
}
# (nome, grupo, padrao, fonte, rate, extras)
T = [
 ("Baby Vinyl","CLASSIC","Baby","Vox AHH","1/4",{}),
 ("Classic DJ","CLASSIC","Baby","Vox OHH","1/8",{"curve":.4}),
 ("Forward Cut","CLASSIC","Forward","Vox AHH","1/8",{}),
 ("Back Cut","CLASSIC","Backward","Vox OHH","1/8",{}),
 ("Transformer","CLASSIC","Transformer","Vox AHH","1/4",{"fader":1.0}),
 ("Chirp","CLASSIC","Chirp","Vox EHH","1/8",{"fader":.95}),
 ("Flare","CLASSIC","Flare","Vox AHH","1/4",{"fader":1.0}),
 ("Orbit","CLASSIC","Orbit","Vox OHH","1/4",{"fader":1.0}),
 ("Crab","CLASSIC","Crab","Vox EHH","1/4",{"fader":1.0,"speed":1.2}),
 ("Stab","CLASSIC","Stab","Synth Stab","1/8",{}),
 ("Old School","HIP HOP","Baby","Vox OHH","1/4",{"momentum":.5,"tone":.6}),
 ("Boom Bap","HIP HOP","Chirp","Vox AHH","1/4",{"swing":.4}),
 ("Turntable","HIP HOP","Scribble","Vox AHH","1/4",{"variation":.3}),
 ("Golden Era","HIP HOP","Forward","Vox EHH","1/4",{"fx_sat":.4,"tone":.55}),
 ("Break Scratch","HIP HOP","Tear","Vox OHH","1/8",{}),
 ("Vinyl Funk","HIP HOP","Flare","Vox EHH","1/8",{"swing":.35,"fx_delay":.12}),
 ("Street Cut","HIP HOP","Stab","Vox AHH","1/16",{"speed":1.3}),
 ("Dusty Deck","HIP HOP","Baby","Vinyl Texture","1/2",{"vinyl":.8,"fx_vinyl":.6}),
 ("DJ Battle","HIP HOP","Crab","Vox OHH","1/8",{"speed":1.5,"fader":1.0}),
 ("Classic Break","HIP HOP","Backward","Vox AHH","1/4",{"swing":.25}),
 ("Trap Scratch","MODERN","Stutter","Vox EHH","1/16T",{"pitch":-5}),
 ("808 Scratch","MODERN","Baby","808 Tone","1/4",{"pitch":-12,"tone":.6}),
 ("Glitch Scratch","MODERN","Glitch","Digital Chirp","1/16",{}),
 ("Digital Vinyl","MODERN","Transformer","Digital Chirp","1/8",{"fx_crush":.3}),
 ("Cyber Scratch","MODERN","Orbit","Robot Vox","1/8",{}),
 ("Future Scratch","MODERN","Slicer","Vox AHH","1/4",{"pitch":7,"fx_delay":.25}),
 ("Hyper Scratch","MODERN","Scribble","Digital Chirp","1/16",{"speed":2.0,"variation":.9}),
 ("Electronic Cut","MODERN","Forward","Synth Stab","1/16",{"gate":.5}),
 ("Neon Vinyl","MODERN","Chirp","Robot Vox","1/8T",{"fx_width":1.0}),
 ("Digital DJ","MODERN","Rhythmic","Digital Chirp","1/8",{}),
 ("Metal Scratch","METAL","Baby","Metal Hit","1/8",{}),
 ("Heavy Cut","METAL","Forward","Metal Hit","1/4",{"pitch":-7}),
 ("Industrial Scratch","METAL","Transformer","Noise Sweep","1/8",{"fx_crush":.25}),
 ("Breakdown Scratch","METAL","Brake","Synth Stab","1/2",{"pitch":-5}),
 ("Distorted Vinyl","METAL","Baby","Vox AHH","1/8",{"fx_drive":.85}),
 ("Aggressive Cut","METAL","Stab","Metal Hit","1/16",{"speed":1.6}),
 ("Machine Scratch","METAL","Slicer","Robot Vox","1/8",{"fx_drive":.6}),
 ("Metal Glitch","METAL","Glitch","Metal Hit","1/16",{}),
 ("Brutal Scratch","METAL","Tear","Noise Sweep","1/4",{"fx_drive":.9,"pitch":-3}),
 ("Heavy Transformer","METAL","Transformer","Synth Stab","1/4",{"pitch":-12,"fader":1.0}),
 ("Modern Metalcore","METALCORE","Chirp","Vox AHH","1/8",{}),
 ("Electronicore","METALCORE","Rhythmic","Robot Vox","1/16",{}),
 ("Breakdown FX","METALCORE","Vinyl Stop","Synth Stab","1/1",{}),
 ("Chug Scratch","METALCORE","Stutter","Metal Hit","1/8",{"pitch":-5}),
 ("Cyber Metal","METALCORE","Flare","Digital Chirp","1/8",{"fx_crush":.2}),
 ("Architectural","METALCORE","Slicer","Noise Sweep","1/4",{"fx_reverb":.35}),
 ("Digital Breakdown","METALCORE","Brake","Digital Chirp","1/2",{}),
 ("Modern Breakdown","METALCORE","Reverse","Synth Stab","1/4",{"pitch":-7}),
 ("Heavy Glitch","METALCORE","Glitch","Robot Vox","1/8",{"fx_drive":.6}),
 ("Metalcore FX","METALCORE","Orbit","Vox EHH","1/4T",{"fx_delay":.3}),
 ("Glitch Orbit","EXPERIMENTAL","Orbit","Digital Chirp","1/16T",{"random":.9}),
 ("Granular Scratch","EXPERIMENTAL","Scribble","Vinyl Texture","1/32",{"variation":1.0}),
 ("Broken Vinyl","EXPERIMENTAL","Glitch","Vinyl Texture","1/8",{"vinyl":1.0,"fx_vinyl":.8}),
 ("Robot Scratch","EXPERIMENTAL","Transformer","Robot Vox","1/16",{"pitch":5}),
 ("Reverse Machine","EXPERIMENTAL","Reverse","Robot Vox","1/8",{"direction":1}),
 ("Digital Noise","EXPERIMENTAL","Transformer","Noise Sweep","1/16T",{"fx_crush":.45,"speed":1.4}),
 ("Cybernetic","EXPERIMENTAL","Stutter","Digital Chirp","1/32",{"pitch":12}),
 ("Destroyed Vinyl","EXPERIMENTAL","Tear","Vinyl Texture","1/4",{"fx_drive":.8,"fx_crush":.5}),
 ("Stutter Machine","EXPERIMENTAL","Stutter","Synth Stab","1/16",{"repeat":4}),
 ("Chaos Scratch","EXPERIMENTAL","Random","Vox OHH","1/8",{"random":1.0}),
 ("Sync Baby 8ths (Play)","PERFORMANCE","Baby","Vox AHH","1/8",{"mode":1}),
 ("Sync Chirp 16ths (Play)","PERFORMANCE","Chirp","Vox EHH","1/16",{"mode":1}),
 ("Triplet Flare (Play)","PERFORMANCE","Flare","Vox OHH","1/8T",{"mode":1}),
 ("Swing Transformer (Play)","PERFORMANCE","Transformer","Synth Stab","1/8",{"mode":1,"swing":.5}),
 ("Hold Scribble","PERFORMANCE","Scribble","Robot Vox","1/4",{"hold":1}),
 ("Loop Orbit","PERFORMANCE","Orbit","Vox AHH","1/4",{"loop":1}),
 ("Free Glitch","PERFORMANCE","Glitch","Digital Chirp","1/8",{"mode":2}),
 ("Half Bar Tear","PERFORMANCE","Tear","Vox EHH","1/2",{}),
 ("Sample Ready Baby","PERFORMANCE","Baby","Vox AHH","1/4",{"engine":1}),
 ("Sample Ready Transformer","PERFORMANCE","Transformer","Vox OHH","1/8",{"engine":1}),
]
assert len (T) == 70, len (T)
seen = set(); lines = ["// GERADO por tools/gen_scratch.py - 70 presets do SCRATCH MACHINE"]
for name, grp, pat, src, rate, extra in T:
    v = { "scr_on": 1, "scr_move": P[pat], "scr_source": S[src], "scr_rate": R[rate], "scr_level": 0.32 }
    base = dict (GROUP[grp]); base.update (extra)
    keymap = { "vinyl":"scr_vinyl","noise":"scr_noise","tone":"scr_tone","res":"scr_resonance","curve":"scr_curve","fader":"scr_fader",
               "speed":"scr_speed","momentum":"scr_momentum","swing":"scr_swing","variation":"scr_variation","pitch":"scr_pitch","gate":"scr_gate",
               "random":"scr_random","direction":"scr_direction","repeat":"scr_repeat","mode":"scr_mode","hold":"scr_hold","loop":"scr_loop",
               "engine":"scr_engine" }
    for k, x in base.items():
        v[keymap.get (k, "scr_" + k)] = float (x)
    # nivel: o scratch fica abaixo da protecao de saida; o nivelamento usa o MASTER do preset (depois da protecao)
    gm = float (gains.get (name, -6.0))
    v["master_gain"] = min (6.0, gm)
    if gm > 6.0: v["scr_level"] = min (1.0, v["scr_level"] * 10 ** ((gm - 6.0) / 20.0))   # ganho alem do master: nivel interno (fonte longe da protecao)
    sig = (pat, src, rate, round (v.get ("scr_speed",1),2), round (v.get ("scr_pitch",0),1), grp, round (v.get ("scr_fx_drive",0),2), v.get ("scr_mode",0))
    if sig in seen: sys.exit ("comportamento duplicado: " + name)
    seen.add (sig)
    body = "".join (' .set ("%s", %sf)' % (k, ("%.4g" % x) if "." in ("%.4g" % x) else ("%.4g" % x) + ".0") for k, x in v.items())
    lines.append ('make ("%s", "SCRATCH", B().scratchBase()%s),' % (name, body))
open (OUT, "w").write ("\n".join (lines) + "\n")
print ("presets:", len (T), "| comportamentos distintos:", len (seen))
