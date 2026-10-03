#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters/Parameters.h"
#include "ModulationDefs.h"

namespace mm
{
// Ponteiros diretos (lock-free) para os parametros, lidos no audio thread.
struct SynthParams
{
    using P = std::atomic<float>*;

    struct Osc { P wave{}, octave{}, semi{}, fine{}, phase{}, level{}, pan{}, wt{}, wtpos{}, fmratio{}, fmamt{}; };
    Osc osc[numOscillators];

    P subWave{}, subOctave{}, subLevel{};
    P noiseType{}, noiseLevel{};

    P filterType{}, cutoff{}, resonance{}, drive{}, keyTrack{}, mix{}, envAmount{};
    P fA{}, fD{}, fS{}, fR{};
    P aA{}, aD{}, aS{}, aR{};
    P mA{}, mD{}, mS{}, mR{};   // ENV 3 (Mod Env)
    P bendRange{};

    P voiceMode{}, voiceCount{}, glide{}, unisonVoices{}, unisonDetune{}, unisonSpread{};

    struct Lfo  { P shape{}, sync{}, rate{}, div{}, phase{}, fade{}, amount{}, smooth{}; };
    struct Slot { P src{}, dst{}, amt{}, curve{}, mode{}; };
    Lfo  lfo[numLfos];
    Slot slot[numModSlots];
    P    macro[numMacros] {};

    void attach (juce::AudioProcessorValueTreeState& s)
    {
        auto get = [&s] (const juce::String& id)
        {
            auto* p = s.getRawParameterValue (id);
            jassert (p != nullptr);
            return p;
        };

        for (int i = 0; i < numOscillators; ++i)
        {
            osc[i].wave   = get (ParamIDs::osc (i + 1, "wave"));
            osc[i].octave = get (ParamIDs::osc (i + 1, "octave"));
            osc[i].semi   = get (ParamIDs::osc (i + 1, "semi"));
            osc[i].fine   = get (ParamIDs::osc (i + 1, "fine"));
            osc[i].phase  = get (ParamIDs::osc (i + 1, "phase"));
            osc[i].level  = get (ParamIDs::osc (i + 1, "level"));
            osc[i].pan    = get (ParamIDs::osc (i + 1, "pan"));
            osc[i].wt      = get (ParamIDs::osc (i + 1, "wt"));
            osc[i].wtpos   = get (ParamIDs::osc (i + 1, "wtpos"));
            osc[i].fmratio = get (ParamIDs::osc (i + 1, "fmratio"));
            osc[i].fmamt   = get (ParamIDs::osc (i + 1, "fmamt"));
        }

        subWave = get (ParamIDs::subWave); subOctave = get (ParamIDs::subOctave); subLevel = get (ParamIDs::subLevel);
        noiseType = get (ParamIDs::noiseType); noiseLevel = get (ParamIDs::noiseLevel);

        filterType = get (ParamIDs::filterType); cutoff = get (ParamIDs::cutoff);
        resonance = get (ParamIDs::resonance); drive = get (ParamIDs::filterDrive);
        keyTrack = get (ParamIDs::keyTrack); mix = get (ParamIDs::filterMix);
        envAmount = get (ParamIDs::filterEnvAmt);

        fA = get (ParamIDs::fenvAttack); fD = get (ParamIDs::fenvDecay);
        fS = get (ParamIDs::fenvSustain); fR = get (ParamIDs::fenvRelease);
        aA = get (ParamIDs::ampAttack); aD = get (ParamIDs::ampDecay);
        aS = get (ParamIDs::ampSustain); aR = get (ParamIDs::ampRelease);
        mA = get (ParamIDs::menvAttack); mD = get (ParamIDs::menvDecay);
        mS = get (ParamIDs::menvSustain); mR = get (ParamIDs::menvRelease);

        bendRange = get (ParamIDs::bendRange);

        voiceMode = get (ParamIDs::voiceMode); voiceCount = get (ParamIDs::voiceCount);
        glide = get (ParamIDs::glide); unisonVoices = get (ParamIDs::unisonVoices);
        unisonDetune = get (ParamIDs::unisonDetune); unisonSpread = get (ParamIDs::unisonSpread);

        for (int i = 0; i < numLfos; ++i)
        {
            auto& l = lfo[i];
            l.shape = get (ParamIDs::lfo (i + 1, "shape")); l.sync = get (ParamIDs::lfo (i + 1, "sync"));
            l.rate = get (ParamIDs::lfo (i + 1, "rate"));   l.div = get (ParamIDs::lfo (i + 1, "div"));
            l.phase = get (ParamIDs::lfo (i + 1, "phase")); l.fade = get (ParamIDs::lfo (i + 1, "fade"));
            l.amount = get (ParamIDs::lfo (i + 1, "amount")); l.smooth = get (ParamIDs::lfo (i + 1, "smooth"));
        }
        for (int i = 0; i < numModSlots; ++i)
        {
            auto& m = slot[i];
            m.src = get (ParamIDs::mod (i + 1, "src"));     m.dst = get (ParamIDs::mod (i + 1, "dst"));
            m.amt = get (ParamIDs::mod (i + 1, "amt"));     m.curve = get (ParamIDs::mod (i + 1, "curve"));
            m.mode = get (ParamIDs::mod (i + 1, "mode"));
        }
        for (int i = 0; i < numMacros; ++i)
            macro[i] = get (ParamIDs::macro (i + 1));
    }
};

struct PerformanceState
{
    float modWheel   = 0.0f;
    float aftertouch = 0.0f;
};

// Estado global de modulacao compartilhado pelas vozes (escrito pelo SynthEngine)
struct GlobalModState
{
    const float* lfo[numLfos] {};  // valores dos LFOs (-1..1) do trecho atual
    int   chunkStart = 0;          // indice do buffer onde o trecho comeca
    float glideFromNote = -1.0f;   // nota de origem do glide para a proxima voz
};

// Funcoes compartilhadas entre DSP e interface (o grafico mostra exatamente o que o filtro faz)
struct FilterMode
{
    int  baseType;   // 0 LP, 1 HP, 2 BP, 3 Notch  (= StateVariableFilter::Type)
    bool twoStages;  // 24 dB/oct

    static FilterMode fromChoice (int choice) noexcept
    {
        switch (choice)
        {
            case 0:  return { 0, false }; // LP12
            case 1:  return { 0, true  }; // LP24
            case 2:  return { 1, false }; // HP12
            case 3:  return { 1, true  }; // HP24
            case 4:  return { 2, false }; // BP
            default: return { 3, false }; // NOTCH
        }
    }
};

inline float driveGain (float drive) noexcept         { return 1.0f + drive * 15.0f; }
inline float driveCompensation (float drive) noexcept { return 1.0f / (1.0f + drive * 3.0f); }
} // namespace mm
