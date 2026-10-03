#pragma once
#include <juce_core/juce_core.h>

namespace mm
{
// =============================================================================
//  Escalas e conversao "grau da escala -> nota MIDI".
//  Os padroes do sequencer sao escritos em GRAUS (0 = tonica, 2 = terca...),
//  entao continuam musicais em qualquer KEY/SCALE escolhida.
// =============================================================================
inline const juce::StringArray& noteNames()
{
    static const juce::StringArray n { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n;
}

inline const juce::StringArray& scaleNames()
{
    static const juce::StringArray n { "Minor", "Harmonic Minor", "Phrygian", "Phrygian Dominant",
                                       "Dorian", "Locrian", "Major", "Chromatic",
                                       "Melodic Minor", "Minor Pentatonic", "Blues" };   // v0.12 (sempre no final)
    return n;
}

struct ScaleDef { int size; int steps[12]; };

inline const ScaleDef& scaleDef (int index) noexcept
{
    static const ScaleDef defs[] {
        { 7, { 0, 2, 3, 5, 7, 8, 10 } },   // Minor
        { 7, { 0, 2, 3, 5, 7, 8, 11 } },   // Harmonic Minor
        { 7, { 0, 1, 3, 5, 7, 8, 10 } },   // Phrygian
        { 7, { 0, 1, 4, 5, 7, 8, 10 } },   // Phrygian Dominant
        { 7, { 0, 2, 3, 5, 7, 9, 10 } },   // Dorian
        { 7, { 0, 1, 3, 5, 6, 8, 10 } },   // Locrian
        { 7, { 0, 2, 4, 5, 7, 9, 11 } },   // Major
        { 12, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } }, // Chromatic
        { 7, { 0, 2, 3, 5, 7, 9, 11 } },   // Melodic Minor
        { 5, { 0, 3, 5, 7, 10 } },         // Minor Pentatonic
        { 6, { 0, 3, 5, 6, 7, 10 } }       // Blues
    };
    return defs[juce::jlimit (0, 10, index)];
}

inline int floorDiv (int a, int b) noexcept { return (a >= 0) ? a / b : -((-a + b - 1) / b); }

// Nota "base" (ajustada para dentro da escala) + deslocamento em graus
inline int degreeToNote (int baseNote, int degree, int root, int scaleIndex) noexcept
{
    const auto& s = scaleDef (scaleIndex);
    const int rel = baseNote - root;
    const int octave = floorDiv (rel, 12);
    const int pc = rel - octave * 12;

    int idx = 0;                                   // maior grau <= classe da nota
    for (int i = 0; i < s.size; ++i)
        if (s.steps[i] <= pc) idx = i;

    const int total = octave * s.size + idx + degree;
    const int o = floorDiv (total, s.size);
    return juce::jlimit (0, 127, root + o * 12 + s.steps[total - o * s.size]);
}

inline int quantizeToScale (int note, int root, int scaleIndex) noexcept
{
    return degreeToNote (note, 0, root, scaleIndex);
}
} // namespace mm
