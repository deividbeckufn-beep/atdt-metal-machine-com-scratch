#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

namespace mm
{
// =============================================================================
//  MidiActivity - registra atividade MIDI para os indicadores da interface.
//  Fase posterior: MIDI Learn sera implementado no modulo /MIDI.
// =============================================================================
class MidiActivity
{
public:
    void scan (const juce::MidiBuffer& midi) noexcept
    {
        for (const auto meta : midi)
        {
            const auto msg = meta.getMessage();
            if (msg.isNoteOn())
            {
                lastNote.store (msg.getNoteNumber());
                lastVelocity.store (msg.getVelocity());
                noteOnCounter.fetch_add (1);
            }
            else if (msg.isController() || msg.isPitchWheel())
            {
                controlCounter.fetch_add (1);
            }
        }
    }

    uint32_t getNoteOnCount() const noexcept  { return noteOnCounter.load(); }
    uint32_t getControlCount() const noexcept { return controlCounter.load(); }
    int getLastNote() const noexcept          { return lastNote.load(); }
    int getLastVelocity() const noexcept      { return lastVelocity.load(); }

private:
    std::atomic<uint32_t> noteOnCounter { 0 }, controlCounter { 0 };
    std::atomic<int> lastNote { -1 }, lastVelocity { 0 };
};
} // namespace mm
