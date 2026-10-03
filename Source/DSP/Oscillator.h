#pragma once
#include <juce_core/juce_core.h>
#include <cmath>
#include "WavetableBank.h"

namespace mm
{
// =============================================================================
//  Oscillator - oscilador com PolyBLEP (anti-aliasing) para Saw/Square/Pulse.
//  Sem alocacao de memoria: seguro para o audio thread.
//  Preparado para receber Wavetable/FM nas proximas fases (novas formas).
// =============================================================================
class Oscillator
{
public:
    enum class Wave { Sine = 0, Triangle, Saw, Square, Pulse, Noise, Wavetable, FM };

    void prepare (double newSampleRate) noexcept { sampleRate = newSampleRate; }

    void reset (double startPhase = 0.0) noexcept { phase = startPhase; modPhase = startPhase; smoothedPos = targetPos; }

    // ---- Wavetable: tabela + posicao (0..1). A posicao e suavizada por amostra (sem "degraus") ----
    void setWavetable (int tableIndex, float position) noexcept
    {
        table = juce::jlimit (0, WavetableBank::numTables() - 1, tableIndex);
        targetPos = juce::jlimit (0.0f, 1.0f, position);
    }

    // ---- FM (modulacao de fase): razao do modulador e indice 0..1 (ate ~8 radianos) ----
    void setFM (float ratio, float amount) noexcept
    {
        fmRatio = ratio;
        fmIndex = juce::jlimit (0.0f, 1.0f, amount) * 8.0f;
    }

    void setFrequency (double hz) noexcept
    {
        increment = juce::jlimit (0.0, 0.5, hz / sampleRate);
    }

    float process (Wave wave) noexcept
    {
        const double t = phase;
        const double dt = increment;
        float out = 0.0f;

        switch (wave)
        {
            case Wave::Sine:
                out = (float) std::sin (juce::MathConstants<double>::twoPi * t);
                break;

            case Wave::Triangle:
                out = (float) (1.0 - 4.0 * std::abs (t - 0.5));
                break;

            case Wave::Saw:
                out = (float) (2.0 * t - 1.0 - polyBlep (t, dt));
                break;

            case Wave::Square:
                out = (float) ((t < 0.5 ? 1.0 : -1.0)
                               + polyBlep (t, dt)
                               - polyBlep (std::fmod (t + 0.5, 1.0), dt));
                break;

            case Wave::Pulse:
            {
                constexpr double width = 0.25;
                double v = (t < width ? 1.0 : -1.0)
                           + polyBlep (t, dt)
                           - polyBlep (std::fmod (t + 1.0 - width, 1.0), dt);
                out = (float) (v - (2.0 * width - 1.0)); // remove DC
                break;
            }

            case Wave::Noise:
                out = random.nextFloat() * 2.0f - 1.0f;
                break;

            case Wave::Wavetable:
            {
                smoothedPos += (targetPos - smoothedPos) * 0.002f;
                out = WavetableBank::get().read (table, WavetableBank::mipFor (dt), smoothedPos, t);
                break;
            }

            case Wave::FM:
            {
                // indice reduzido nos agudos: evita aliasing (bandas laterais acima de Nyquist)
                const double modInc = dt * fmRatio;
                const double safe = juce::jlimit (0.0, 1.0, 1.0 - (dt * (1.0 + fmRatio * fmIndex)) * 4.0);
                const double m = std::sin (juce::MathConstants<double>::twoPi * modPhase) * fmIndex * safe;
                out = (float) std::sin (juce::MathConstants<double>::twoPi * t + m);
                modPhase += modInc;
                if (modPhase >= 1.0) modPhase -= std::floor (modPhase);
                break;
            }
        }

        phase += dt;
        if (phase >= 1.0)
            phase -= 1.0;

        return out;
    }

private:
    static double polyBlep (double t, double dt) noexcept
    {
        if (dt <= 0.0)
            return 0.0;

        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0;
        }

        if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }

        return 0.0;
    }

    double sampleRate = 44100.0;
    double phase = 0.0;
    double increment = 0.0;
    double modPhase = 0.0;
    int table = 0;
    float targetPos = 0.0f, smoothedPos = 0.0f;
    float fmRatio = 1.0f, fmIndex = 0.0f;
    juce::Random random;
};
} // namespace mm
