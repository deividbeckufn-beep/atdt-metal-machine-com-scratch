#pragma once
#include "Controls.h"
#include "Displays.h"

namespace mm
{
// Pagina principal de sintese. Todos os controles estao ligados ao DSP real.
class SynthPage : public juce::Component
{
public:
    explicit SynthPage (APVTS& state);
    void resized() override;

private:
    struct OscPanel : Panel, private juce::Timer
    {
        OscPanel (APVTS&, int index);
        ~OscPanel() override { stopTimer(); }
        void resized() override;
        ChoiceBox wave; WaveformDisplay display;
        Knob oct, semi, fine, phase, level, pan;
        // so aparecem com WAVE = Wavetable (tabela + posicao) ou FM (razao + quantidade)
        ChoiceBox table, ratio;
        Knob wtPos, fmAmt;

    private:
        void timerCallback() override;
        std::atomic<float>* waveParam;
        int lastWave = -1;
    };

    struct SubPanel : Panel
    {
        explicit SubPanel (APVTS&);
        void resized() override;
        ChoiceBox wave; Knob octave, level;
    };

    struct NoisePanel : Panel
    {
        explicit NoisePanel (APVTS&);
        void resized() override;
        ChoiceBox type; Knob level;
    };

    struct FilterPanel : Panel
    {
        explicit FilterPanel (APVTS&);
        void resized() override;
        ChoiceBox type; FilterDisplay display;
        Knob cutoff, reso, drive, keytrack, mix;
    };

    struct EnvPanel : Panel
    {
        EnvPanel (APVTS&, const juce::String& panelTitle, const juce::String& a, const juce::String& d,
                  const juce::String& s, const juce::String& r, const juce::String& amountID);
        void resized() override;
        EnvelopeDisplay display;
        Knob attack, decay, sustain, release;
        std::unique_ptr<Knob> amount;
    };

    struct VoicePanel : Panel
    {
        explicit VoicePanel (APVTS&);
        void resized() override;
        ChoiceBox mode, voices;
        Knob glide, unison, detune, spread;
    };

    struct MasterPanel : Panel
    {
        explicit MasterPanel (APVTS&);
        void resized() override;
        Knob gain, bend, bassMono;
    };

    std::vector<std::unique_ptr<OscPanel>> oscs;
    SubPanel sub;
    NoisePanel noise;
    FilterPanel filter;
    EnvPanel filterEnv, ampEnv;
    VoicePanel voice;
    MasterPanel master;
};
} // namespace mm
