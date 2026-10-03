#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

namespace mm
{
// =============================================================================
//  MidiLearn - liga qualquer CC do teclado/controlador a um parametro.
//  Uso: clique direito no knob -> "MIDI Learn" -> mexa um knob/fader fisico.
//  Thread-safe: a interface so escreve atomics; o audio thread aplica os CCs.
//  Os mapeamentos sao salvos dentro do projeto do REAPER.
// =============================================================================
class MidiLearn
{
public:
    static constexpr int numCCs = 128;

    MidiLearn() { for (auto& m : ccToParam) m.store (-1); }

    void setParameters (const juce::Array<juce::AudioProcessorParameter*>& p) { params = p; }

    // ---- interface (message thread) ----
    void startLearning (int paramIndex) noexcept { learnTarget.store (paramIndex); }
    void cancelLearning() noexcept               { learnTarget.store (-1); }
    bool isLearning (int paramIndex) const noexcept { return paramIndex >= 0 && learnTarget.load() == paramIndex; }

    int getCCFor (int paramIndex) const noexcept
    {
        for (int cc = 0; cc < numCCs; ++cc)
            if (ccToParam[(size_t) cc].load() == paramIndex)
                return cc;
        return -1;
    }

    void clearMapping (int paramIndex) noexcept
    {
        for (auto& m : ccToParam)
            if (m.load() == paramIndex)
                m.store (-1);
    }

    // ---- audio thread ----
    void process (const juce::MidiBuffer& midi) noexcept
    {
        for (const auto meta : midi)
        {
            const auto msg = meta.getMessage();
            if (! msg.isController())
                continue;

            const int cc = msg.getControllerNumber();
            if (cc == 64 || cc >= 120)          // sustain e mensagens de modo nunca sao mapeados
                continue;

            int target = learnTarget.load();
            if (target >= 0 && learnTarget.compare_exchange_strong (target, -1))
            {
                clearMapping (target);
                ccToParam[(size_t) cc].store (target);
            }

            const int p = ccToParam[(size_t) cc].load();
            if (p >= 0 && p < params.size())
                params.getUnchecked (p)->setValueNotifyingHost ((float) msg.getControllerValue() / 127.0f);
        }
    }

    // ---- salvar / carregar ----
    juce::ValueTree toValueTree() const
    {
        juce::ValueTree t ("MIDI_LEARN");
        for (int cc = 0; cc < numCCs; ++cc)
            if (const int p = ccToParam[(size_t) cc].load(); p >= 0 && p < params.size())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (params.getUnchecked (p)))
                    t.appendChild (juce::ValueTree ("MAP", { { "cc", cc }, { "param", rp->getParameterID() } }), nullptr);
        return t;
    }

    void fromValueTree (const juce::ValueTree& t)
    {
        for (auto& m : ccToParam) m.store (-1);
        for (const auto& child : t)
        {
            const int cc = child.getProperty ("cc", -1);
            const auto id = child.getProperty ("param").toString();
            if (cc < 0 || cc >= numCCs)
                continue;
            for (int i = 0; i < params.size(); ++i)
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (params.getUnchecked (i)))
                    if (rp->getParameterID() == id)
                        ccToParam[(size_t) cc].store (i);
        }
    }

private:
    std::array<std::atomic<int>, numCCs> ccToParam;
    std::atomic<int> learnTarget { -1 };
    juce::Array<juce::AudioProcessorParameter*> params;
};

// Interface para que os knobs encontrem o MidiLearn sem depender do Processor
struct MidiLearnProvider
{
    virtual ~MidiLearnProvider() = default;
    virtual MidiLearn& getMidiLearn() = 0;
};
} // namespace mm
