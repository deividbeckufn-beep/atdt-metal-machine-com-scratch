#pragma once
#include <juce_core/juce_core.h>
#include <cmath>

namespace mm
{
// =============================================================================
//  Definicoes do sistema de modulacao (compartilhadas por DSP e interface).
//  Para adicionar uma fonte/destino: acrescente NO FINAL do enum e do nome
//  (nunca no meio - quebraria presets e projetos salvos).
// =============================================================================
inline constexpr int numLfos = 4;
inline constexpr int numModSlots = 8;
inline constexpr int numMacros = 4;

enum class ModSource
{
    None = 0, Lfo1, Lfo2, Lfo3, Lfo4, FilterEnv, AmpEnv, Velocity, ModWheel, Aftertouch, Note,
    Macro1, Macro2, Macro3, Macro4,
    Random, ModEnv,                                   // v0.4
    Count
};

enum class ModDest
{
    None = 0, PitchAll, Osc1Pitch, Osc2Pitch, Osc3Pitch, Osc1Level, Osc2Level, Osc3Level,
    SubLevel, NoiseLevel, Cutoff, Resonance, Drive, FilterMix, Amp, Pan, UnisonDetune,
    Lfo1Amount, Lfo2Amount, Lfo3Amount, Lfo4Amount,
    Osc1WtPos, Osc2WtPos, Osc3WtPos, Osc1FmAmount, Osc2FmAmount, Osc3FmAmount,   // v0.4
    Count
};

inline const juce::StringArray& modSourceNames()
{
    static const juce::StringArray n { "None", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "Filter Env", "Amp Env",
                                       "Velocity", "Mod Wheel", "Aftertouch", "Note",
                                       "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Random", "Mod Env" };
    return n;
}

inline const juce::StringArray& modSourceShortNames()
{
    static const juce::StringArray n { "-", "LFO1", "LFO2", "LFO3", "LFO4", "F.ENV", "A.ENV",
                                       "VEL", "MW", "AT", "NOTE", "M1", "M2", "M3", "M4", "RND", "M.ENV" };
    return n;
}

inline const juce::StringArray& modDestNames()
{
    static const juce::StringArray n { "None", "Pitch", "OSC1 Pitch", "OSC2 Pitch", "OSC3 Pitch",
                                       "OSC1 Level", "OSC2 Level", "OSC3 Level", "Sub Level", "Noise Level",
                                       "Cutoff", "Resonance", "Drive", "Filter Mix", "Amp", "Pan", "Unison Detune",
                                       "LFO1 Amount", "LFO2 Amount", "LFO3 Amount", "LFO4 Amount",
                                       "OSC1 WT Pos", "OSC2 WT Pos", "OSC3 WT Pos",
                                       "OSC1 FM Amt", "OSC2 FM Amt", "OSC3 FM Amt" };
    return n;
}

inline const juce::StringArray& modCurveNames() { static const juce::StringArray n { "Linear", "Exp", "Log" }; return n; }
inline const juce::StringArray& modModeNames()  { static const juce::StringArray n { "Normal", "Invert" };      return n; }

// LFOs e Note vao de -1 a +1; as outras fontes de 0 a 1
inline bool isBipolarSource (int s) noexcept
{
    return (s >= (int) ModSource::Lfo1 && s <= (int) ModSource::Lfo4) || s == (int) ModSource::Note
        || s == (int) ModSource::Random;
}

inline bool isLfoAmountDest (int d) noexcept
{
    return d >= (int) ModDest::Lfo1Amount && d <= (int) ModDest::Lfo4Amount;
}

inline float applyModCurve (float v, int curve) noexcept
{
    switch (curve)
    {
        case 1:  return v * std::abs (v);                                   // Exp
        case 2:  return std::copysign (std::sqrt (std::abs (v)), v);        // Log
        default: return v;                                                  // Linear
    }
}

// Escala de cada destino quando amount = 100%
namespace ModScale
{
    inline constexpr float pitchSemitones = 24.0f;
    inline constexpr float cutoffOctaves  = 6.0f;
}

// Fracao do curso do knob coberta por amount = 100% (usada no anel de modulacao)
inline float destKnobSpan (ModDest d) noexcept
{
    if (d == ModDest::Cutoff)
        return ModScale::cutoffOctaves / 9.97f;   // 20 Hz - 20 kHz = ~10 oitavas
    return 1.0f;                                  // SEMI (24 st), levels, etc.
}

// Qual destino corresponde a cada knob (para arrastar-e-soltar e o anel)
inline ModDest destForParamID (const juce::String& id)
{
    static const std::pair<const char*, ModDest> map[] {
        { "osc1_semi", ModDest::Osc1Pitch }, { "osc2_semi", ModDest::Osc2Pitch }, { "osc3_semi", ModDest::Osc3Pitch },
        { "osc1_level", ModDest::Osc1Level }, { "osc2_level", ModDest::Osc2Level }, { "osc3_level", ModDest::Osc3Level },
        { "sub_level", ModDest::SubLevel }, { "noise_level", ModDest::NoiseLevel },
        { "filter_cutoff", ModDest::Cutoff }, { "filter_resonance", ModDest::Resonance },
        { "filter_drive", ModDest::Drive }, { "filter_mix", ModDest::FilterMix },
        { "unison_detune", ModDest::UnisonDetune },
        { "lfo1_amount", ModDest::Lfo1Amount }, { "lfo2_amount", ModDest::Lfo2Amount },
        { "lfo3_amount", ModDest::Lfo3Amount }, { "lfo4_amount", ModDest::Lfo4Amount },
        { "osc1_wtpos", ModDest::Osc1WtPos }, { "osc2_wtpos", ModDest::Osc2WtPos }, { "osc3_wtpos", ModDest::Osc3WtPos },
        { "osc1_fmamt", ModDest::Osc1FmAmount }, { "osc2_fmamt", ModDest::Osc2FmAmount }, { "osc3_fmamt", ModDest::Osc3FmAmount } };

    for (auto& [pid, dest] : map)
        if (id == pid)
            return dest;
    return ModDest::None;
}
} // namespace mm
