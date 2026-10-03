#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

namespace mm
{
// =============================================================================
//  SCRATCH SOURCES - fontes do SYNTH SCRATCH, todas geradas por sintese (nada gravado de terceiros)
//  0-3 mantem a v0.15 (vozes AHH/OHH/EHH + stab). 4 = User Sample (carregado no SamplePlayer).
// =============================================================================
namespace ScratchSources
{
    inline const juce::StringArray& names()
    {
        static const juce::StringArray n { "Vox AHH", "Vox OHH", "Vox EHH", "Synth Stab", "User Sample",
                                           "Noise Sweep", "808 Tone", "Digital Chirp", "Metal Hit", "Robot Vox", "Vinyl Texture" };
        return n;
    }
    constexpr int count = 11, userIndex = 4;

    inline void normalize (juce::AudioBuffer<float>& b)
    {
        const float pk = b.getMagnitude (0, b.getNumSamples());
        if (pk > 1e-5f) b.applyGain (0.9f / pk);
    }

    // voz por formantes (fonte glotal serra ou pulso quantizado = robo)
    inline void formantVoice (juce::AudioBuffer<float>& b, double sr, const double (&F)[3], bool robot)
    {
        const int n = b.getNumSamples();
        double s1[3] {}, s2[3] {};
        for (int i = 0; i < n; ++i)
        {
            const double t = i / sr;
            const double f0 = robot ? 110.0 : 150.0 * (1.0 + 0.03 * std::sin (juce::MathConstants<double>::twoPi * 5.0 * t));
            double src = 2.0 * std::fmod (t * f0, 1.0) - 1.0;
            if (robot) src = std::fmod (t * f0, 1.0) < 0.25 ? 1.0 : -0.33;
            double y = 0;
            for (int k = 0; k < 3; ++k)
            {
                const double r = std::exp (-juce::MathConstants<double>::pi * 90.0 / sr);
                const double c = 2.0 * r * std::cos (juce::MathConstants<double>::twoPi * F[k] / sr);
                const double out = src * (1.0 - r) + c * s1[k] - r * r * s2[k];
                s2[k] = s1[k]; s1[k] = out;
                y += out * (k == 0 ? 1.0 : k == 1 ? 0.7 : 0.35);
            }
            if (robot) y = std::round (y * 12.0) / 12.0;   // quantizado: carater digital
            const double env = juce::jmin (1.0, t / 0.02) * (t > 1.0 ? std::exp (-(t - 1.0) * 20.0) : 1.0);
            b.setSample (0, i, (float) (y * env));
        }
    }

    inline juce::AudioBuffer<float> make (int kind, double sr)
    {
        const int n = (int) (sr * 1.2);
        juce::AudioBuffer<float> b (1, n);
        b.clear();
        juce::Random rng (1234 + kind);
        static const double F[4][3] { { 800, 1150, 2900 }, { 450, 800, 2830 }, { 400, 2000, 2600 }, { 600, 1700, 2400 } };
        switch (kind)
        {
            case 0: case 1: case 2: formantVoice (b, sr, F[kind], false); break;
            case 9: formantVoice (b, sr, F[3], true); break;
            case 3:   // stab: acorde de serras com envelope
                for (int i = 0; i < n; ++i)
                {
                    const double t = i / sr; float x = 0;
                    for (double f : { 110.0, 164.8, 220.0, 261.6 }) x += (float) (2.0 * std::fmod (t * f, 1.0) - 1.0);
                    b.setSample (0, i, x * 0.22f * (float) std::exp (-t * 2.0));
                }
                break;
            case 5:   // varredura de ruido (passa-banda subindo)
            {
                double lp = 0, bp = 0;
                for (int i = 0; i < n; ++i)
                {
                    const double t = i / sr, fc = 300.0 * std::pow (20.0, t / 1.2);
                    const double f = 2.0 * std::sin (juce::MathConstants<double>::pi * juce::jmin (0.45, fc / sr));
                    const double x = rng.nextFloat() * 2.0 - 1.0;
                    lp += f * bp; const double hp = x - lp - 0.3 * bp; bp += f * hp;
                    b.setSample (0, i, (float) bp);
                }
                break;
            }
            case 6:   // 808: seno grave com queda de pitch e saturacao
            {
                double ph = 0;   // fase local (nunca estatica: seria compartilhada entre instancias)
                for (int i = 0; i < n; ++i)
                {
                    const double t = i / sr, f = 55.0 * (1.0 + 1.5 * std::exp (-t * 25.0));
                    ph += f / sr;
                    b.setSample (0, i, (float) std::tanh (2.5 * std::sin (juce::MathConstants<double>::twoPi * ph)) * (float) std::exp (-t * 1.5));
                }
                break;
            }
            case 7:   // chirp digital: varredura de seno quantizada
            {
                double ph = 0;
                for (int i = 0; i < n; ++i)
                {
                    const double t = i / sr, f = 200.0 + 3000.0 * std::fmod (t * 3.0, 1.0);
                    ph += f / sr;
                    b.setSample (0, i, (float) (std::round (std::sin (juce::MathConstants<double>::twoPi * ph) * 6.0) / 6.0));
                }
                break;
            }
            case 8:   // golpe metalico: parciais inarmonicos + ruido
                for (int i = 0; i < n; ++i)
                {
                    const double t = i / sr; double x = 0;
                    for (double f : { 317.0, 489.0, 731.0, 1043.0, 1517.0 }) x += std::sin (juce::MathConstants<double>::twoPi * f * t);
                    x = x * 0.2 + (rng.nextFloat() * 2.0 - 1.0) * 0.3 * std::exp (-t * 30.0);
                    b.setSample (0, i, (float) (x * std::exp (-t * 2.5)));
                }
                break;
            default:  // 10 textura de vinil: ruido rosado + estalos + zumbido
            {
                double p = 0;
                for (int i = 0; i < n; ++i)
                {
                    const double t = i / sr;
                    p = 0.98 * p + 0.02 * (rng.nextFloat() * 2.0 - 1.0);
                    double x = p * 4.0 + 0.15 * std::sin (juce::MathConstants<double>::twoPi * 60.0 * t);
                    if (rng.nextFloat() < 0.0008f) x += (rng.nextFloat() * 2.0 - 1.0) * 1.5;
                    b.setSample (0, i, (float) x);
                }
                break;
            }
        }
        normalize (b);
        return b;
    }
}
} // namespace mm
