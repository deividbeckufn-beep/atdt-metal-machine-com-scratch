#include "FactoryPresets.h"
#include "Drums/DrumPattern.h"
#include "Parameters/Parameters.h"
#include "Sequencer/SequencePattern.h"

// =============================================================================
//  Presets de fabrica do ATDT SYNTH MACHINE.
//  Cada preset parte do INIT e altera apenas o necessario.
//  Macros padrao (slots 5-8 da matriz):
//    M1 AGGRESSION -> Drive      M2 MOVEMENT -> LFO1 Amount
//    M3 DARKNESS   -> Cutoff (-) M4 SPACE    -> Unison Detune
// =============================================================================
namespace mm
{
namespace
{
    // indices dos enums (mesma ordem de Parameters.h / ModulationDefs.h)
    enum Wave  { Sine = 0, Tri, Saw, Square, Pulse, NoiseW };
    enum Filt  { LP12 = 0, LP24, HP12, HP24, BP, NOTCH };
    enum Noise { White = 0, Pink, Digital };
    enum Lfo   { LSine = 0, LTri, LSaw, LSquare, LSH, LRandom };
    enum Div   { D2_1 = 0, D1_1, D1_2, D1_4, D1_8, D1_16, D1_32, D1_4T, D1_8T, D1_16T };
    enum Mode  { Poly = 0, Mono, Legato };
    enum SeqM  { SeqOff = 0, SeqOn, SeqArp };
    enum Trig  { TKey = 0, TLatch, TTransport };
    enum ArpM  { AUp = 0, ADown, AUpDown, ARandom, AOrder, AChord };
    enum Src   { SNone = 0, SLfo1, SLfo2, SLfo3, SLfo4, SFEnv, SAEnv, SVel, SMW, SAT, SNote, SM1, SM2, SM3, SM4 };
    enum Dst   { DNone = 0, DPitch, DOsc1P, DOsc2P, DOsc3P, DOsc1L, DOsc2L, DOsc3L, DSub, DNoise,
                 DCutoff, DReso, DDrive, DFMix, DAmp, DPan, DUniDet, DLfo1A, DLfo2A, DLfo3A, DLfo4A };

    struct B
    {
        std::vector<std::pair<juce::String, float>> v;
        juce::String patternName;
        juce::String drumName;

        // SCRATCH MACHINE: synth mudo (so o scratch toca nas teclas C4-G#4)
        B& scratchBase()
        {
            for (int o = 1; o <= 3; ++o) set (ParamIDs::osc (o, "level"), 0.0f);
            set (ParamIDs::subLevel, 0.0f);
            return *this;
        }

        // bateria: liga o DrumEngine com um padrao da biblioteca (toca junto com o play do REAPER)
        B& drums (const char* name, bool keysPlayDrums = false)
        {
            drumName = name;
            set (ParamIDs::drumOn, 1.0f);
            if (const int i = findDrumPattern (name); i >= 0)
                set (ParamIDs::drumLength, (float) getDrumLibrary()[(size_t) i].length);
            if (keysPlayDrums)   // kit puro: o teclado toca a bateria e o synth fica mudo
            {
                set (ParamIDs::drumKeys, 1.0f);
                for (int o = 1; o <= 3; ++o) set (ParamIDs::osc (o, "level"), 0.0f);
                set (ParamIDs::subLevel, 0.0f);
            }
            return *this;
        }

        // sequencia da biblioteca: tambem define LENGTH, RATE e FEEL do padrao
        B& pattern (const char* n)
        {
            patternName = n;
            if (const int i = findPattern (n); i >= 0)
            {
                const auto& d = getPatternLibrary()[(size_t) i];
                set (ParamIDs::seqLength, (float) juce::StringArray::fromTokens (d.steps, " ", {}).size());
                set (ParamIDs::seqRate, (float) d.rate).set (ParamIDs::timeFeel, (float) d.feel);
            }
            return *this;
        }
        B& seq (int trigger, int octave)
        {
            return set (ParamIDs::seqMode, (float) SeqOn).set (ParamIDs::seqTrigger, (float) trigger).set (ParamIDs::seqOctave, (float) octave);
        }
        B& arp (int mode, int octaves, int octave = 0)
        {
            return set (ParamIDs::seqMode, (float) SeqArp).set (ParamIDs::arpMode, (float) mode)
                  .set (ParamIDs::arpOctaves, (float) octaves).set (ParamIDs::seqOctave, (float) octave);
        }

