#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "MetalLookAndFeel.h"
#include "Parameters/Parameters.h"

namespace mm
{
using APVTS = juce::AudioProcessorValueTreeState;

// -----------------------------------------------------------------------------
// Base: vigia parametros (30x/s) e so redesenha quando algo muda -> CPU minima
// -----------------------------------------------------------------------------
class WatchingDisplay : public juce::Component, private juce::Timer
{
public:
    WatchingDisplay() { startTimerHz (30); }
    ~WatchingDisplay() override { stopTimer(); }

protected:
    using Snapshot = std::array<float, 8>;
    virtual void takeSnapshot (Snapshot&) const = 0;
    void paintBackground (juce::Graphics&, juce::Rectangle<float>, int verticalLines, int horizontalLines) const;

private:
    void timerCallback() override
    {
        Snapshot s {};
        takeSnapshot (s);
        if (s != last) { last = s; repaint(); }
    }
    Snapshot last { { -1e9f } };
};

// ---- Forma de onda do oscilador (reflete WAVE, PHASE, LEVEL, OCT) ----
class WaveformDisplay : public WatchingDisplay
{
public:
    WaveformDisplay (APVTS& state, int oscIndex);
    void paint (juce::Graphics&) override;

private:
    void takeSnapshot (Snapshot&) const override;
    std::atomic<float>* wave; std::atomic<float>* phase; std::atomic<float>* level; std::atomic<float>* octave;
    std::atomic<float>* wt; std::atomic<float>* wtpos; std::atomic<float>* fmratio; std::atomic<float>* fmamt;
};

// ---- Curva de resposta do filtro (mesma matematica do DSP) ----
class FilterDisplay : public WatchingDisplay
{
public:
    explicit FilterDisplay (APVTS& state);
    void paint (juce::Graphics&) override;

private:
    void takeSnapshot (Snapshot&) const override;
    std::atomic<float>* type; std::atomic<float>* cutoff; std::atomic<float>* resonance;
    std::atomic<float>* drive; std::atomic<float>* mix;
};

// ---- Envelope ADSR com pontos arrastaveis (altera os parametros reais) ----
class EnvelopeDisplay : public WatchingDisplay
{
public:
    EnvelopeDisplay (APVTS& state, const juce::String& attackID, const juce::String& decayID,
                     const juce::String& sustainID, const juce::String& releaseID);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    struct Geometry { juce::Rectangle<float> area; float segW, holdW; juce::Point<float> p0, attack, decay, sustainEnd, release; };
    Geometry getGeometry() const;
    int hitTestHandle (juce::Point<float>) const;
    void takeSnapshot (Snapshot&) const override;
    static void setNormalised (juce::RangedAudioParameter*, float);

    juce::RangedAudioParameter* a; juce::RangedAudioParameter* d;
    juce::RangedAudioParameter* s; juce::RangedAudioParameter* r;
    int dragging = -1, hover = -1;
};
} // namespace mm
