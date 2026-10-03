#include "SequencePattern.h"

// =============================================================================
//  Biblioteca de sequencias - escritas em GRAUS da escala.
//  Notacao por passo:  0 = tonica, 2 = terca, 4 = quinta, 7 = oitava, -7 = oitava abaixo
//    a = accent   s = slide   l = nota longa   h = nota curta   . = mute
//  Rates: 0=1/4 1=1/8 2=1/16 3=1/32 4=1/8T 5=1/16T   Feel: 0=Half 1=Normal 2=Double
//  Nada e amarrado a BPM: o padrao segue o andamento do REAPER.
// =============================================================================
namespace mm
{
const std::vector<PatternDef>& getPatternLibrary()
{
    static const std::vector<PatternDef> lib
    {
        { "Init 16ths",         "INIT",      "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0", 2, 1 },

        { "Dark Pulse",         "INTRO",     "0 . . 0 . . 0 . 0 . . 0 . . 2 .", 2, 1 },
        { "Cinematic Steps",    "INTRO",     "0l . . . 2l . . . 4l . . . 5l . . .", 2, 1 },

        { "Verse Chug",         "VERSE",     "0a 0 . 0 0a . 0 . 0a 0 . 0 1s 0 . 0", 2, 1 },
        { "Offbeat Djent",      "VERSE",     "0a . . 0a . . 0a . . 0a . 0 . 0a . .", 2, 1 },
        { "Cyber Gate",         "VERSE",     "0 0 7 0 0 5 0 3 0 0 7 0 5 3 2 0", 2, 1 },

        { "Build 8ths to 16ths","BUILDUP",   "0 . 0 . 0 . 0 . 2 . 2 . 3 . 4 . 0 0 0 0 2 2 2 2 3 3 3 3 4a 4a 5a 7a", 2, 1 },
        { "Pitch Climb",        "BUILDUP",   "0 1 2 3 4 5 6 7 7 8 9 10 11a 12a 13a 14a", 2, 1 },

        { "Anthem Octaves",     "CHORUS",    "0l . 7 . 0 . 7 . -2l . 5 . -2 . 5 .", 2, 1 },
        { "Wide Chorus Arp",    "CHORUS",    "0 2 4 7 4 2 0 2 -3 -1 1 4 1 -1 -3 -1", 2, 1 },

        { "Breakdown Stabs",    "BREAKDOWN", "0ah . . . . . 0ah . . . 0ah . . . . .", 2, 1 },
        { "Chug Gate",          "BREAKDOWN", "0a 0 0 . 0a 0 . 0 0a . 0 0 0a 0 . .", 2, 1 },
        { "Half-Time Slam",     "BREAKDOWN", "0al . . . . . . . 0a . . 0a . . . .", 2, 0 },
        { "Drop Slide",         "BREAKDOWN", "0a . . . 0a . . . 0as -7l . . . . . .", 2, 1 },

        { "Floating Bridge",    "BRIDGE",    "4l . . 2 . . 0l . . . 2 . 4 . 5 .", 2, 1 },
        { "Syncopated Bridge",  "BRIDGE",    "0 . 3 . . 2 . 0 . 5 . . 4 . 2 .", 2, 1 },

        { "Fading Pulse",       "OUTRO",     "0 . . . 0h . . . 0 . . . . . . .", 2, 1 },
    };
    return lib;
}

int findPattern (const juce::String& name)
{
    const auto& lib = getPatternLibrary();
    for (int i = 0; i < (int) lib.size(); ++i)
        if (name == lib[(size_t) i].name)
            return i;
    return -1;
}

int applyPattern (SequencePattern& p, const PatternDef& def)
{
    p.setInit();
    const auto tokens = juce::StringArray::fromTokens (def.steps, " ", {});
    int n = 0;
    for (auto& raw : tokens)
    {
        auto tok = raw.trim();
        if (tok.isEmpty() || n >= SequencePattern::maxSteps)
            continue;
        auto& s = p.steps[(size_t) n++];
        if (tok == ".")
        {
            s.on = false;
            continue;
        }
        const auto digits = tok.initialSectionContainingOnly ("-0123456789");
        const auto flags = tok.substring (digits.length());
        s.degree = juce::jlimit (SequencePattern::minDegree, SequencePattern::maxDegree, digits.getIntValue());
        s.accent = flags.containsChar ('a');
        s.slide  = flags.containsChar ('s');
        s.velocity = s.accent.load() ? 1.0f : 0.8f;
        s.gate = flags.containsChar ('l') ? 0.95f : flags.containsChar ('h') ? 0.25f : 0.5f;
    }
    p.name = def.name;
    p.touch();
    return juce::jmax (1, n);
}

// SMART RANDOM: so graus da escala, acentos nos tempos fortes, tonica predominante
void randomizePattern (SequencePattern& p, int length, juce::Random& r)
{
    static const int degrees[] { 0, 0, 0, 0, 2, 4, 7, -3, 5, 1, 3 };
    for (int i = 0; i < SequencePattern::maxSteps; ++i)
    {
        auto& s = p.steps[(size_t) i];
        const bool strong = (i % 4) == 0;
        s.on = strong || r.nextFloat() < 0.6f;
        s.degree = (strong && r.nextFloat() < 0.6f) ? 0 : degrees[r.nextInt ((int) std::size (degrees))];
        s.accent = strong && r.nextFloat() < 0.5f;
        s.velocity = s.accent.load() ? 1.0f : 0.6f + 0.3f * r.nextFloat();
        s.gate = 0.25f + 0.6f * r.nextFloat();
        s.slide = ! strong && r.nextFloat() < 0.1f;
    }
    juce::ignoreUnused (length);
    p.name = "Random";
    p.touch();
}
} // namespace mm
