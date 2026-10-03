#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "SequencePattern.h"
#include "Scale.h"

namespace mm
{
struct SeqParams
{
    std::atomic<float>* mode = nullptr;      // 0 Off, 1 Sequence, 2 Arp
    std::atomic<float>* trigger = nullptr;   // 0 Key, 1 Latch, 2 Transport
    std::atomic<float>* rate = nullptr;
    std::atomic<float>* feel = nullptr;      // 0 Half, 1 Normal, 2 Double
    std::atomic<float>* length = nullptr;
    std::atomic<float>* gate = nullptr;
    std::atomic<float>* root = nullptr;
    std::atomic<float>* scale = nullptr;
    std::atomic<float>* octave = nullptr;
    std::atomic<float>* scaleLock = nullptr;
    std::atomic<float>* arpMode = nullptr;   // Up, Down, Up/Down, Random, Order, Chord
    std::atomic<float>* arpOctaves = nullptr;
    std::atomic<float>* humVel = nullptr;
    std::atomic<float>* humTime = nullptr;
    std::atomic<float>* swing = nullptr;       // 0..1: atrasa os passos pares (100% = tercina)
    std::atomic<float>* transpose = nullptr;   // semitons (-24..+24)

    void attach (juce::AudioProcessorValueTreeState&);
};

// Multiplicador de tempo do TIME FEEL (usado tambem pelos LFOs)
inline double feelMultiplier (int feel) noexcept { return feel == 0 ? 0.5 : feel == 2 ? 2.0 : 1.0; }

// =============================================================================
//  StepSequencer / Arpeggiator - gerador de MIDI.
//  Entrada: MIDI do teclado. Saida: MIDI para o synth (e para a saida MIDI).
//  Relogio: posicao (PPQ) do REAPER quando tocando; relogio interno quando parado
//  (reinicia na primeira tecla). Sem alocacao no audio thread.
// =============================================================================
class StepSequencer
{
public:
    StepSequencer (const SeqParams& p, const SequencePattern& pat) : params (p), pattern (pat) {}

    void prepare (double sampleRate);
    void process (const juce::MidiBuffer& in, juce::MidiBuffer& out, int numSamples,
                  double bpm, bool hostPlaying, double hostPpq);

    int getCurrentStep() const noexcept { return uiStep.load(); }

private:
    static constexpr int maxPending = 64;
    struct Pending { juce::int64 time = 0, off = 0; int note = 0; float vel = 0.0f; bool tie = false; bool used = false; };

    int  handleInput (const juce::MidiMessage&, int samplePos, juce::MidiBuffer& out, bool useHost, double bps);
    void trigger (juce::int64 k, int sample, double stepSamples, bool transportActive);
    void schedule (int note, float vel, juce::int64 on, juce::int64 off, bool tie);
    void emitDue (juce::MidiBuffer& out, int numSamples);
    void allNotesOff (juce::MidiBuffer& out, int sample);
    int  activeCount() const noexcept;

    const SeqParams& params;
    const SequencePattern& pattern;
    double sampleRate = 44100.0;

    juce::int64 blockStart = 0;
    double internalPpq = 0.0;               // PPQ interno no inicio do bloco
    std::array<juce::int64, 128> offTime {};
    std::array<Pending, maxPending> pending {};

    std::array<int, 16> held {};  int numHeld = 0;      // ordem em que foram pressionadas
    std::array<int, 16> latched {}; int numLatched = 0;
    int lastPressed = -1;
    int arpIndex = 0;
    bool prevSlide = false; int prevNote = -1;
    int lastMode = -1;
    bool wasTransportActive = false;
    juce::Random random;
    std::atomic<int> uiStep { -1 };
};
} // namespace mm
