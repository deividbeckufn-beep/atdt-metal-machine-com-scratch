#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

namespace mm
{
// =============================================================================
//  SCRATCH FX - cadeia propria do Scratch Machine (estereo, por amostra, sem alocacao no audio):
//  FILTER (LP com ressonancia) > DRIVE > SATURATION > BITCRUSH > DELAY (1/8 pontuada, ping-pong) > REVERB
//  > WIDTH (M/S) > VINYL NOISE > FX MIX (seco/molhado) > MASTER
// =============================================================================
class ScratchFX
{
public:
    struct Settings { float filter = 1, drive = 0, sat = 0, crush = 0, delay = 0, reverb = 0, width = 0.5f, vinyl = 0, mix = 1, masterDb = 0; };

    void prepare (double sr)
    {
        sampleRate = sr;
        delayLen = (int) (sr * 2.0) + 2;
        for (auto& d : delayBuf) d.assign ((size_t) delayLen, 0.0f);
        wp = 0; ic1 = ic2 = { 0, 0, 0, 0 }; crushHold = { 0, 0 }; crushCount = 0; dcIn = dcOut = { 0, 0 };
        reverb.setSampleRate (sr);
        reverb.reset();
    }

    // 'active': ha scratch acontecendo; o ruido de vinil some ~0,4 s depois (nunca toca para sempre)
    void process (float* L, float* R, int n, const Settings& s, double bpm, bool active)
    {
        const float bedDecay = std::exp (-1.0f / (0.4f * (float) sampleRate));
        // filtro SVF (TPT) passa-baixa: 200 Hz .. 18 kHz
        const double fc = 200.0 * std::pow (90.0, (double) s.filter);
        const double g = std::tan (juce::MathConstants<double>::pi * juce::jmin (0.45, fc / sampleRate));
        const double k = 1.2;   // amortecimento (Q ~0,8): filtro de tom limpo; a ressonancia expressiva esta no motor
        const double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float drive = 1.0f + s.drive * 12.0f, sat = s.sat;
        const float levels = std::exp2 (16.0f - s.crush * 13.0f) * 0.5f;
        const int down = 1 + (int) (s.crush * 12.0f);
        const int dlen = juce::jlimit (16, delayLen - 2, (int) (0.75 * 60.0 / juce::jlimit (20.0, 999.0, bpm) * sampleRate));
        const float master = juce::Decibels::decibelsToGain (s.masterDb);
        for (int i = 0; i < n; ++i)
        {
            float x[2] { L[i], R[i] };
            const float dry[2] { x[0], x[1] };
            for (size_t c = 0; c < 2; ++c)
            {
                const double v3 = x[c] - ic2[c];
                const double v1 = a1 * ic1[c] + a2 * v3, v2 = ic2[c] + a2 * ic1[c] + a3 * v3;
                ic1[c] = 2.0 * v1 - ic1[c]; ic2[c] = 2.0 * v2 - ic2[c];
                float y = (float) v2;
                if (s.drive > 0.001f) y = std::tanh (y * drive) / std::tanh (juce::jmin (drive, 3.0f));
                if (sat > 0.001f) { const float z = y * (1.0f + 3.0f * sat); y = (z / (1.0f + std::abs (z))) * (1.0f + 0.15f * sat); }
                x[c] = y;
            }
            if (s.crush > 0.001f)
            {
                if (crushCount == 0) for (int c = 0; c < 2; ++c) crushHold[(size_t) c] = std::round (x[c] * levels) / levels;
                if (++crushCount >= down) crushCount = 0;
                x[0] = crushHold[0]; x[1] = crushHold[1];
            }
            // delay ping-pong
            const int rp = (wp - dlen + delayLen) % delayLen;
            const float dl = delayBuf[0][(size_t) rp], dr = delayBuf[1][(size_t) rp];
            delayBuf[0][(size_t) wp] = 0.5f * (x[0] + x[1]) + dr * 0.38f;
            delayBuf[1][(size_t) wp] = dl * 0.38f;
            if (++wp >= delayLen) wp = 0;
            // delay com compensacao do seco (ganho constante)
            x[0] = x[0] * (1.0f - 0.25f * s.delay) + dl * s.delay * 0.5f;
            x[1] = x[1] * (1.0f - 0.25f * s.delay) + dr * s.delay * 0.5f;
            // largura (M/S)
            const float m = 0.5f * (x[0] + x[1]), sd = 0.5f * (x[0] - x[1]) * (0.2f + 1.0f * s.width);   // largura limitada (mono-compativel)
            x[0] = m + sd; x[1] = m - sd;
            // ruido de vinil (chiado rosado + estalos)
            bedEnv = active ? 1.0f : bedEnv * bedDecay;
            if (s.vinyl > 0.001f && bedEnv > 1e-4f)
            {
                pink = 0.97f * pink + 0.03f * (rng.nextFloat() * 2.0f - 1.0f);
                float v = pink * 0.08f;
                if (rng.nextFloat() < 0.0004f) v += (rng.nextFloat() * 2.0f - 1.0f) * 0.25f;
                x[0] += v * s.vinyl * bedEnv; x[1] += v * s.vinyl * bedEnv;
            }
            L[i] = (dry[0] + (x[0] - dry[0]) * s.mix) * master;
            R[i] = (dry[1] + (x[1] - dry[1]) * s.mix) * master;
        }
        if (s.reverb > 0.001f)
        {
            processReverb (L, R, n, s);
        }
        // protecao de saida suave: transparente ate 0,8; teto de 1,0 (nunca clipa duro)
        for (int i = 0; i < n; ++i) { L[i] = softCeil (L[i]); R[i] = softCeil (R[i]); }
    }

private:
    static float softCeil (float x)
    {
        const float a = std::abs (x);
        return a <= 0.8f ? x : std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f), x);
    }
    void processReverb (float* L, float* R, int n, const Settings& s)
    {
        {
            juce::Reverb::Parameters rp; rp.roomSize = 0.75f; rp.damping = 0.5f; rp.width = 1.0f;
            rp.wetLevel = 0.22f * s.reverb * s.mix; rp.dryLevel = 1.0f - 0.3f * s.reverb * s.mix;   // reverb compensa o seco
            reverb.setParameters (rp);
            reverb.processStereo (L, R, n);
        }
    }

    double sampleRate = 44100.0;
    std::array<std::vector<float>, 2> delayBuf;
    int delayLen = 1, wp = 0, crushCount = 0;
    std::array<double, 4> ic1 {}, ic2 {};
    std::array<float, 2> crushHold {}, dcIn {}, dcOut {};
    float pink = 0.0f, bedEnv = 0.0f;
    juce::Random rng;
    juce::Reverb reverb;
};
} // namespace mm
