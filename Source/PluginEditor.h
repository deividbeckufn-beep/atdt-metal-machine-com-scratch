#pragma once
#include "PluginProcessor.h"
#include "UI/MainView.h"

// =============================================================================
//  Editor: hospeda a MainView (1280x720 base) e aplica UI SCALE e THEME.
// =============================================================================
class MetalMachineAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MetalMachineAudioProcessorEditor (MetalMachineAudioProcessor&);
    ~MetalMachineAudioProcessorEditor() override;

    void resized() override;

private:
    void applyScale (float scale);
    void applyTheme (const juce::String& name);

    MetalMachineAudioProcessor& processor;
    mm::MetalLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };
    mm::MainView view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MetalMachineAudioProcessorEditor)
};
