#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

namespace mm
{
// =============================================================================
//  WavetableBank - wavetables ORIGINAIS geradas por sintese aditiva (nenhum
//  conteudo de terceiros). Cada tabela tem 32 quadros (posicao 0..1) e 10
//  mipmaps: o mipmap k contem no maximo 512 >> k harmonicos, entao notas agudas
//  usam versoes com menos harmonicos e nao geram aliasing.
//  Construido uma unica vez (fora do audio thread) e compartilhado por todas as
//  vozes/instancias. Leitura no audio thread e somente leitura (sem locks).
//  Futuro: importar .wav do usuario em /Content/Wavetables usando o mesmo formato.
// =============================================================================
class WavetableBank
{
public:
    static constexpr int tableSize = 2048;
    static constexpr int numFrames = 32;
    static constexpr int numMips   = 10;
    static constexpr int maxHarmonics = 512;

    static const juce::StringArray& names()
    {
        static const juce::StringArray n { "Basic Morph", "Metal Formant", "Digital Grit", "Vocal A-O",
                                           "Growl", "PWM Sweep", "Harsh Sync" };
        return n;
    }
    static int numTables() { return names().size(); }

    static const WavetableBank& get()
    {
        static const WavetableBank bank;   // inicializacao thread-safe (C++11)
        return bank;
    }

    const float* frame (int table, int mip, int frameIndex) const noexcept
    {
        const size_t idx = (((size_t) table * numMips + (size_t) mip) * numFrames + (size_t) frameIndex) * tableSize;
        return data.data() + idx;
    }

    // Mipmap adequado: garante harmonicos <= Nyquist para a frequencia tocada
    static int mipFor (double increment) noexcept
    {
        const double maxH = 0.5 / juce::jmax (1.0e-9, increment);   // harmonicos que cabem ate Nyquist
        int mip = 0;
        while (mip < numMips - 1 && (double) (maxHarmonics >> mip) > maxH)
            ++mip;
        return mip;
    }

    // Leitura com interpolacao linear entre amostras e entre quadros (posicao continua)
    float read (int table, int mip, float position, double phase) const noexcept
    {
        const float fpos = juce::jlimit (0.0f, 1.0f, position) * (float) (numFrames - 1);
        const int f0 = juce::jmin ((int) fpos, numFrames - 1);
        const int f1 = juce::jmin (f0 + 1, numFrames - 1);
        const float ff = fpos - (float) f0;

        const double x = phase * tableSize;
        const int i0 = (int) x & (tableSize - 1);
        const int i1 = (i0 + 1) & (tableSize - 1);
        const float xf = (float) (x - std::floor (x));

        const float* a = frame (table, mip, f0);
        const float* b = frame (table, mip, f1);
        const float va = a[i0] + (a[i1] - a[i0]) * xf;
        const float vb = b[i0] + (b[i1] - b[i0]) * xf;
        return va + (vb - va) * ff;
    }

    // Amplitude do harmonico h (1..512) na posicao p (0..1) de cada tabela
    static float harmonicAmp (int table, int h, float p) noexcept
    {
        const float fh = (float) h;
        auto gauss = [] (float x, float c, float w) { const float d = (x - c) / w; return std::exp (-d * d); };

        switch (table)
        {
            case 0: // Basic Morph: seno -> triangulo -> dente de serra -> quadrada
            {
                const float tri = (h % 2 == 1) ? 1.0f / (fh * fh) : 0.0f;
                const float saw = 1.0f / fh;
                const float sq  = (h % 2 == 1) ? 1.0f / fh : 0.0f;
                const float sine = h == 1 ? 1.0f : 0.0f;
                const float seg = p * 3.0f;
                if (seg < 1.0f) return sine + (tri - sine) * seg;
                if (seg < 2.0f) return tri + (saw - tri) * (seg - 1.0f);
                return saw + (sq - saw) * (seg - 2.0f);
            }
            case 1: // Metal Formant: serra com pico ressonante que sobe
                return (1.0f / fh) * (1.0f + 7.0f * gauss (fh, 3.0f + p * 40.0f, 3.0f));
            case 2: // Digital Grit: padrao de cancelamentos "digitais" que muda com a posicao
            {
                const float comb = 0.5f + 0.5f * std::cos (fh * (0.15f + p * 1.6f));
                const float odd  = (h % 2 == 1) ? 1.0f : 0.35f + 0.65f * (1.0f - p);
                return (1.0f / std::sqrt (fh)) * comb * odd / fh * 3.0f;
            }
            case 3: // Vocal A-O: dois formantes que se deslocam (A -> O)
            {
                const float f1 = 7.0f - p * 3.0f, f2 = 12.0f - p * 5.0f;
                return (1.0f / fh) * (0.25f + 3.0f * gauss (fh, f1, 1.6f) + 2.2f * gauss (fh, f2, 2.2f));
            }
            case 4: // Growl: formantes largos + pares enfraquecidos
            {
                const float c1 = 2.0f + p * 12.0f;
                const float even = (h % 2 == 0) ? 1.0f - 0.7f * p : 1.0f;
                return (1.0f / fh) * even * (0.6f + 4.0f * gauss (fh, c1, 1.5f + p * 3.0f) + 2.5f * gauss (fh, c1 * 2.5f, 3.0f));
            }
            case 5: // PWM Sweep: pulso de largura 50% -> 5%
            {
                const float w = 0.5f - p * 0.45f;
                return std::abs (std::sin (juce::MathConstants<float>::pi * fh * w)) * 2.0f / (juce::MathConstants<float>::pi * fh);
            }
            default: // Harsh Sync: aproximacao espectral de hard sync (razao 1 -> 8)
            {
                const float r = 1.0f + p * 7.0f;
                return (1.0f / fh) * (1.0f + 4.0f * gauss (fh, r, 1.2f));
            }
        }
    }

private:
    WavetableBank()
    {
        const int nt = numTables();
        data.assign ((size_t) nt * numMips * numFrames * tableSize, 0.0f);

        constexpr int order = 11;   // 2^11 = 2048
        static_assert ((1 << order) == tableSize, "tamanho da tabela");
        juce::dsp::FFT fft (order);
        std::vector<float> buf ((size_t) tableSize * 2);

        for (int t = 0; t < nt; ++t)
            for (int f = 0; f < numFrames; ++f)
            {
                const float p = (float) f / (float) (numFrames - 1);
                float norm = 0.0f;

                for (int m = 0; m < numMips; ++m)
                {
                    const int limit = maxHarmonics >> m;
                    std::fill (buf.begin(), buf.end(), 0.0f);
                    // espectro: harmonico h com fase de seno -> componente imaginaria
                    for (int h = 1; h <= limit && h < tableSize / 2; ++h)
                        buf[(size_t) (2 * h + 1)] = -harmonicAmp (t, h, p);

                    fft.performRealOnlyInverseTransform (buf.data());

                    float* dst = data.data() + (((size_t) t * numMips + (size_t) m) * numFrames + (size_t) f) * tableSize;
                    for (int i = 0; i < tableSize; ++i)
                        dst[i] = buf[(size_t) i];

                    if (m == 0)   // normaliza pelo pico do mipmap completo: todos os mipmaps com o mesmo ganho
                        for (int i = 0; i < tableSize; ++i)
                            norm = juce::jmax (norm, std::abs (dst[i]));

                    const float g = norm > 1.0e-9f ? 1.0f / norm : 0.0f;
                    for (int i = 0; i < tableSize; ++i)
                        dst[i] *= g;
                }
            }
    }

    std::vector<float> data;
};
} // namespace mm
