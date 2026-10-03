#include "Parameters.h"
#include "DSP/WavetableBank.h"
#include "Sequencer/Scale.h"
#include "Synth/ModulationDefs.h"

using namespace juce;

namespace mm
{
namespace
{
    using Attr = AudioParameterFloatAttributes;

    String formatTime (float seconds, int)
    {
        return seconds < 1.0f ? String (seconds * 1000.0f, 0) + " ms" : String (seconds, 2) + " s";
    }

    std::unique_ptr<AudioParameterFloat> timeParam (const String& id, const String& name, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
            NormalisableRange<float> (0.001f, 10.0f, 0.0f, 0.3f), def,
            Attr().withStringFromValueFunction (formatTime));
    }

    std::unique_ptr<AudioParameterFloat> percentParam (const String& id, const String& name,
                                                       float min, float max, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
            NormalisableRange<float> (min, max), def,
            Attr().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; }));
    }
}

AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    // ---------------------------------------------------------------- OSCs
    // INIT: OSC1 Saw + OSC2 Saw desafinado; OSC3 desligado; SUB seno -1 oitava
    const int   defWave[]  { 2, 2, 3 };
    const float defFine[]  { 0.0f, 9.0f, 0.0f };
    const float defLevel[] { 0.75f, 0.6f, 0.0f };
    const float defPan[]   { -0.25f, 0.25f, 0.0f };

    for (int i = 1; i <= numOscillators; ++i)
    {
        const String p = "OSC" + String (i) + " ";
        auto g = std::make_unique<AudioProcessorParameterGroup> ("osc" + String (i), "OSC " + String (i), "|");

        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::osc (i, "wave"), 1 }, p + "Wave", waveNames, defWave[i - 1]));
        g->addChild (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::osc (i, "octave"), 1 }, p + "Octave", -3, 3, 0));
        g->addChild (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::osc (i, "semi"), 1 }, p + "Semitone", -12, 12, 0));
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::osc (i, "fine"), 1 }, p + "Fine",
            NormalisableRange<float> (-100.0f, 100.0f, 0.1f), defFine[i - 1],
            Attr().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " ct"; })));
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::osc (i, "phase"), 1 }, p + "Phase",
            NormalisableRange<float> (0.0f, 1.0f), 0.0f,
            Attr().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 360.0f)) + "deg"; })));
        g->addChild (percentParam (ParamIDs::osc (i, "level"), p + "Level", 0.0f, 1.0f, defLevel[i - 1]));
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::osc (i, "pan"), 1 }, p + "Pan",
            NormalisableRange<float> (-1.0f, 1.0f), defPan[i - 1],
            Attr().withStringFromValueFunction ([] (float v, int)
            {
                if (std::abs (v) < 0.01f) return String ("C");
                return String (roundToInt (std::abs (v) * 100.0f)) + (v < 0 ? " L" : " R");
            })));
        // ---- v0.4: wavetable e FM (usados quando WAVE = Wavetable / FM) ----
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::osc (i, "wt"), 1 }, p + "Wavetable",
                                                             WavetableBank::names(), 0));
        g->addChild (percentParam (ParamIDs::osc (i, "wtpos"), p + "WT Position", 0.0f, 1.0f, 0.0f));
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::osc (i, "fmratio"), 1 }, p + "FM Ratio",
                                                             fmRatioNames, 3));
        g->addChild (percentParam (ParamIDs::osc (i, "fmamt"), p + "FM Amount", 0.0f, 1.0f, 0.3f));
        layout.add (std::move (g));
    }

    // ----------------------------------------------------------- SUB / NOISE
    {
        auto g = std::make_unique<AudioProcessorParameterGroup> ("sub", "SUB", "|");
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::subWave, 1 }, "Sub Wave", subWaveNames, 0));
        g->addChild (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::subOctave, 1 }, "Sub Octave", -2, 0, -1));
        g->addChild (percentParam (ParamIDs::subLevel, "Sub Level", 0.0f, 1.0f, 0.5f));
        layout.add (std::move (g));
    }
    {
        auto g = std::make_unique<AudioProcessorParameterGroup> ("noise", "NOISE", "|");
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::noiseType, 1 }, "Noise Type", noiseNames, 0));
        g->addChild (percentParam (ParamIDs::noiseLevel, "Noise Level", 0.0f, 1.0f, 0.0f));
        layout.add (std::move (g));
    }

    // -------------------------------------------------------------- FILTER
    {
        auto g = std::make_unique<AudioProcessorParameterGroup> ("filter", "FILTER", "|");
        NormalisableRange<float> cutoffRange (20.0f, 20000.0f);
        cutoffRange.setSkewForCentre (1000.0f);

        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::filterType, 1 }, "Filter Type", filterNames, 1));
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::cutoff, 1 }, "Filter Cutoff", cutoffRange, 2200.0f,
            Attr().withStringFromValueFunction ([] (float v, int)
            {
                return v < 1000.0f ? String (roundToInt (v)) + " Hz" : String (v / 1000.0f, 2) + " kHz";
            })));
        g->addChild (percentParam (ParamIDs::resonance,    "Filter Resonance",  0.0f, 1.0f, 0.25f));
        g->addChild (percentParam (ParamIDs::filterDrive,  "Filter Drive",      0.0f, 1.0f, 0.2f));
        g->addChild (percentParam (ParamIDs::keyTrack,     "Filter Keytrack",   0.0f, 1.0f, 0.3f));
        g->addChild (percentParam (ParamIDs::filterMix,    "Filter Mix",        0.0f, 1.0f, 1.0f));
        g->addChild (percentParam (ParamIDs::filterEnvAmt, "Filter Env Amount", -1.0f, 1.0f, 0.35f));

        g->addChild (timeParam (ParamIDs::fenvAttack, "Filter Env Attack", 0.003f));
        g->addChild (timeParam (ParamIDs::fenvDecay,  "Filter Env Decay",  0.25f));
        g->addChild (percentParam (ParamIDs::fenvSustain, "Filter Env Sustain", 0.0f, 1.0f, 0.25f));
        g->addChild (timeParam (ParamIDs::fenvRelease, "Filter Env Release", 0.3f));
        layout.add (std::move (g));
    }

    // ----------------------------------------------------------------- AMP
    {
        auto g = std::make_unique<AudioProcessorParameterGroup> ("amp", "AMP", "|");
        g->addChild (timeParam (ParamIDs::ampAttack, "Amp Attack", 0.002f));
        g->addChild (timeParam (ParamIDs::ampDecay,  "Amp Decay",  0.2f));
        g->addChild (percentParam (ParamIDs::ampSustain, "Amp Sustain", 0.0f, 1.0f, 0.85f));
        g->addChild (timeParam (ParamIDs::ampRelease, "Amp Release", 0.2f));
        layout.add (std::move (g));
    }

    // ---- ENV 3: envelope de modulacao (fonte "Mod Env" da matriz) ----
    {
        auto g = std::make_unique<AudioProcessorParameterGroup> ("menv", "MOD ENV", "|");
        g->addChild (timeParam (ParamIDs::menvAttack, "Mod Env Attack", 0.01f));
        g->addChild (timeParam (ParamIDs::menvDecay,  "Mod Env Decay",  0.4f));
        g->addChild (percentParam (ParamIDs::menvSustain, "Mod Env Sustain", 0.0f, 1.0f, 0.0f));
        g->addChild (timeParam (ParamIDs::menvRelease, "Mod Env Release", 0.3f));
        layout.add (std::move (g));
    }

    // -------------------------------------------------------------- MASTER
    {
        auto g = std::make_unique<AudioProcessorParameterGroup> ("master", "MASTER", "|");
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::masterGain, 1 }, "Master Gain",
            NormalisableRange<float> (-60.0f, 18.0f, 0.1f), -6.0f,
            Attr().withStringFromValueFunction ([] (float v, int) { return v <= -59.9f ? String ("-inf") : String (v, 1) + " dB"; })));
        g->addChild (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::bendRange, 1 }, "Pitch Bend Range", 1, 24, 2));
        layout.add (std::move (g));
    }

    // --------------------------------------------------------------- VOICE
    {
        auto g = std::make_unique<AudioProcessorParameterGroup> ("voice", "VOICE", "|");
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::voiceMode, 1 }, "Voice Mode", voiceModeNames, 0));
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::voiceCount, 1 }, "Voices", voiceCountNames, 4));
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::glide, 1 }, "Glide",
            NormalisableRange<float> (0.0f, 2.0f, 0.0f, 0.35f), 0.0f,
            Attr().withStringFromValueFunction ([] (float v, int)
            {
                return v < 0.0005f ? String ("OFF") : formatTime (v, 0);
            })));
        g->addChild (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::unisonVoices, 1 }, "Unison", 1, 8, 1));
        g->addChild (percentParam (ParamIDs::unisonDetune, "Unison Detune", 0.0f, 1.0f, 0.25f));
        g->addChild (percentParam (ParamIDs::unisonSpread, "Unison Spread", 0.0f, 1.0f, 0.6f));
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::bassMono, 1 }, "Bass Mono",
            NormalisableRange<float> (0.0f, 300.0f, 1.0f), 0.0f,
            Attr().withStringFromValueFunction ([] (float v, int)
            {
                return v < 20.0f ? String ("OFF") : String (roundToInt (v)) + " Hz";
            })));
        layout.add (std::move (g));
    }

    // ---------------------------------------------------------------- LFOs
    for (int i = 1; i <= numLfos; ++i)
    {
        const String p = "LFO" + String (i) + " ";
        auto g = std::make_unique<AudioProcessorParameterGroup> ("lfo" + String (i), "LFO " + String (i), "|");
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::lfo (i, "shape"), 1 }, p + "Shape", lfoShapeNames, 0));
        g->addChild (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::lfo (i, "sync"), 1 }, p + "Sync", true));
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::lfo (i, "rate"), 1 }, p + "Rate",
            NormalisableRange<float> (0.02f, 40.0f, 0.0f, 0.3f), 2.0f,
            Attr().withStringFromValueFunction ([] (float v, int) { return String (v, v < 10.0f ? 2 : 1) + " Hz"; })));
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::lfo (i, "div"), 1 }, p + "Division", lfoDivNames, 3));
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::lfo (i, "phase"), 1 }, p + "Phase",
            NormalisableRange<float> (0.0f, 1.0f), 0.0f,
            Attr().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 360.0f)) + "deg"; })));
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::lfo (i, "fade"), 1 }, p + "Fade In",
            NormalisableRange<float> (0.0f, 5.0f, 0.0f, 0.4f), 0.0f,
            Attr().withStringFromValueFunction ([] (float v, int) { return v < 0.0005f ? String ("OFF") : formatTime (v, 0); })));
        g->addChild (percentParam (ParamIDs::lfo (i, "amount"), p + "Amount", 0.0f, 1.0f, 1.0f));
        g->addChild (percentParam (ParamIDs::lfo (i, "smooth"), p + "Smooth", 0.0f, 1.0f, 0.0f));
        layout.add (std::move (g));
    }

    // ------------------------------------------------------ MOD MATRIX (8)
    for (int i = 1; i <= numModSlots; ++i)
    {
        const String p = "Mod " + String (i) + " ";
        auto g = std::make_unique<AudioProcessorParameterGroup> ("mod" + String (i), "MOD SLOT " + String (i), "|");
        // INIT: slot 1 = Mod Wheel -> Cutoff (mesmo comportamento da Fase 1)
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::mod (i, "src"), 1 }, p + "Source",
                                                             modSourceNames(), i == 1 ? (int) ModSource::ModWheel : 0));
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::mod (i, "dst"), 1 }, p + "Destination",
                                                             modDestNames(), i == 1 ? (int) ModDest::Cutoff : 0));
        g->addChild (percentParam (ParamIDs::mod (i, "amt"), p + "Amount", -1.0f, 1.0f, i == 1 ? 0.35f : 0.5f));
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::mod (i, "curve"), 1 }, p + "Curve", modCurveNames(), 0));
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::mod (i, "mode"), 1 }, p + "Mode", modModeNames(), 0));
        layout.add (std::move (g));
    }

    // ----------------------------------------------------- SEQUENCER / ARP
    {
        auto g = std::make_unique<AudioProcessorParameterGroup> ("sequencer", "SEQUENCER", "|");
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::seqMode, 1 }, "Seq Mode", seqModeNames, 0));
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::seqTrigger, 1 }, "Seq Trigger", seqTriggerNames, 0));
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::seqRate, 1 }, "Seq Rate", seqRateNames, 2));
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::timeFeel, 1 }, "Time Feel", timeFeelNames, 1));
        g->addChild (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::seqLength, 1 }, "Seq Length", 1, 64, 16));
        g->addChild (percentParam (ParamIDs::seqSwing, "Seq Swing", 0.0f, 1.0f, 0.0f));
        g->addChild (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::seqTranspose, 1 }, "Seq Transpose", -24, 24, 0,
            AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return (v > 0 ? "+" : "") + String (v) + " st"; })));
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::seqGate, 1 }, "Seq Gate",
            NormalisableRange<float> (0.1f, 1.5f), 1.0f,
            Attr().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })));
        g->addChild (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::seqOctave, 1 }, "Seq Octave", -3, 3, 0));
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::arpMode, 1 }, "Arp Mode", arpModeNames, 0));
        g->addChild (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::arpOctaves, 1 }, "Arp Octaves", 1, 4, 1));
        g->addChild (percentParam (ParamIDs::humanizeVel,  "Humanize Velocity", 0.0f, 0.5f, 0.0f));
        g->addChild (percentParam (ParamIDs::humanizeTime, "Humanize Timing",   0.0f, 0.5f, 0.0f));
        g->addChild (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::midiOut, 1 }, "MIDI Out", false));
        layout.add (std::move (g));
    }

    // ------------------------------------------------------ KEY / TEMPO
    {
        auto g = std::make_unique<AudioProcessorParameterGroup> ("key", "KEY / TEMPO", "|");
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::keyRoot, 1 }, "Key Root", noteNames(), 4));   // E
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::keyScale, 1 }, "Key Scale", scaleNames(), 0)); // Minor
        g->addChild (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::scaleLock, 1 }, "Scale Lock", false));
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::tempoSource, 1 }, "Tempo Source", tempoSourceNames, 0));
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::internalBpm, 1 }, "Internal BPM",
            NormalisableRange<float> (40.0f, 240.0f, 0.1f), 140.0f,
            Attr().withStringFromValueFunction ([] (float v, int) { return String (v, 1); })));
        layout.add (std::move (g));
    }

    // -------------------------------------------------------------- MACROS
    {
        auto g = std::make_unique<AudioProcessorParameterGroup> ("macros", "MACROS", "|");
        for (int i = 1; i <= numMacros; ++i)
            g->addChild (percentParam (ParamIDs::macro (i), "Macro " + String (i), 0.0f, 1.0f, 0.0f));
        layout.add (std::move (g));
    }

    // ---------------------------------------------------------------- FX (v0.6)
    {
        auto g = std::make_unique<AudioProcessorParameterGroup> ("fx", "FX", "|");
        auto onOff = [&g] (const char* id, const String& name, bool def)
        { g->addChild (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
        auto dbParam = [&g] (const char* id, const String& name, float min, float max, float def)
        {
            g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (min, max, 0.1f), def,
                Attr().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; })));
        };
        auto msParam = [&g] (const char* id, const String& name, float min, float max, float def)
        {
            g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (min, max, 0.0f, 0.4f), def,
                Attr().withStringFromValueFunction ([] (float v, int) { return String (v, v < 10.0f ? 1 : 0) + " ms"; })));
        };

        onOff (ParamIDs::fxDistOn, "FX Dist On", false);
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::fxDistType, 1 }, "FX Dist Type",
                                                             StringArray { "Soft", "Hard", "Fuzz", "Rectify" }, 0));
        g->addChild (percentParam (ParamIDs::fxDistDrive, "FX Dist Drive", 0.0f, 1.0f, 0.4f));
        g->addChild (percentParam (ParamIDs::fxDistTone,  "FX Dist Tone",  0.0f, 1.0f, 0.7f));
        g->addChild (percentParam (ParamIDs::fxDistMix,   "FX Dist Mix",   0.0f, 1.0f, 1.0f));

        onOff (ParamIDs::fxEqOn, "FX EQ On", false);
        dbParam (ParamIDs::fxEqLow,  "FX EQ Low",  -12.0f, 12.0f, 0.0f);
        dbParam (ParamIDs::fxEqMid,  "FX EQ Mid",  -12.0f, 12.0f, 0.0f);
        dbParam (ParamIDs::fxEqHigh, "FX EQ High", -12.0f, 12.0f, 0.0f);

        onOff (ParamIDs::fxChOn, "FX Chorus On", false);
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::fxChRate, 1 }, "FX Chorus Rate",
            NormalisableRange<float> (0.05f, 5.0f, 0.0f, 0.5f), 0.6f,
            Attr().withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " Hz"; })));
        g->addChild (percentParam (ParamIDs::fxChDepth, "FX Chorus Depth", 0.0f, 1.0f, 0.35f));
        g->addChild (percentParam (ParamIDs::fxChMix,   "FX Chorus Mix",   0.0f, 1.0f, 0.4f));

        onOff (ParamIDs::fxDlOn, "FX Delay On", false);
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::fxDlDiv, 1 }, "FX Delay Time",
                                                             StringArray { "1/4", "1/8", "1/8D", "1/16", "1/4T", "1/8T", "1/2" }, 2));
        g->addChild (percentParam (ParamIDs::fxDlFb,  "FX Delay Feedback", 0.0f, 0.95f, 0.35f));
        g->addChild (percentParam (ParamIDs::fxDlMix, "FX Delay Mix",      0.0f, 1.0f, 0.25f));
        onOff (ParamIDs::fxDlPing, "FX Delay PingPong", true);

        onOff (ParamIDs::fxRvOn, "FX Reverb On", false);
        g->addChild (percentParam (ParamIDs::fxRvSize,  "FX Reverb Size",  0.0f, 1.0f, 0.7f));
        g->addChild (percentParam (ParamIDs::fxRvDamp,  "FX Reverb Damp",  0.0f, 1.0f, 0.5f));
        g->addChild (percentParam (ParamIDs::fxRvWidth, "FX Reverb Width", 0.0f, 1.0f, 1.0f));
        g->addChild (percentParam (ParamIDs::fxRvMix,   "FX Reverb Mix",   0.0f, 1.0f, 0.25f));

        onOff (ParamIDs::fxCpOn, "FX Comp On", false);
        dbParam (ParamIDs::fxCpThresh, "FX Comp Threshold", -40.0f, 0.0f, -18.0f);
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::fxCpRatio, 1 }, "FX Comp Ratio",
            NormalisableRange<float> (1.0f, 20.0f, 0.0f, 0.5f), 4.0f,
            Attr().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + ":1"; })));
        msParam (ParamIDs::fxCpAttack,  "FX Comp Attack",  0.1f, 100.0f, 5.0f);
        msParam (ParamIDs::fxCpRelease, "FX Comp Release", 10.0f, 1000.0f, 120.0f);
        dbParam (ParamIDs::fxCpMakeup,  "FX Comp Makeup",  0.0f, 24.0f, 4.0f);

        onOff (ParamIDs::fxLimOn, "FX Limiter On", true);

        // ---- v0.9: saturacao, bitcrush, phaser, flanger, glitch ----
        onOff ("fx_sat_on", "FX Saturation On", false);
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { "fx_sat_type", 1 }, "FX Saturation Type",
                                                             StringArray { "Warm", "Tape", "Tube", "Digital" }, 0));
        g->addChild (percentParam ("fx_sat_drive", "FX Saturation Drive", 0.0f, 1.0f, 0.35f));
        g->addChild (percentParam ("fx_sat_mix",   "FX Saturation Mix",   0.0f, 1.0f, 1.0f));

        onOff ("fx_cr_on", "FX Bitcrush On", false);
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { "fx_cr_bits", 1 }, "FX Bitcrush Bits",
            NormalisableRange<float> (2.0f, 16.0f, 0.1f), 8.0f,
            Attr().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " bit"; })));
        g->addChild (std::make_unique<AudioParameterInt> (ParameterID { "fx_cr_down", 1 }, "FX Bitcrush Downsample", 1, 64, 4));
        g->addChild (percentParam ("fx_cr_mix", "FX Bitcrush Mix", 0.0f, 1.0f, 1.0f));

        onOff ("fx_ph_on", "FX Phaser On", false);
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { "fx_ph_rate", 1 }, "FX Phaser Rate",
            NormalisableRange<float> (0.02f, 8.0f, 0.0f, 0.4f), 0.4f,
            Attr().withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " Hz"; })));
        g->addChild (percentParam ("fx_ph_depth", "FX Phaser Depth",    0.0f, 1.0f, 0.6f));
        g->addChild (percentParam ("fx_ph_fb",    "FX Phaser Feedback", -0.9f, 0.9f, 0.4f));
        g->addChild (percentParam ("fx_ph_mix",   "FX Phaser Mix",      0.0f, 1.0f, 0.5f));

        onOff ("fx_fl_on", "FX Flanger On", false);
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { "fx_fl_rate", 1 }, "FX Flanger Rate",
            NormalisableRange<float> (0.02f, 5.0f, 0.0f, 0.4f), 0.25f,
            Attr().withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " Hz"; })));
        g->addChild (percentParam ("fx_fl_depth", "FX Flanger Depth",    0.0f, 1.0f, 0.7f));
        g->addChild (percentParam ("fx_fl_fb",    "FX Flanger Feedback", -0.95f, 0.95f, 0.6f));
        g->addChild (percentParam ("fx_fl_mix",   "FX Flanger Mix",      0.0f, 1.0f, 1.0f));

        onOff ("fx_gl_on", "FX Glitch On", false);
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { "fx_gl_mode", 1 }, "FX Glitch Mode",
                                                             StringArray { "Stutter", "Gate", "Reverse", "Tape Stop" }, 0));
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { "fx_gl_div", 1 }, "FX Glitch Division",
                                                             StringArray { "1/2", "1/4", "1/8", "1/16", "1/32", "1/64" }, 3));
        g->addChild (percentParam ("fx_gl_mix", "FX Glitch Mix", 0.0f, 1.0f, 1.0f));
        layout.add (std::move (g));
    }

    // ---------------------------------------------------------------- DRUMS (v0.7)
    {
        auto g = std::make_unique<AudioProcessorParameterGroup> ("drums", "DRUMS", "|");
        g->addChild (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::drumOn, 1 }, "Drums On", false));
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::drumMode, 1 }, "Drums Play",
                                                             StringArray { "Transport", "Always" }, 0));
        g->addChild (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::drumLength, 1 }, "Drums Length", 1, 64, 16));
        g->addChild (percentParam (ParamIDs::drumSwing, "Drums Swing", 0.0f, 1.0f, 0.0f));
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::drumVolume, 1 }, "Drums Volume",
            NormalisableRange<float> (-60.0f, 6.0f, 0.1f), -3.0f,
            Attr().withStringFromValueFunction ([] (float v, int) { return v <= -59.9f ? String ("-inf") : String (v, 1) + " dB"; })));
        g->addChild (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::drumKeys, 1 }, "Drums Keys", false));
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { "drum808_type", 1 }, "808 Type",
            StringArray { "Clean", "Sub", "Distorted", "Digital", "Metal", "Reese", "Short", "Long" }, 0));
        g->addChild (percentParam ("drum808_drive", "808 Drive", 0.0f, 1.0f, 0.2f));
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { "drum808_glide", 1 }, "808 Glide",
            NormalisableRange<float> (0.01f, 0.5f, 0.0f, 0.5f), 0.08f,
            Attr().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 1000.0f)) + " ms"; })));

        static const char* rowNames[] { "Kick", "Snare", "Clap", "Hat", "Open Hat", "Perc", "Tom", "Crash", "Ride", "FX", "808" };
        for (int r = 0; r < 11; ++r)
        {
            const String p = String ("Drum ") + rowNames[r] + " ";
            g->addChild (percentParam (ParamIDs::drum (r, "level"), p + "Level", 0.0f, 1.0f, 0.8f));
            g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::drum (r, "pan"), 1 }, p + "Pan",
                NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
                Attr().withStringFromValueFunction ([] (float v, int) { return std::abs (v) < 0.01f ? String ("C") : String (roundToInt (std::abs (v) * 100.0f)) + (v < 0 ? " L" : " R"); })));
            g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::drum (r, "pitch"), 1 }, p + "Pitch",
                NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
                Attr().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " st"; })));
            g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::drum (r, "decay"), 1 }, p + "Decay",
                NormalisableRange<float> (0.25f, 4.0f, 0.0f, 0.5f), 1.0f,
                Attr().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })));
        }
        layout.add (std::move (g));
    }

    // ---------------------------------------------------------------- BEAT ENGINE (v0.10)
    {
        auto g = std::make_unique<AudioProcessorParameterGroup> ("beat", "BEAT ENGINE", "|");
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { "beat_style", 1 }, "Beat Style",
            StringArray { "Modern Metal", "Metalcore", "Electronic Metal", "Industrial", "Nu Metal", "Trap Metal",
                          "Electronicore", "Dubstep Metal", "Half-Time", "Dark Electronic", "Cinematic Metal", "Hybrid",
                          "Modern Trap", "Dark Trap", "Modern Electronicore", "Modern Trap Rock", "Industrial Metalcore", "Trapcore" }, 1));
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { "beat_section", 1 }, "Beat Section",
            StringArray { "Intro", "Verse", "Pre-Chorus", "Chorus", "Breakdown", "Bridge", "Outro", "Pre-Drop", "Drop" }, 1));
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { "beat_variation", 1 }, "Beat Variation",
            StringArray { "A", "B", "C", "D" }, 0));
        for (auto [id, name, def] : { std::tuple<const char*, const char*, float> { "beat_energy", "Beat Energy", 0.6f },
                                      { "beat_density", "Beat Density", 0.5f }, { "beat_aggression", "Beat Aggression", 0.5f },
                                      { "beat_electronic", "Beat Electronic", 0.3f }, { "beat_trap", "Beat Trap", 0.0f },
                                      { "beat_glitch", "Beat Glitch", 0.0f },
                                      { "beat_modernity", "Beat Modernity", 0.3f }, { "beat_syncopation", "Beat Syncopation", 0.3f },
                                      { "beat_space", "Beat Space", 0.2f }, { "beat_complexity", "Beat Complexity", 0.4f } })
            g->addChild (percentParam (id, name, 0.0f, 1.0f, def));
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { "beat_halftime", 1 }, "Beat Half-Time",
            StringArray { "Auto", "Full Time", "Half Time" }, 0));
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { "beat_doublekick", 1 }, "Beat Double Kick",
            StringArray { "Off", "Light", "Medium", "Heavy", "Extreme" }, 0));
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { "beat_bars", 1 }, "Beat Bars",
            StringArray { "1 Bar", "2 Bars" }, 1));
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { "beat_randmode", 1 }, "Beat Randomize",
            StringArray { "Subtle", "Medium", "Extreme" }, 1));
        layout.add (std::move (g));
    }

    // ---------------------------------------------------------------- SONG GENERATOR (v0.12)
    {
        auto g = std::make_unique<AudioProcessorParameterGroup> ("song", "SONG", "|");
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { "song_style", 1 }, "Song Style",
            StringArray { "Modern Metalcore", "Electronic Metalcore", "Dark Metalcore", "Trap Metalcore", "Industrial Metalcore",
                          "Cinematic Metalcore", "Digital Metalcore", "Emotional Metalcore", "Heavy Electronic", "Modern Post-Hardcore",
                          "Nu Metal Modern", "Electronicore", "Darkcore", "Cyber Metal", "Modern Trap Rock", "Industrial Modern Metal" }, 0));
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { "song_mood", 1 }, "Song Mood",
            StringArray { "Dark", "Emotional", "Epic", "Aggressive", "Melancholic", "Tense", "Cinematic" }, 0));
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { "song_structure", 1 }, "Song Structure",
            StringArray { "Short", "Standard", "Extended" }, 1));
        for (auto [id, name, def] : { std::tuple<const char*, const char*, float> { "song_heaviness", "Song Heaviness", 0.85f },
                                      { "song_electronic", "Song Electronic", 0.7f }, { "song_trap", "Song Trap", 0.4f },
                                      { "song_darkness", "Song Darkness", 0.8f }, { "song_melodic", "Song Melodic", 0.7f },
                                      { "song_modernity", "Song Modernity", 0.9f }, { "song_texture", "Song Texture", 0.6f },
                                      { "song_fx", "Song FX Density", 0.6f } })
            g->addChild (percentParam (id, name, 0.0f, 1.0f, def));
        g->addChild (std::make_unique<AudioParameterBool> (ParameterID { "song_vocalspace", 1 }, "Song Vocal Space", true));
        g->addChild (std::make_unique<AudioParameterBool> (ParameterID { "song_quick", 1 }, "Song Quick Idea", false));
        layout.add (std::move (g));
    }

    // ---------------------------------------------------------------- SCRATCH MACHINE (v0.15/v0.16)
    {
        auto g = std::make_unique<AudioProcessorParameterGroup> ("scratch", "SCRATCH", "|");
        auto pc = [&g] (const char* id, const char* name, float lo, float hi, float def) { g->addChild (percentParam (id, name, lo, hi, def)); };
        auto ms = [&g] (const char* id, const char* name, float lo, float hi, float def)
        {
            g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.0f, 0.5f), def,
                Attr().withStringFromValueFunction ([] (float v, int) { return String (v, v < 10.0f ? 1 : 0) + " ms"; })));
        };
        g->addChild (std::make_unique<AudioParameterBool> (ParameterID { "scr_on", 1 }, "Scratch On", false));
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { "scr_mode", 1 }, "Scratch Trigger",
            StringArray { "Trigger (Keys)", "Sync (Transport)", "Free" }, 0));
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { "scr_move", 1 }, "Scratch Pattern",
            StringArray { "Baby", "Forward", "Chirp", "Transformer", "Tear", "Flare", "Stab", "Scribble", "Backward", "Orbit", "Crab",
                          "Slicer", "Stutter", "Reverse", "Brake", "Vinyl Stop", "Rhythmic", "Glitch", "Random" }, 0));
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { "scr_rate", 1 }, "Scratch Rate",
            StringArray { "1/4", "1/8", "1/16", "1/2", "1/1", "1/32", "1/4T", "1/8T", "1/16T", "1/32T" }, 0));
        pc ("scr_depth", "Scratch Depth", 0.0f, 1.0f, 0.6f);
        pc ("scr_start", "Scratch Start", 0.0f, 0.95f, 0.1f);
        pc ("scr_fader", "Scratch Fader", 0.0f, 1.0f, 0.8f);
        pc ("scr_level", "Scratch Level", 0.0f, 1.0f, 0.6f);
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { "scr_source", 1 }, "Scratch Source",
            StringArray { "Vox AHH", "Vox OHH", "Vox EHH", "Synth Stab", "User Sample", "Noise Sweep", "808 Tone", "Digital Chirp",
                          "Metal Hit", "Robot Vox", "Vinyl Texture" }, 0));
        // ---- v0.16
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { "scr_engine", 1 }, "Scratch Engine", StringArray { "Synth", "Sample" }, 0));
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { "scr_speed", 1 }, "Scratch Speed", NormalisableRange<float> (0.25f, 4.0f, 0.0f, 0.5f), 1.0f,
            Attr().withStringFromValueFunction ([] (float v, int) { return String (v, 2) + "x"; })));
        g->addChild (std::make_unique<AudioParameterChoice> (ParameterID { "scr_direction", 1 }, "Scratch Direction", StringArray { "Forward", "Backward", "Both" }, 0));
        pc ("scr_curve", "Scratch Curve", 0.0f, 1.0f, 0.2f);
        ms ("scr_attack", "Scratch Attack", 0.5f, 50.0f, 2.0f);
        ms ("scr_release", "Scratch Release", 2.0f, 300.0f, 30.0f);
        pc ("scr_gate", "Scratch Gate", 0.0f, 1.0f, 0.0f);
        pc ("scr_resonance", "Scratch Resonance", 0.0f, 1.0f, 0.15f);
        pc ("scr_tone", "Scratch Tone", 0.0f, 1.0f, 0.85f);
        pc ("scr_noise", "Scratch Noise", 0.0f, 1.0f, 0.1f);
        pc ("scr_vinyl", "Scratch Vinyl", 0.0f, 1.0f, 0.15f);
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { "scr_pitch", 1 }, "Scratch Pitch", NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
            Attr().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " st"; })));
        pc ("scr_width", "Scratch Width", 0.0f, 1.0f, 0.3f);
        pc ("scr_momentum", "Scratch Momentum", 0.0f, 1.0f, 0.3f);
        pc ("scr_accel", "Scratch Acceleration", 0.0f, 1.0f, 0.0f);
        pc ("scr_decel", "Scratch Deceleration", 0.0f, 1.0f, 0.0f);
        pc ("scr_random", "Scratch Randomness", 0.0f, 1.0f, 0.15f);
        pc ("scr_swing", "Scratch Swing", 0.0f, 1.0f, 0.0f);
        g->addChild (std::make_unique<AudioParameterInt> (ParameterID { "scr_repeat", 1 }, "Scratch Repeat", 1, 8, 1));
        pc ("scr_variation", "Scratch Variation", 0.0f, 1.0f, 0.5f);
        g->addChild (std::make_unique<AudioParameterBool> (ParameterID { "scr_hold", 1 }, "Scratch Hold", false));
        g->addChild (std::make_unique<AudioParameterBool> (ParameterID { "scr_loop", 1 }, "Scratch Loop", false));
        pc ("scr_end", "Scratch End", 0.05f, 1.0f, 1.0f);
        pc ("scr_center", "Scratch Center", 0.0f, 1.0f, 0.5f);
        pc ("scr_fadein", "Scratch Fade In", 0.0f, 0.5f, 0.0f);
        pc ("scr_fadeout", "Scratch Fade Out", 0.0f, 0.5f, 0.0f);
        pc ("scr_intensity", "Scratch Intensity", 0.0f, 1.0f, 0.7f);
        pc ("scr_fx_filter", "Scratch FX Filter", 0.0f, 1.0f, 1.0f);
        pc ("scr_fx_drive", "Scratch FX Drive", 0.0f, 1.0f, 0.0f);
        pc ("scr_fx_sat", "Scratch FX Saturation", 0.0f, 1.0f, 0.0f);
        pc ("scr_fx_crush", "Scratch FX Bitcrush", 0.0f, 1.0f, 0.0f);
        pc ("scr_fx_delay", "Scratch FX Delay", 0.0f, 1.0f, 0.0f);
        pc ("scr_fx_reverb", "Scratch FX Reverb", 0.0f, 1.0f, 0.1f);
        pc ("scr_fx_width", "Scratch FX Width", 0.0f, 1.0f, 0.5f);
        pc ("scr_fx_vinyl", "Scratch FX Vinyl Noise", 0.0f, 1.0f, 0.0f);
        pc ("scr_fx_mix", "Scratch FX Mix", 0.0f, 1.0f, 1.0f);
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { "scr_master", 1 }, "Scratch Master", NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f,
            Attr().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; })));
        layout.add (std::move (g));
    }

    return layout;
}
} // namespace mm
