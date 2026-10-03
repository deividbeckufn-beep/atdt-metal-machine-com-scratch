#pragma once
#include "Controls.h"

namespace mm
{
// =============================================================================
//  Pagina FX: modulos reais do FxEngine, cada um com ON na barra de titulo.
//  Ordem do sinal (fixa nesta versao): DIST > EQ > CHORUS > DELAY > REVERB > COMP > MASTER > LIMITER
// =============================================================================
class FxModulePanel : public Panel
{
public:
    FxModulePanel (APVTS& s, const juce::String& panelTitle, const char* onParamID)
        : Panel (panelTitle)
    {
        onButton.setClickingTogglesState (true);
        onButton.setTooltip ("Liga/desliga " + panelTitle + " (com crossfade, sem clicks)");
        onAttachment = std::make_unique<APVTS::ButtonAttachment> (s, onParamID, onButton);
        onButton.onStateChange = [this] { updateBypassLook(); };
        addAndMakeVisible (onButton);
    }

    bool isOn() const { return onButton.getToggleState(); }

    // bypass = hardware "apagado": controles escurecem (continuam editaveis)
    void updateBypassLook()
    {
        const float a = onButton.getToggleState() ? 1.0f : 0.45f;
        for (auto* c : items) c->setAlpha (a);
        if (titleChoice != nullptr) titleChoice->setAlpha (a);
        if (extra != nullptr) extra->setAlpha (a);
    }

    void addKnob (Knob* k)            { knobs.add (k); items.push_back (k); addAndMakeVisible (k); updateBypassLook(); }
    void addComponent (juce::Component* c) { items.push_back (c); addAndMakeVisible (c); updateBypassLook(); }   // nao e dono
    void setTitleChoice (ChoiceBox* c) { titleChoice.reset (c); addAndMakeVisible (c); }
    void setExtraButton (juce::TextButton* b) { extra = b; addAndMakeVisible (b); updateBypassLook(); }

    void resized() override
    {
        auto bar = getTitleBar();
        onButton.setBounds (bar.removeFromRight (40).reduced (0, 1));
        if (titleChoice != nullptr)
        {
            bar.removeFromRight (6);
            titleChoice->setBounds (bar.removeFromRight (90));
        }
        auto r = getContentArea();
        if (extra != nullptr)
            extra->setBounds (r.removeFromBottom (24).withSizeKeepingCentre (110, 22));
        const int w = items.empty() ? 0 : r.getWidth() / (int) items.size();
        for (auto* c : items)
        {
            auto cell = r.removeFromLeft (w).reduced (2, 0);
            if (dynamic_cast<Knob*> (c) != nullptr) c->setBounds (cell.withSizeKeepingCentre (w - 4, juce::jmin (cell.getHeight(), 118)));
            else                                    c->setBounds (cell.withSizeKeepingCentre (w - 4, 44));
        }
    }

private:
    juce::TextButton onButton { "ON" };
    std::unique_ptr<APVTS::ButtonAttachment> onAttachment;
    juce::OwnedArray<Knob> knobs;
    std::vector<juce::Component*> items;
    std::unique_ptr<ChoiceBox> titleChoice;
    juce::TextButton* extra = nullptr;
};

class FxPage : public juce::Component, private juce::Timer
{
public:
    explicit FxPage (APVTS&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    FxModulePanel dist, sat, crush, eq, chorus, phaser, flanger, glitch, delay, reverb, comp, limiter;
    juce::Label limiterInfo;   // texto informativo do LIMITER (o painel so guarda o ponteiro)
    juce::TextButton pingButton { "PING-PONG" };
    std::unique_ptr<APVTS::ButtonAttachment> pingAttachment;
    juce::Rectangle<int> chainArea;
    void timerCallback() override;
    uint32_t lastOnMask = 0xffffffff;
    std::unique_ptr<ChoiceBox> glitchDiv;
};
} // namespace mm
