#include "PluginEditor.h"

MetalMachineAudioProcessorEditor::MetalMachineAudioProcessorEditor (MetalMachineAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), view (p)
{
    lookAndFeel.setTheme (mm::Theme::byName (processor.getThemeName()));
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (view);

    view.header.onScaleChosen = [this] (float s) { applyScale (s); };
    view.header.onThemeChosen = [this] (const juce::String& n) { applyTheme (n); };

    applyScale (processor.getUiScale());
}

MetalMachineAudioProcessorEditor::~MetalMachineAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void MetalMachineAudioProcessorEditor::applyScale (float scale)
{
    scale = juce::jlimit (0.5f, 2.0f, scale);
    processor.setUiScale (scale);
    view.setTransform (juce::AffineTransform::scale (scale));
    setSize (juce::roundToInt (mm::MainView::baseWidth * scale), juce::roundToInt (mm::MainView::baseHeight * scale));
}

void MetalMachineAudioProcessorEditor::applyTheme (const juce::String& name)
{
    processor.setThemeName (name);
    lookAndFeel.setTheme (mm::Theme::byName (name));
    sendLookAndFeelChange();
    repaint();
}

void MetalMachineAudioProcessorEditor::resized()
{
    view.setBounds (0, 0, mm::MainView::baseWidth, mm::MainView::baseHeight);
}