        B& set (const juce::String& id, float x) { v.emplace_back (id, x); return *this; }

        B& osc (int i, int wave, float level, int oct = 0, int semi = 0, float fine = 0.0f, float pan = 0.0f)
        {
            return set (ParamIDs::osc (i, "wave"), (float) wave).set (ParamIDs::osc (i, "level"), level)
                  .set (ParamIDs::osc (i, "octave"), (float) oct).set (ParamIDs::osc (i, "semi"), (float) semi)
                  .set (ParamIDs::osc (i, "fine"), fine).set (ParamIDs::osc (i, "pan"), pan);
        }
        B& off (int i)                              { return set (ParamIDs::osc (i, "level"), 0.0f); }
        B& sub (int wave, int oct, float level)     { return set (ParamIDs::subWave, (float) wave).set (ParamIDs::subOctave, (float) oct).set (ParamIDs::subLevel, level); }
        B& noise (int type, float level)            { return set (ParamIDs::noiseType, (float) type).set (ParamIDs::noiseLevel, level); }

        B& filter (int type, float cutoff, float reso, float drive, float env, float keytrack = 0.3f, float mix = 1.0f)
        {
            return set (ParamIDs::filterType, (float) type).set (ParamIDs::cutoff, cutoff).set (ParamIDs::resonance, reso)
                  .set (ParamIDs::filterDrive, drive).set (ParamIDs::filterEnvAmt, env).set (ParamIDs::keyTrack, keytrack)
                  .set (ParamIDs::filterMix, mix);
        }
        B& fenv (float a, float d, float s, float r)
        {
            return set (ParamIDs::fenvAttack, a).set (ParamIDs::fenvDecay, d).set (ParamIDs::fenvSustain, s).set (ParamIDs::fenvRelease, r);
        }
        B& amp (float a, float d, float s, float r)
        {
            return set (ParamIDs::ampAttack, a).set (ParamIDs::ampDecay, d).set (ParamIDs::ampSustain, s).set (ParamIDs::ampRelease, r);
        }
        B& voice (int mode, int countIndex, float glide)
        {
            return set (ParamIDs::voiceMode, (float) mode).set (ParamIDs::voiceCount, (float) countIndex).set (ParamIDs::glide, glide);
        }
        B& unison (int n, float detune, float spread)
        {
            return set (ParamIDs::unisonVoices, (float) n).set (ParamIDs::unisonDetune, detune).set (ParamIDs::unisonSpread, spread);
        }
        B& mono (float hz)   { return set (ParamIDs::bassMono, hz); }
        B& gain (float db)   { return set (ParamIDs::masterGain, db); }

        B& lfoSync (int i, int shape, int div, float amount)
        {
            return set (ParamIDs::lfo (i, "shape"), (float) shape).set (ParamIDs::lfo (i, "sync"), 1.0f)
                  .set (ParamIDs::lfo (i, "div"), (float) div).set (ParamIDs::lfo (i, "amount"), amount);
        }
        B& lfoFree (int i, int shape, float hz, float amount)
        {
            return set (ParamIDs::lfo (i, "shape"), (float) shape).set (ParamIDs::lfo (i, "sync"), 0.0f)
                  .set (ParamIDs::lfo (i, "rate"), hz).set (ParamIDs::lfo (i, "amount"), amount);
        }
        B& mod (int slot, int src, int dst, float amt)
        {
            return set (ParamIDs::mod (slot, "src"), (float) src).set (ParamIDs::mod (slot, "dst"), (float) dst)
                  .set (ParamIDs::mod (slot, "amt"), amt);
        }
        // vibrato no mod wheel: MW -> LFO4 Amount, LFO4 -> Pitch
        B& vibrato()
        {
            return lfoFree (4, LSine, 5.5f, 0.0f).mod (3, SMW, DLfo4A, 1.0f).mod (4, SLfo4, DPitch, 0.012f);
        }
        B& macros (float drive = 0.6f, float dark = -0.45f, float space = 0.5f)
        {
            return mod (5, SM1, DDrive, drive).mod (6, SM2, DLfo1A, 1.0f).mod (7, SM3, DCutoff, dark).mod (8, SM4, DUniDet, space);
        }
    };

