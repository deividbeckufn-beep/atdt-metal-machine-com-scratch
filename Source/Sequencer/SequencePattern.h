#pragma once
#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <atomic>
#include <vector>

namespace mm
{
// =============================================================================
//  Padrao do step sequencer (ate 64 passos).
//  Campos atomicos: a interface edita e o audio thread le sem travas.
//  Salvo no projeto do REAPER e nos presets.
// =============================================================================
struct SeqStep
{
    std::atomic<int>   degree   { 0 };      // grau da escala (-7..+14)
    std::atomic<float> velocity { 0.8f };   // 0..1
    std::atomic<float> gate     { 0.5f };   // fracao do passo (0.05..1)
    std::atomic<bool>  on       { true };   // false = MUTE
    std::atomic<bool>  accent   { false };
    std::atomic<bool>  slide    { false };
};

class SequencePattern
{
public:
    static constexpr int maxSteps = 64;
    static constexpr int minDegree = -7, maxDegree = 14;

    std::array<SeqStep, maxSteps> steps;
    std::atomic<uint32_t> revision { 0 };

    void touch() noexcept { revision.fetch_add (1); }

    void setInit()
    {
        for (auto& s : steps)
        {
            s.degree = 0; s.velocity = 0.8f; s.gate = 0.5f;
            s.on = true; s.accent = false; s.slide = false;
        }
        name = "Init";
        touch();
    }

    juce::ValueTree toValueTree() const
    {
        juce::ValueTree t ("SEQUENCE");
        t.setProperty ("name", name, nullptr);
        for (int i = 0; i < maxSteps; ++i)
        {
            const auto& s = steps[(size_t) i];
            t.appendChild (juce::ValueTree ("STEP", { { "i", i }, { "deg", s.degree.load() },
                                                      { "vel", s.velocity.load() }, { "gate", s.gate.load() },
                                                      { "on", s.on.load() }, { "acc", s.accent.load() },
                                                      { "sld", s.slide.load() } }), nullptr);
        }
        return t;
    }

    void fromValueTree (const juce::ValueTree& t)
    {
        setInit();
        if (! t.isValid())
            return;
        name = t.getProperty ("name", "Init").toString();
        for (const auto& c : t)
        {
            const int i = c.getProperty ("i", -1);
            if (i < 0 || i >= maxSteps)
                continue;
            auto& s = steps[(size_t) i];
            s.degree   = juce::jlimit (minDegree, maxDegree, (int) c.getProperty ("deg", 0));
            s.velocity = juce::jlimit (0.0f, 1.0f, (float) c.getProperty ("vel", 0.8f));
            s.gate     = juce::jlimit (0.05f, 1.0f, (float) c.getProperty ("gate", 0.5f));
            s.on       = (bool) c.getProperty ("on", true);
            s.accent   = (bool) c.getProperty ("acc", false);
            s.slide    = (bool) c.getProperty ("sld", false);
        }
        touch();
    }

    juce::String name { "Init" };   // so message thread
};

// ---- biblioteca de padroes (PatternLibrary.cpp) ----
struct PatternDef
{
    const char* name;
    const char* section;   // INTRO, VERSE, BUILDUP, CHORUS, BREAKDOWN, BRIDGE, OUTRO
    const char* steps;     // tokens: grau + sufixos (a=accent s=slide l=longo h=curto) ou "." = mute
    int rate;              // indice em seqRateNames
    int feel;              // 0 Half, 1 Normal, 2 Double
};

const std::vector<PatternDef>& getPatternLibrary();
int  findPattern (const juce::String& name);                  // -1 se nao existir
int  applyPattern (SequencePattern&, const PatternDef&);      // retorna o numero de passos
void randomizePattern (SequencePattern&, int length, juce::Random&);
} // namespace mm
