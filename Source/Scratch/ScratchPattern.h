#pragma once
#include <juce_core/juce_core.h>
#include <cmath>

namespace mm
{
// =============================================================================
//  SCRATCH PATTERN - cada padrao = curva de POSICAO do disco p(u) (0..1) + CROSSFADER g(u) (0/1)
//  u = 0..1 no tempo do golpe. Os 8 primeiros mantem os indices da v0.15 (compatibilidade).
//  'seed' deixa golpes como GLITCH diferentes a cada disparo; 'variation' muda a forma sem mudar o padrao.
// =============================================================================
namespace ScratchPattern
{
    enum Id { Baby = 0, Forward, Chirp, Transformer, Tear, Flare, Stab, Scribble,
              Backward, Orbit, Crab, Slicer, Stutter, Reverse, Brake, VinylStop, Rhythmic, Glitch, Random, Count };

    inline const juce::StringArray& names()
    {
        static const juce::StringArray n { "Baby", "Forward", "Chirp", "Transformer", "Tear", "Flare", "Stab", "Scribble",
                                           "Backward", "Orbit", "Crab", "Slicer", "Stutter", "Reverse", "Brake", "Vinyl Stop",
                                           "Rhythmic", "Glitch", "Random" };
        return n;
    }

    // CURVE: 0 = senoide (mao suave) .. 1 = triangular/linear (mao seca e rapida)
    inline double stroke (double x, double curve)
    {
        const double s = std::sin (juce::MathConstants<double>::pi * x);
        const double tri = 1.0 - std::abs (2.0 * x - 1.0);
        return s + (tri - s) * curve;
    }
    // ACCELERATION / DECELERATION: deformam o tempo (mao que acelera no inicio / freia no fim)
    inline double warp (double u, double accel, double decel)
    {
        const double a = 1.0 + 2.0 * accel, d = 1.0 + 2.0 * decel;
        if (u < 0.5) return 0.5 * std::pow (2.0 * u, a);
        return 1.0 - 0.5 * std::pow (2.0 * (1.0 - u), d);
    }
    inline uint32_t hash (uint32_t x) { x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16; return x; }

    inline double motion (int id, double u, double curve, double variation, uint32_t seed, float& gate)
    {
        const double pi = juce::MathConstants<double>::pi;
        gate = 1.0f;
        const double chops = 4.0 + std::round (variation * 8.0);   // VARIATION: mais/menos cortes
        switch (id)
        {
            case Baby:        return stroke (u, curve);
            case Forward:     gate = u < 0.5 ? 1.0f : 0.0f; return stroke (u, curve);
            case Chirp:       gate = (u > 0.42 && u < 0.58) ? 0.0f : 1.0f; return stroke (u, curve);
            case Transformer: gate = std::fmod (u * chops * 2.0, 1.0) < 0.5 ? 1.0f : 0.0f; return stroke (u, curve);
            case Tear:        return u < 0.5 ? (u < 0.2 ? u / 0.2 * 0.5 : u < 0.3 ? 0.5 : 0.5 + (u - 0.3) / 0.2 * 0.5) : 1.0 - (u - 0.5) / 0.5;
            case Flare:       gate = (u > 0.18 && u < 0.26) ? 0.0f : 1.0f; return stroke (u, curve);
            case Stab:        gate = u < 0.25 ? 1.0f : 0.0f; return stroke (juce::jmin (1.0, u * 2.0), curve) * 0.6;
            case Scribble:    return 0.15 * (1.0 + std::sin (2.0 * pi * (6.0 + variation * 6.0) * u - pi * 0.5));
            case Backward:    gate = u >= 0.5 ? 1.0f : 0.0f; return stroke (u, curve);                    // so a volta
            case Orbit:       gate = ((u > 0.18 && u < 0.24) || (u > 0.76 && u < 0.82)) ? 0.0f : 1.0f;      // flare na ida e na volta
                              return stroke (u, curve);
            case Crab:        { const double c = std::fmod (u * 8.0, 1.0); gate = (u < 0.5 && c > 0.55) ? 0.0f : 1.0f;  // 4 cliques rapidos na ida
                                return stroke (u, curve); }
            case Slicer:      gate = std::fmod (u * (8.0 + variation * 8.0), 1.0) < 0.45 ? 1.0f : 0.0f;
                              return juce::jmin (1.0, u * 1.4);                                                 // ida longa fatiada
            case Stutter:     { const double k = std::fmod (u * 4.0, 1.0); gate = k < 0.35 ? 1.0f : 0.0f;       // 4 stabs curtos
                                return stroke (juce::jmin (1.0, k * 2.5), curve) * 0.5; }
            case Reverse:     return 1.0 - u;                                                                  // de tras para frente
            case Brake:       return 1.0 - (1.0 - u) * (1.0 - u);                                              // desacelera ate parar
            case VinylStop:   return u * (1.0 - 0.5 * u) * 2.0 * 0.5 + 0.0;                                    // velocidade 1 -> 0
            case Rhythmic:    gate = std::fmod (u * 4.0, 1.0) < 0.6 ? 1.0f : 0.0f; return stroke (u, curve);
            case Glitch:      { const auto seg = (uint32_t) (u * 8.0);                                         // saltos aleatorios
                                const double base = (hash (seed + seg * 977u) % 1000) / 1000.0;
                                gate = (hash (seed * 31u + seg) % 3) == 0 ? 0.0f : 1.0f;
                                return base + 0.15 * std::fmod (u * 8.0, 1.0); }
            default:          return stroke (u, curve);
        }
    }
}
} // namespace mm