    FactoryPreset make (const char* name, const char* category, const B& b, juce::StringArray macroNames = {})
    {
        return { name, category, b.v, macroNames, b.patternName, b.drumName };
    }
}

const std::vector<FactoryPreset>& getFactoryPresets()
{
    static const std::vector<FactoryPreset> list
    {
        // =========================================================== BASS
        make ("Modern Metal Sub", "BASS", B().gain (3.3f).osc (1, Saw, 0.55f).osc (2, Square, 0.4f, 0, 0, -7.0f).off (3).sub (Sine, -1, 0.8f)
              .filter (LP24, 900.0f, 0.2f, 0.35f, 0.45f).fenv (0.002f, 0.35f, 0.3f, 0.2f).amp (0.002f, 0.3f, 0.9f, 0.12f)
              .voice (Mono, 0, 0.03f).unison (3, 0.15f, 0.4f).mono (120.0f)
              .lfoSync (1, LSine, D1_8, 0.0f).mod (2, SLfo1, DCutoff, 0.3f).macros()),

        make ("Distorted Sub", "BASS", B().gain (10.7f).osc (1, Tri, 0.5f, -1).off (2).off (3).sub (Sine, -1, 1.0f)
              .filter (LP12, 1800.0f, 0.1f, 0.9f, 0.2f).amp (0.002f, 0.3f, 1.0f, 0.1f)
              .voice (Mono, 0, 0.02f).mono (150.0f).lfoSync (1, LSquare, D1_16, 0.0f).mod (2, SLfo1, DCutoff, 0.25f).macros (0.4f)),

        make ("Aggressive Bass", "BASS", B().gain (2.3f).osc (1, Saw, 0.6f).osc (2, Saw, 0.5f, 0, 0, 12.0f).osc (3, Square, 0.5f, -1).sub (Sine, -1, 0.5f)
              .filter (LP24, 700.0f, 0.35f, 0.7f, 0.6f).fenv (0.001f, 0.18f, 0.15f, 0.15f).amp (0.001f, 0.3f, 1.0f, 0.1f)
              .voice (Mono, 0, 0.0f).unison (5, 0.3f, 0.5f).mono (120.0f)
              .lfoSync (1, LSaw, D1_8, 0.0f).mod (2, SLfo1, DCutoff, 0.35f).macros()),

        make ("Industrial Bass", "BASS", B().gain (7.6f).osc (1, Square, 0.6f).osc (2, Pulse, 0.35f, 0, 7).off (3).sub (Square, -1, 0.35f).noise (Digital, 0.15f)
              .filter (LP24, 1200.0f, 0.55f, 0.85f, 0.35f).fenv (0.001f, 0.2f, 0.2f, 0.15f).amp (0.001f, 0.25f, 0.9f, 0.1f)
              .voice (Mono, 0, 0.0f).mono (120.0f)
              .lfoSync (1, LSH, D1_16, 0.6f).mod (2, SLfo1, DCutoff, 0.25f).macros()),

        make ("Reese Bass", "BASS", B().gain (2.9f).osc (1, Saw, 0.6f, 0, 0, -12.0f, -0.3f).osc (2, Saw, 0.6f, 0, 0, 12.0f, 0.3f).off (3).sub (Sine, -1, 0.6f)
              .filter (LP24, 600.0f, 0.3f, 0.4f, 0.15f).amp (0.005f, 0.3f, 1.0f, 0.2f)
              .voice (Legato, 0, 0.08f).unison (4, 0.45f, 0.5f).mono (120.0f)
              .lfoSync (1, LSine, D1_1, 1.0f).mod (2, SLfo1, DCutoff, 0.15f).macros()),

        make ("Growl Bass", "BASS", B().gain (-1.5f).osc (1, Saw, 0.6f).osc (2, Square, 0.45f, -1).off (3).sub (Sine, -1, 0.6f)
              .filter (BP, 700.0f, 0.7f, 0.8f, 0.0f, 0.2f).amp (0.002f, 0.3f, 1.0f, 0.1f)
              .voice (Mono, 0, 0.02f).unison (3, 0.2f, 0.4f).mono (150.0f)
              .lfoSync (1, LTri, D1_8, 1.0f).mod (2, SLfo1, DCutoff, 0.6f)
              .lfoSync (2, LSH, D1_16, 1.0f).set (ParamIDs::mod (1, "src"), (float) SLfo2).set (ParamIDs::mod (1, "dst"), (float) DReso)
              .set (ParamIDs::mod (1, "amt"), 0.3f).macros (0.5f)),

        make ("Dark Bass", "BASS", B().gain (4.9f).osc (1, Tri, 0.7f).osc (2, Saw, 0.35f, -1).off (3).sub (Sine, -1, 0.7f)
              .filter (LP24, 380.0f, 0.15f, 0.3f, 0.2f).amp (0.01f, 0.4f, 0.9f, 0.3f)
              .voice (Poly, 3, 0.0f).mono (120.0f).lfoSync (1, LSine, D1_2, 0.0f).mod (2, SLfo1, DCutoff, 0.3f).macros (0.6f, -0.3f)),

        make ("Punch Bass", "BASS", B().gain (6.8f).osc (1, Saw, 0.6f).osc (2, Square, 0.45f, 0, 0, -5.0f).off (3).sub (Sine, -1, 0.7f)
              .filter (LP24, 300.0f, 0.25f, 0.45f, 0.85f).fenv (0.001f, 0.12f, 0.0f, 0.1f).amp (0.001f, 0.25f, 0.6f, 0.08f)
              .voice (Mono, 0, 0.0f).mono (120.0f).lfoSync (1, LSquare, D1_16, 0.0f).mod (2, SLfo1, DCutoff, 0.3f).macros()),

        make ("Cyber Bass", "BASS", B().gain (0.9f).osc (1, Square, 0.6f).osc (2, Saw, 0.35f, 0, 12).off (3).sub (Sine, -1, 0.5f).noise (Digital, 0.1f)
              .filter (LP12, 900.0f, 0.6f, 0.55f, 0.5f).fenv (0.001f, 0.09f, 0.1f, 0.1f).amp (0.001f, 0.3f, 0.9f, 0.1f)
              .voice (Mono, 0, 0.015f).unison (2, 0.2f, 0.5f).mono (120.0f)
              .lfoSync (1, LSquare, D1_16, 1.0f).mod (2, SLfo1, DCutoff, 0.25f).macros()),

        make ("Nu Metal Bass", "BASS", B().gain (8.3f).osc (1, Saw, 0.65f).osc (2, Saw, 0.4f, -1).off (3).sub (Sine, -1, 0.5f)
              .filter (LP24, 1600.0f, 0.1f, 0.6f, 0.25f).fenv (0.001f, 0.3f, 0.4f, 0.2f).amp (0.001f, 0.5f, 0.7f, 0.15f)
              .voice (Mono, 0, 0.0f).mono (100.0f).lfoSync (1, LSine, D1_4, 0.0f).mod (2, SLfo1, DCutoff, 0.3f).macros()),

        make ("Hybrid Bass", "BASS", B().gain (2.4f).osc (1, Saw, 0.55f).osc (2, Square, 0.45f).osc (3, Sine, 0.15f, 1).sub (Sine, -1, 0.6f)
              .filter (LP24, 1100.0f, 0.3f, 0.5f, 0.4f).fenv (0.002f, 0.25f, 0.3f, 0.2f).amp (0.002f, 0.3f, 0.95f, 0.12f)
              .voice (Mono, 0, 0.02f).unison (3, 0.25f, 0.5f).mono (120.0f)
              .lfoSync (1, LSine, D1_2, 1.0f).mod (2, SLfo1, DCutoff, 0.12f).macros()),

        make ("Trap 808", "TRAP", B().gain (8.3f).osc (1, Sine, 0.9f, 0, 0, 0.0f).off (2).off (3).sub (Sine, -1, 0.0f)
              .filter (LP12, 8000.0f, 0.0f, 0.6f, 0.0f, 0.0f).fenv (0.001f, 0.12f, 0.0f, 0.1f).amp (0.001f, 1.5f, 0.0f, 0.3f)
              .voice (Mono, 0, 0.06f).mono (150.0f)
              .mod (1, SFEnv, DPitch, 0.25f).mod (5, SM1, DDrive, 0.4f).mod (6, SM2, DAmp, -0.3f)
              .mod (7, SM3, DCutoff, -0.4f), juce::StringArray { "DISTORTION", "TAIL", "DARKNESS", "SPACE" }),

        // ========================================================= DUBSTEP
        make ("Dubstep Bass", "DUBSTEP", B().gain (2.2f).osc (1, Saw, 0.6f).osc (2, Square, 0.5f, 0, 0, 8.0f).off (3).sub (Sine, -1, 0.8f)
              .filter (LP24, 250.0f, 0.55f, 0.75f, 0.0f, 0.2f).amp (0.002f, 0.3f, 1.0f, 0.1f)
              .voice (Legato, 0, 0.03f).unison (3, 0.2f, 0.5f).mono (150.0f)
              .lfoSync (1, LSine, D1_8, 1.0f).mod (2, SLfo1, DCutoff, 0.8f)
              .mod (5, SM1, DDrive, 0.25f).mod (6, SM2, DCutoff, 0.35f).mod (7, SM3, DReso, 0.3f).mod (8, SM4, DUniDet, 0.5f),
              juce::StringArray { "AGGRESSION", "OPEN", "SCREAM", "SPACE" }),

        make ("Metal Dubstep", "DUBSTEP", B().gain (2.7f).osc (1, Square, 0.6f).osc (2, Saw, 0.3f, 0, 7).off (3).sub (Sine, -1, 0.7f)
              .filter (LP24, 400.0f, 0.6f, 0.9f, 0.0f, 0.2f).amp (0.002f, 0.3f, 1.0f, 0.1f)
              .voice (Mono, 0, 0.02f).unison (2, 0.25f, 0.5f).mono (150.0f)
              .lfoSync (1, LSaw, D1_8T, 1.0f).mod (2, SLfo1, DCutoff, 0.7f)
              .lfoSync (2, LSH, D1_16, 1.0f).mod (1, SLfo2, DOsc2L, -0.4f).macros (0.3f)),

        // ============================================================ KEYS
        make ("Dark Piano", "KEYS", B().gain (-4.5f).osc (1, Tri, 0.7f).osc (2, Saw, 0.2f, 1, 0, 4.0f).off (3).sub (Sine, -1, 0.0f)
              .filter (LP12, 1400.0f, 0.1f, 0.1f, 0.5f, 0.6f).fenv (0.001f, 0.6f, 0.1f, 0.5f).amp (0.001f, 1.8f, 0.0f, 0.5f)
              .mod (1, SVel, DCutoff, 0.3f)),

        make ("Digital Piano", "KEYS", B().gain (-3.1f).osc (1, Sine, 0.7f).osc (2, Pulse, 0.25f, 1).osc (3, Sine, 0.12f, 2).sub (Sine, -1, 0.0f)
              .filter (LP12, 3000.0f, 0.05f, 0.0f, 0.4f, 0.5f).fenv (0.001f, 0.4f, 0.15f, 0.4f).amp (0.001f, 1.4f, 0.0f, 0.4f)
              .mod (1, SVel, DCutoff, 0.25f)),

        make ("Industrial Keys", "KEYS", B().gain (-1.5f).osc (1, Square, 0.6f).osc (2, Saw, 0.3f, 0, 7).off (3).sub (Sine, -1, 0.0f).noise (Digital, 0.06f)
              .filter (LP24, 1200.0f, 0.45f, 0.5f, 0.5f, 0.5f).fenv (0.001f, 0.25f, 0.1f, 0.3f).amp (0.001f, 0.9f, 0.2f, 0.25f)),

        make ("Ambient Keys", "KEYS", B().gain (-2.7f).osc (1, Tri, 0.6f).osc (2, Sine, 0.4f, 1).off (3).sub (Sine, -1, 0.0f)
              .filter (LP12, 2200.0f, 0.1f, 0.0f, 0.2f, 0.5f).amp (0.01f, 2.5f, 0.3f, 1.8f)
              .unison (3, 0.12f, 0.9f).lfoFree (1, LSine, 0.3f, 1.0f).mod (2, SLfo1, DPan, 0.3f)),

        make ("Broken Keys", "KEYS", B().gain (-2.4f).osc (1, Pulse, 0.6f).osc (2, Tri, 0.4f, 0, 0, 25.0f).off (3).sub (Sine, -1, 0.0f)
              .filter (LP12, 1800.0f, 0.15f, 0.4f, 0.3f, 0.5f).amp (0.002f, 1.2f, 0.1f, 0.4f)
              .lfoFree (1, LRandom, 3.0f, 1.0f).mod (2, SLfo1, DPitch, 0.006f)
              .lfoSync (2, LSH, D1_16, 1.0f).mod (3, SLfo2, DAmp, -0.3f)),

        make ("Metallic Keys", "KEYS", B().gain (2.7f).osc (1, Square, 0.55f).osc (2, Square, 0.45f, 1, 6, 14.0f).off (3).sub (Sine, -1, 0.0f)
              .filter (BP, 2500.0f, 0.5f, 0.2f, 0.2f, 1.0f).amp (0.001f, 1.0f, 0.0f, 0.6f)),

        make ("Soft Keys", "KEYS", B().gain (-3.3f).osc (1, Sine, 0.7f).osc (2, Tri, 0.2f, 1).off (3).sub (Sine, -1, 0.0f)
              .filter (LP12, 1600.0f, 0.0f, 0.0f, 0.15f, 0.5f).amp (0.006f, 1.6f, 0.2f, 0.7f).mod (1, SVel, DCutoff, 0.3f)),

        // ============================================================ PADS
        make ("Dark Pad", "PADS", B().gain (-2.6f).osc (1, Saw, 0.55f).osc (2, Saw, 0.55f, 0, 0, -9.0f).off (3).sub (Sine, -1, 0.0f)
              .filter (LP24, 700.0f, 0.2f, 0.1f, 0.1f).amp (1.2f, 1.0f, 0.9f, 2.5f)
              .unison (6, 0.35f, 1.0f).lfoFree (1, LTri, 0.15f, 1.0f).mod (2, SLfo1, DCutoff, 0.2f).macros (0.4f, -0.4f, 0.4f)),

        make ("Atmospheric Pad", "PADS", B().gain (0.2f).osc (1, Tri, 0.6f).osc (2, Saw, 0.3f, 1).off (3).sub (Sine, -1, 0.0f).noise (Pink, 0.08f)
              .filter (LP12, 1800.0f, 0.15f, 0.0f, 0.1f).amp (2.0f, 1.0f, 0.9f, 3.0f)
              .unison (5, 0.25f, 1.0f).lfoSync (1, LSine, D2_1, 1.0f).mod (2, SLfo1, DCutoff, 0.15f)
              .lfoFree (2, LSine, 0.1f, 1.0f).mod (3, SLfo2, DPan, 0.4f)),

        make ("Digital Pad", "PADS", B().gain (-5.4f).osc (1, Square, 0.5f).osc (2, Pulse, 0.5f, 0, 0, 10.0f).off (3).sub (Sine, -1, 0.0f)
              .filter (LP24, 1500.0f, 0.35f, 0.1f, 0.1f).amp (0.4f, 1.0f, 0.85f, 1.5f)
              .unison (4, 0.2f, 0.9f).lfoSync (1, LSH, D1_16, 1.0f).mod (2, SLfo1, DCutoff, 0.2f)),

        // =========================================================== LEADS
        make ("Aggressive Lead", "LEADS", B().gain (7.2f).osc (1, Saw, 0.6f).osc (2, Square, 0.45f, 0, 0, 7.0f).off (3).sub (Sine, -1, 0.0f)
              .filter (LP24, 3000.0f, 0.3f, 0.6f, 0.3f).fenv (0.002f, 0.3f, 0.5f, 0.2f).amp (0.002f, 0.3f, 0.9f, 0.15f)
              .voice (Legato, 0, 0.05f).unison (3, 0.2f, 0.6f).vibrato()),

        make ("Screaming Lead", "LEADS", B().gain (7.5f).osc (1, Saw, 0.6f).osc (2, Saw, 0.45f, 0, 12).off (3).sub (Sine, -1, 0.0f)
              .filter (BP, 2500.0f, 0.7f, 0.9f, 0.2f, 0.5f).amp (0.002f, 0.3f, 0.95f, 0.2f)
              .voice (Legato, 0, 0.06f).unison (2, 0.15f, 0.5f).vibrato()),

        make ("Metallic Lead", "LEADS", B().gain (6.0f).osc (1, Square, 0.55f).osc (2, Square, 0.45f, 0, 7, 20.0f).off (3).sub (Sine, -1, 0.0f)
              .filter (NOTCH, 1600.0f, 0.5f, 0.5f, 0.1f).amp (0.002f, 0.3f, 0.9f, 0.2f)
              .voice (Mono, 0, 0.03f).lfoSync (1, LTri, D1_4, 1.0f).mod (2, SLfo1, DCutoff, 0.3f).vibrato()),

        make ("Industrial Synth", "LEADS", B().gain (0.1f).osc (1, Pulse, 0.55f).osc (2, Saw, 0.4f, 0, -5).off (3).sub (Sine, -1, 0.0f).noise (Digital, 0.12f)
              .filter (LP24, 1500.0f, 0.6f, 0.8f, 0.3f).amp (0.002f, 0.4f, 0.8f, 0.2f)
              .lfoSync (1, LSquare, D1_16, 1.0f).mod (2, SLfo1, DCutoff, 0.35f).macros()),

        // ========================================================== PLUCKS
        make ("Pluck", "PLUCKS", B().gain (-2.8f).osc (1, Saw, 0.6f).osc (2, Square, 0.4f, 0, 0, 6.0f).off (3).sub (Sine, -1, 0.0f)
              .filter (LP24, 300.0f, 0.3f, 0.2f, 0.8f, 0.5f).fenv (0.001f, 0.18f, 0.0f, 0.15f).amp (0.001f, 0.35f, 0.0f, 0.25f)
              .mod (1, SVel, DCutoff, 0.2f)),

        make ("Digital Pluck", "PLUCKS", B().gain (-3.9f).osc (1, Square, 0.6f).osc (2, Pulse, 0.3f, 1).off (3).sub (Sine, -1, 0.0f)
              .filter (LP12, 500.0f, 0.55f, 0.2f, 0.7f, 0.5f).fenv (0.001f, 0.12f, 0.0f, 0.12f).amp (0.001f, 0.25f, 0.0f, 0.2f)
              .mod (1, SVel, DCutoff, 0.3f)),

        // ============================================================ ARPS
        make ("Arp Synth", "ARPS", B().gain (-2.8f).osc (1, Saw, 0.6f).osc (2, Square, 0.3f, 1).off (3).sub (Sine, -1, 0.0f)
              .filter (LP24, 700.0f, 0.4f, 0.3f, 0.6f, 0.4f).fenv (0.001f, 0.15f, 0.05f, 0.12f).amp (0.001f, 0.2f, 0.2f, 0.12f)
              .unison (2, 0.15f, 0.6f).lfoSync (1, LTri, D1_1, 1.0f).mod (2, SLfo1, DCutoff, 0.2f)
              .pattern ("Init 16ths").arp (AUp, 2)),

        // ============================================================== FX
        make ("Noise Riser", "FX", B().gain (11.1f).off (1).off (2).off (3).sub (Sine, -1, 0.0f).noise (White, 0.8f)
              .filter (BP, 400.0f, 0.5f, 0.0f, 0.0f, 0.0f).amp (0.5f, 1.0f, 1.0f, 1.5f)
              .lfoSync (1, LSaw, D2_1, 1.0f).mod (2, SLfo1, DCutoff, -0.8f)),

        make ("Glitch Sweep", "FX", B().gain (-10.3f).osc (1, Square, 0.5f).off (2).off (3).sub (Sine, -1, 0.0f).noise (Digital, 0.3f)
              .filter (LP12, 1000.0f, 0.8f, 0.3f, 0.0f).amp (0.01f, 0.5f, 1.0f, 0.8f)
              .lfoSync (1, LSH, D1_32, 1.0f).mod (2, SLfo1, DCutoff, 0.6f)
              .lfoFree (2, LRandom, 7.0f, 1.0f).mod (3, SLfo2, DPitch, 0.3f)),
        // ======================================================= SEQUENCES (v0.3)
        make ("Metalcore Verse Seq", "SEQUENCES", B().gain (5.0f).osc (1, Saw, 0.6f).osc (2, Square, 0.45f, 0, 0, -5.0f).off (3).sub (Sine, -1, 0.6f)
              .filter (LP24, 350.0f, 0.3f, 0.5f, 0.8f).fenv (0.001f, 0.12f, 0.1f, 0.08f).amp (0.001f, 0.2f, 0.7f, 0.06f)
              .voice (Mono, 0, 0.0f).mono (120.0f).pattern ("Verse Chug").seq (TKey, -1).macros()),

        make ("Breakdown Sub Stabs", "SEQUENCES", B().gain (8.7f).osc (1, Saw, 0.5f).osc (2, Square, 0.4f, -1).off (3).sub (Sine, -1, 0.9f)
              .filter (LP24, 500.0f, 0.3f, 0.8f, 0.6f).fenv (0.001f, 0.1f, 0.0f, 0.08f).amp (0.001f, 0.25f, 0.3f, 0.08f)
              .voice (Mono, 0, 0.0f).mono (150.0f).pattern ("Breakdown Stabs").set (ParamIDs::timeFeel, 0.0f).seq (TKey, -1).macros()),

        make ("Industrial Chug Gate", "SEQUENCES", B().gain (7.0f).osc (1, Square, 0.6f).osc (2, Pulse, 0.35f, 0, 7).off (3).sub (Square, -1, 0.35f).noise (Digital, 0.12f)
              .filter (LP24, 900.0f, 0.55f, 0.85f, 0.4f).fenv (0.001f, 0.1f, 0.2f, 0.08f).amp (0.001f, 0.15f, 0.8f, 0.05f)
              .voice (Mono, 0, 0.0f).mono (120.0f).pattern ("Chug Gate").seq (TKey, -1).macros()),

        make ("Cinematic Build Seq", "SEQUENCES", B().gain (-2.0f).osc (1, Saw, 0.5f).osc (2, Square, 0.3f, 1).off (3).sub (Sine, -1, 0.0f)
              .filter (LP24, 600.0f, 0.4f, 0.2f, 0.6f, 0.4f).fenv (0.001f, 0.15f, 0.1f, 0.2f).amp (0.001f, 0.3f, 0.3f, 0.2f)
              .unison (3, 0.2f, 0.8f).lfoSync (1, LSaw, D2_1, 1.0f).mod (2, SLfo1, DCutoff, -0.3f)
              .pattern ("Build 8ths to 16ths").seq (TKey, 0)),

        make ("Drop Slide Bass", "SEQUENCES", B().gain (3.0f).osc (1, Saw, 0.6f, 0, 0, -12.0f, -0.3f).osc (2, Saw, 0.6f, 0, 0, 12.0f, 0.3f).off (3).sub (Sine, -1, 0.7f)
              .filter (LP24, 700.0f, 0.3f, 0.6f, 0.3f).amp (0.002f, 0.3f, 1.0f, 0.15f)
              .voice (Legato, 0, 0.12f).unison (3, 0.35f, 0.5f).mono (120.0f).pattern ("Drop Slide").seq (TKey, -1).macros()),

        // ============================================================ ARPS (v0.3)
        make ("Cyber Arp", "ARPS", B().gain (0.7f).osc (1, Square, 0.6f).osc (2, Pulse, 0.3f, 1).off (3).sub (Sine, -1, 0.0f)
              .filter (LP12, 700.0f, 0.55f, 0.2f, 0.6f, 0.5f).fenv (0.001f, 0.1f, 0.0f, 0.1f).amp (0.001f, 0.2f, 0.0f, 0.12f)
              .pattern ("Init 16ths").arp (AUpDown, 2)),

        make ("Dark Chord Gate", "ARPS", B().gain (-4.7f).osc (1, Saw, 0.55f).osc (2, Saw, 0.55f, 0, 0, -9.0f).off (3).sub (Sine, -1, 0.0f)
              .filter (LP24, 900.0f, 0.2f, 0.1f, 0.1f).amp (0.003f, 0.2f, 0.9f, 0.05f)
              .unison (4, 0.3f, 1.0f).pattern ("Chug Gate").arp (AChord, 1)),

        make ("Metalcore Arp", "ARPS", B().gain (3.8f).osc (1, Saw, 0.6f).osc (2, Square, 0.4f, 0, 0, 6.0f).off (3).sub (Sine, -1, 0.0f)
              .filter (LP24, 500.0f, 0.35f, 0.2f, 0.7f, 0.5f).fenv (0.001f, 0.14f, 0.0f, 0.12f).amp (0.001f, 0.25f, 0.0f, 0.15f)
              .pattern ("Init 16ths").arp (AOrder, 2)),

        // =========================================================== DRUMS (v0.7) - kits: o teclado toca a bateria (GM)
        make ("Metal Kit - Basic Rock",   "DRUMS", B().drums ("Basic Rock", true)),
        make ("Metal Kit - Double Kick",  "DRUMS", B().drums ("Double Kick Groove", true)),
        make ("Metal Kit - Gallop",       "DRUMS", B().drums ("Metal Gallop", true)),
        make ("Metal Kit - Blast Beat",   "DRUMS", B().drums ("Blast Beat", true)),
        make ("Breakdown Kit - Chug",     "DRUMS", B().drums ("Breakdown Chug", true)),
        make ("Breakdown Kit - Destroy",  "DRUMS", B().drums ("Half-Time Destroy", true)),
        make ("Trap Kit - Dark",          "DRUMS", B().drums ("Dark Trap", true)),
        make ("Trap Kit - Metal Trap",    "DRUMS", B().drums ("Metal Trap", true)),
        make ("Dubstep Kit - Heavy",      "DRUMS", B().drums ("Heavy Dubstep", true)),
        make ("Industrial Kit - Stomp",   "DRUMS", B().drums ("Industrial Stomp", true)),

        // =========================================================== SCRATCH MACHINE (70 presets, tools/gen_scratch.py)
        #include "ScratchPresets.inc"

        // =========================================================== BIBLIOTECA GERADA (tools/gen_library.py)
        #include "FactoryLibrary.inc"
    };
    return list;
}
} // namespace mm
