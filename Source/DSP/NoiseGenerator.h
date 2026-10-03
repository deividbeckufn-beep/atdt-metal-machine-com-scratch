#pragma once
#include <juce_core/juce_core.h>
#include <cmath>

namespace mm
{
// Gerador de ruido: White, Pink (filtro de Paul Kellet) e Digital (sample&hold quantizado)
class NoiseGenerator
{
public:
    enum class Type { White = 0, Pink, Digital };

    void reset() noexcept { b0 = b1 = b2 = 0.0f; held = 0.0f; counter = 0; }

    float process (Type type) noexcept
    {
        const float w = random.nextFloat() * 2.0f - 1.0f;

        switch (type)
        {
            case Type::White:
                return w;

            case Type::Pink:
                b0 = 0.99765f * b0 + w * 0.0990460f;
                b1 = 0.96300f * b1 + w * 0.2965164f;
                b2 = 0.57000f * b2 + w * 1.0526913f;
                return (b0 + b1 + b2 + w * 0.1848f) * 0.25f;

            case Type::Digital:
                if (--counter <= 0)
                {
                    counter = 6;
                    held = std::round (w * 4.0f) * 0.25f;
                }
                return held;
        }
        return w;
    }

private:
    juce::Random random;
    float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f, held = 0.0f;
    int counter = 0;
};
} // namespace mm
