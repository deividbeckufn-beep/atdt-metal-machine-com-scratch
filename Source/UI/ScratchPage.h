#pragma once
#include "Controls.h"
#include "PluginProcessor.h"
#include "ScratchPad.h"

namespace mm
{
// =============================================================================
//  SCRATCH MACHINE - pagina completa (VINYL // SAMPLE // SYNTH // FX)
// =============================================================================
class ScratchPage : public juce::Component, private juce::Timer
{
public:
    explicit ScratchPage (MetalMachineAudioProcessor&);
    ~ScratchPage() override { stopTimer(); }
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void randomScratch();
    void setReal (const char* id, float v);

    MetalMachineAudioProcessor& processor;
    Panel padPanel { "SCRATCH PAD" }, patPanel { "PATTERN" }, engPanel { "SCRATCH ENGINE" }, srcPanel { "SOURCE" },
          fxPanel { "SCRATCH FX" }, quickPanel { "QUICK SCRATCH" }, randPanel { "RANDOM SCRATCH" };
    ScratchPad pad;
    juce::TextButton onButton { "ON" }, synthMode { "SYNTH" }, sampleMode { "SAMPLE" }, loadButton { "LOAD WAV" },
                     trigButton { "TRIGGER" }, holdButton { "HOLD" }, releaseButton { "RELEASE" }, loopButton { "LOOP" },
                     randomButton { "RANDOM SCRATCH" };
    std::unique_ptr<APVTS::ButtonAttachment> onAtt, holdAtt, loopAtt;
    ChoiceBox pattern, rate, trigMode, source, direction;
    juce::OwnedArray<Knob> patKnobs, engKnobs, srcKnobs, fxKnobs;
    Knob intensity;
    juce::OwnedArray<juce::TextButton> quickButtons, lockButtons;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> headerArea;
    juce::String message;
};
} // namespace mm
