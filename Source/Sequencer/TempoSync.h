#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

namespace mm
{
// =============================================================================
//  TempoSync - le BPM, transporte e posicao (PPQ) do REAPER a cada bloco.
//  Fase 1: apenas leitura/exibicao. Fase 3: base do Step Sequencer.
// =============================================================================
class TempoSync
{
public:
    // useInternal = TEMPO SOURCE "Internal" (ou nao ha host, ex.: app standalone)
    void update (juce::AudioPlayHead* playHead, bool useInternal, double internalBpm) noexcept
    {
        bool hostBpm = false, hostPlay = false;
        double hb = 120.0, hppq = 0.0;
        if (playHead != nullptr)
            if (auto pos = playHead->getPosition())
            {
                if (auto b = pos->getBpm())         { hb = *b; hostBpm = true; }
                if (auto p = pos->getPpqPosition()) hppq = *p;
                hostPlay = pos->getIsPlaying();
            }

        const bool internal = useInternal || ! hostBpm;
        hostHasTempo.store (hostBpm);
        hostPlaying.store (hostPlay);
        internalMode.store (internal);
        bpm.store (internal ? internalBpm : hb);
        playing.store (internal ? false : hostPlay);   // interno: relogio livre
        ppq.store (hppq);
    }

    bool isInternal() const noexcept     { return internalMode.load(); }
    bool isHostPlaying() const noexcept  { return hostPlaying.load(); }

    double getBpm() const noexcept         { return bpm.load(); }
    double getPpq() const noexcept         { return ppq.load(); }
    bool   isPlaying() const noexcept      { return playing.load(); }
    bool   hostProvidesTempo() const noexcept { return hostHasTempo.load(); }

private:
    std::atomic<double> bpm { 120.0 };
    std::atomic<double> ppq { 0.0 };
    std::atomic<bool>   playing { false };
    std::atomic<bool>   hostHasTempo { false };
    std::atomic<bool>   hostPlaying { false };
    std::atomic<bool>   internalMode { true };
};
} // namespace mm
