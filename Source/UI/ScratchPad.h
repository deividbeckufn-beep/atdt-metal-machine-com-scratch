#pragma once
#include "Controls.h"
#include "PluginProcessor.h"

namespace mm
{
// =============================================================================
//  SCRATCH PAD - disco de vinil + forma de onda (hibrido)
//  * o disco gira com a VELOCIDADE REAL do prato no audio (acelera, freia, inverte, para)
//  * arrastar o mouse move o prato: no disco (circular) ou na forma de onda (horizontal)
//  * forma de onda: regiao START/END, CENTER, playhead; zoom na rodinha do mouse
// =============================================================================
class ScratchPad : public juce::Component, private juce::Timer
{
public:
    explicit ScratchPad (MetalMachineAudioProcessor&);
    ~ScratchPad() override { stopTimer(); processor.getScratch().padActive = false; }
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    void timerCallback() override;
    float regionNorm (float sourceNorm) const;
    MetalMachineAudioProcessor& processor;
    juce::Rectangle<int> discArea, waveArea;
    float angle = 0.0f, target = 0.0f, lastAngleMouse = 0.0f, zoom = 1.0f, zoomCentre = 0.5f;
    bool draggingDisc = false;
};
} // namespace mm
