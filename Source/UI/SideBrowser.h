#pragma once
#include "Controls.h"
#include "PluginProcessor.h"

namespace mm
{
// =============================================================================
//  Barra lateral de presets (layout ATDT SYNTH MACHINE)
//  SEARCH | CATEGORIAS | TAGS | BROWSER (Factory/User/Reference/Favorites) | card do preset atual
//  Tudo funcional: cada item abre a lista real de presets e carrega o escolhido.
// =============================================================================
class SideBrowser : public juce::Component, private juce::Timer
{
public:
    explicit SideBrowser (MetalMachineAudioProcessor&);
    ~SideBrowser() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverItem = -1; repaint(); }

private:
    struct Item { juce::String label, kind, value; juce::Rectangle<int> bounds; };   // kind: cat | tag | src
    void timerCallback() override;
    void showList (const juce::Array<int>& indices, const juce::String& title, juce::Component* target);
    juce::Array<int> sourceList (const juce::String& src) const;

    MetalMachineAudioProcessor& processor;
    juce::TextEditor search;
    std::vector<Item> items;
    juce::Rectangle<int> catHeader, tagHeader, srcHeader, card, cardStar;
    int hoverItem = -1;
    juce::String lastName;
};
} // namespace mm
