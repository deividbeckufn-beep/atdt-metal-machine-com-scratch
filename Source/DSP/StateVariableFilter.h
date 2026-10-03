#pragma once
#include <juce_core/juce_core.h>
#include <cmath>

namespace mm
{
// =============================================================================
//  Filtro State-Variable TPT (topologia "zero-delay feedback").
//  Estavel com modulacao rapida de cutoff (ideal para envelopes e LFOs).
//  Saidas: Low Pass, High Pass, Band Pass, Notch.
// =============================================================================
class StateVariableFilter
{
public:
    enum class Type { LowPass = 0, HighPass, BandPass, Notch };

    void prepare (double newSampleRate) noexcept { sampleRate = newSampleRate; reset(); }

    void reset() noexcept { ic1 = ic2 = 0.0f; }

    // resonance: 0..1  (0 = Q 0.5, 1 = Q ~20)
    void setParameters (float cutoffHz, float resonance) noexcept
    {
        const float fc = juce::jlimit (20.0f, (float) (sampleRate * 0.45), cutoffHz);
        g  = std::tan (juce::MathConstants<float>::pi * fc / (float) sampleRate);
        k  = 2.0f - 1.95f * juce::jlimit (0.0f, 1.0f, resonance);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    float process (float x, Type type) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;

        switch (type)
        {
            case Type::LowPass:  return v2;
            case Type::HighPass: return x - k * v1 - v2;
            case Type::BandPass: return v1;
            case Type::Notch:    return x - k * v1;
        }
        return v2;
    }

private:
    double sampleRate = 44100.0;
    float g = 0.0f, k = 2.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;
};
} // namespace mm
