#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "SynthVoice.h"
#include "LfoEngine.h"
#include "Sequencer/TempoSync.h"

namespace mm
{
// =============================================================================
//  SynthEngine - vozes + LFOs globais.
//  POLY: ate 32 vozes (limite escolhido em VOICES), roubo da voz mais antiga
//        (prioridade para vozes ja soltas). Glide a partir da ultima nota.
//  MONO: 1 voz, reataca envelopes a cada nota, glide sempre.
//  LEGATO: 1 voz, notas ligadas nao reatacam (so deslizam).
//  Nas duas monofonicas, soltar uma nota volta para a anterior ainda presa.
// =============================================================================
class SynthEngine : public juce::Synthesiser
{
public:
    static constexpr int maxVoices = 32;

    void initialise (const SynthParams& p)
    {
        params = &p;
        clearVoices();
        clearSounds();
        for (int i = 0; i < maxVoices; ++i)
            addVoice (new SynthVoice (p, perf, global));
        addSound (new SynthSound());
        setNoteStealingEnabled (true);
    }

    void prepare (double newSampleRate, int maxBlockSize)
    {
        engineSampleRate = newSampleRate;
        setCurrentPlaybackSampleRate (newSampleRate);
        lfos.prepare (newSampleRate);
        for (int i = 0; i < getNumVoices(); ++i)
            if (auto* v = dynamic_cast<SynthVoice*> (getVoice (i)))
                v->prepare (newSampleRate, maxBlockSize);
        stackSize = 0;
        lastNote = -1;
    }

    // Renderiza o bloco em partes do tamanho do buffer de LFO (sem alocacao)
    // feelMult: TIME FEEL (0.5 = Half, 2 = Double) aplicado aos LFOs sincronizados
    void render (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, int numSamples,
                 const TempoSync& tempo, double feelMult = 1.0)
    {
        const double bpm = tempo.getBpm() * feelMult;
        const double beatsPerSample = bpm / 60.0 / engineSampleRate;

        for (int pos = 0; pos < numSamples; )
        {
            const int n = juce::jmin (LfoEngine::capacity, numSamples - pos);
            lfos.process (n, *params, bpm, tempo.isPlaying(), tempo.getPpq() * feelMult + pos * beatsPerSample);
            for (int k = 0; k < numLfos; ++k)
                global.lfo[k] = lfos.getBuffer (k);
            global.chunkStart = pos;

            renderNextBlock (buffer, midi, pos, n);
            pos += n;
        }
    }

    const LfoEngine& getLfos() const noexcept { return lfos; }

    // ------------------------------------------------------------- MIDI
    void handleController (int channel, int controller, int value) override
    {
        if (controller == 1)
            perf.modWheel = (float) value / 127.0f;
        juce::Synthesiser::handleController (channel, controller, value);
    }

    void handleChannelPressure (int channel, int value) override
    {
        perf.aftertouch = (float) value / 127.0f;
        juce::Synthesiser::handleChannelPressure (channel, value);
    }

    void noteOn (int channel, int note, float velocity) override
    {
        const int mode = currentMode();
        if (mode == 0)
        {
            global.glideFromNote = (float) lastNote;
            juce::Synthesiser::noteOn (channel, note, velocity);
            lastNote = note;
            return;
        }

        pushNote (note);
        lastVelocity = velocity;

        // ao trocar de POLY para MONO, as outras vozes terminam normalmente
        for (int i = 1; i < getNumVoices(); ++i)
            if (auto* other = getVoice (i); other->isVoiceActive() && ! other->isPlayingButReleased())
                stopVoice (other, 0.0f, true);

        auto* v = static_cast<SynthVoice*> (getVoice (0));
        if (v->isVoiceActive())
        {
            const bool retrigger = (mode == 1) || v->isPlayingButReleased();
            v->monoNoteOn (note, velocity, retrigger);
            v->setKeyDown (true);
        }
        else
        {
            global.glideFromNote = (mode == 1) ? (float) lastNote : -1.0f;
            startVoice (v, getSound (0).get(), channel, note, velocity);
        }
        lastNote = note;
    }

    void noteOff (int channel, int note, float velocity, bool allowTailOff) override
    {
        if (currentMode() == 0)
        {
            juce::Synthesiser::noteOff (channel, note, velocity, allowTailOff);
            return;
        }

        removeNote (note);
        auto* v = static_cast<SynthVoice*> (getVoice (0));
        if (! v->isVoiceActive())
            return;

        if (stackSize > 0)
        {
            const int top = noteStack[(size_t) stackSize - 1];
            if (v->getMonoNote() != top)
                v->monoNoteOn (top, lastVelocity, false);   // volta para a nota ainda presa
            return;
        }

        v->setKeyDown (false);
        if (! v->isSustainPedalDown() && ! v->isSostenutoPedalDown())
            stopVoice (v, velocity, allowTailOff);
    }

    void allNotesOff (int channel, bool allowTailOff) override
    {
        stackSize = 0;
        juce::Synthesiser::allNotesOff (channel, allowTailOff);
    }

protected:
    // Respeita o limite de VOICES; rouba a voz mais antiga (de preferencia ja solta)
    juce::SynthesiserVoice* findFreeVoice (juce::SynthesiserSound* sound, int, int, bool steal) const override
    {
        const int limit = juce::jmin (getNumVoices(), currentVoiceLimit());
        for (int i = 0; i < limit; ++i)
            if (auto* v = getVoice (i); ! v->isVoiceActive() && v->canPlaySound (sound))
                return v;

        if (! steal)
            return nullptr;

        juce::SynthesiserVoice* best = nullptr;
        bool bestReleased = false;
        for (int i = 0; i < limit; ++i)
        {
            auto* v = getVoice (i);
            const bool released = v->isPlayingButReleased();
            if (best == nullptr || (released && ! bestReleased)
                || (released == bestReleased && v->wasStartedBefore (*best)))
            {
                best = v;
                bestReleased = released;
            }
        }
        return best;
    }

private:
    int currentMode() const noexcept { return juce::jlimit (0, 2, (int) params->voiceMode->load()); }
    int currentVoiceLimit() const noexcept { return voiceCountValues[juce::jlimit (0, 5, (int) params->voiceCount->load())]; }

    void pushNote (int note) noexcept
    {
        removeNote (note);
        if (stackSize < (int) noteStack.size())
            noteStack[(size_t) stackSize++] = note;
    }

    void removeNote (int note) noexcept
    {
        for (int i = 0; i < stackSize; ++i)
            if (noteStack[(size_t) i] == note)
            {
                for (int j = i; j < stackSize - 1; ++j)
                    noteStack[(size_t) j] = noteStack[(size_t) j + 1];
                --stackSize;
                return;
            }
    }

    const SynthParams* params = nullptr;
    PerformanceState perf;
    GlobalModState global;
    LfoEngine lfos;
    double engineSampleRate = 44100.0;

    std::array<int, 128> noteStack {};
    int stackSize = 0;
    int lastNote = -1;
    float lastVelocity = 1.0f;
};
} // namespace mm
