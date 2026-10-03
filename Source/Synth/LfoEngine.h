#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <vector>
#include "SynthParams.h"

namespace mm
{
// =============================================================================
//  LfoEngine - 4 LFOs globais.
//  SYNC ligado + REAPER tocando: fase travada na posicao da musica (PPQ),
//  entao o LFO cai sempre no mesmo ponto do compasso, inclusive em render.
//  SYNC ligado + parado: segue o BPM em modo livre. SYNC desligado: Hz.
//  Buffers alocados no prepare -> nenhuma alocacao no audio thread.
// =============================================================================
class LfoEngine
{
public:
    static constexpr int capacity = 4096;   // blocos maiores sao processados em partes

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        for (auto& b : buffers)
            b.assign ((size_t) capacity, 0.0f);
        for (int i = 0; i < numLfos; ++i)
        {
            state[(size_t) i] = {};
            state[(size_t) i].r0 = random.nextFloat() * 2.0f - 1.0f;
            state[(size_t) i].r1 = random.nextFloat() * 2.0f - 1.0f;
        }
    }

    // Forma de onda pura (tambem usada pelo display da interface)
    static float shapeValue (int shape, float p, float r0, float r1, float held) noexcept
    {
        switch (shape)
        {
            case 0:  return std::sin (juce::MathConstants<float>::twoPi * p);
            case 1:  return 1.0f - 4.0f * std::abs (p - 0.5f) - 0.0f;           // triangulo (comeca em -1)
            case 2:  return 1.0f - 2.0f * p;                                     // saw descendente
            case 3:  return p < 0.5f ? 1.0f : -1.0f;
            case 4:  return held;                                                // sample & hold
            case 6:  return 2.0f * p - 1.0f;                                     // reverse saw (sobe)
            default: return r0 + (r1 - r0) * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * p)); // random suave
        }
    }

    void process (int numSamples, const SynthParams& params, double bpm, bool playing, double ppqAtStart)
    {
        jassert (numSamples <= capacity);
        numSamples = juce::jmin (numSamples, capacity);
        bpm = juce::jlimit (20.0, 999.0, bpm);

        for (int k = 0; k < numLfos; ++k)
        {
            auto& st = state[(size_t) k];
            const auto& lp = params.lfo[k];
            const int   shape = juce::jlimit (0, 6, (int) lp.shape->load());
            const bool  sync  = lp.sync->load() > 0.5f;
            const float offset = lp.phase->load();

            double inc;
            if (sync)
            {
                const double beats = lfoDivBeats[juce::jlimit (0, 10, (int) lp.div->load())];
                inc = bpm / 60.0 / beats / sampleRate;
                if (playing)
                {
                    const double pos = ppqAtStart / beats;
                    st.phase = pos - std::floor (pos);
                }
            }
            else
            {
                inc = lp.rate->load() / sampleRate;
            }

            float* out = buffers[(size_t) k].data();
            float value = 0.0f;
            // SMOOTH: filtro de 1 polo (0 = desligado, 100% = ~60 ms), arredonda S&H/quadrada
            const float smooth = lp.smooth != nullptr ? lp.smooth->load() : 0.0f;
            const float coef = smooth > 0.001f ? 1.0f - std::exp (-1.0f / (smooth * 0.06f * (float) sampleRate)) : 1.0f;
            for (int s = 0; s < numSamples; ++s)
            {
                float p = (float) (st.phase + offset);
                p -= std::floor (p);

                if (p < st.lastP)   // novo ciclo
                {
                    st.r0 = st.r1;
                    st.r1 = random.nextFloat() * 2.0f - 1.0f;
                    st.held = st.r1;
                }
                st.lastP = p;

                st.smoothed += (shapeValue (shape, p, st.r0, st.r1, st.held) - st.smoothed) * coef;
                value = st.smoothed;
                out[s] = value;

                st.phase += inc;
                if (st.phase >= 1.0) st.phase -= std::floor (st.phase);
            }

            uiPhase[(size_t) k].store (st.lastP);
            uiValue[(size_t) k].store (value);
        }
    }

    const float* getBuffer (int k) const noexcept { return buffers[(size_t) k].data(); }
    float getUiPhase (int k) const noexcept       { return uiPhase[(size_t) k].load(); }
    float getUiValue (int k) const noexcept       { return uiValue[(size_t) k].load(); }

private:
    struct State { double phase = 0.0; float lastP = 0.0f, r0 = 0.0f, r1 = 0.0f, held = 0.0f, smoothed = 0.0f; };

    double sampleRate = 44100.0;
    std::array<std::vector<float>, numLfos> buffers;
    std::array<State, numLfos> state {};
    std::array<std::atomic<float>, numLfos> uiPhase {}, uiValue {};
    juce::Random random;
};
} // namespace mm
