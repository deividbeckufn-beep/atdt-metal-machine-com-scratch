#pragma once
#include "Controls.h"
#include "Displays.h"
#include "PluginProcessor.h"

namespace mm
{
// ---- display do LFO: forma real + cursor da fase atual (lida do motor) ----
class LfoDisplay : public juce::Component, private juce::Timer
{
public:
    LfoDisplay (APVTS&, const LfoEngine&, int lfoIndex);
    ~LfoDisplay() override { stopTimer(); }
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    const LfoEngine& engine;
    int index;
    std::atomic<float>* shape;
    std::atomic<float>* amount;
};

class LfoPanel : public Panel, private juce::Timer
{
public:
    LfoPanel (MetalMachineAudioProcessor&, int lfoIndex);
    ~LfoPanel() override { stopTimer(); }
    void resized() override;

private:
    void timerCallback() override;

    APVTS& state;
    int index;
    ModSourceChip chip;
    ChoiceBox shape;
    LfoDisplay display;
    juce::TextButton syncButton { "SYNC" };
    std::unique_ptr<APVTS::ButtonAttachment> syncAttachment;
    Knob rate;
    ChoiceBox division;
    Knob phase, fade, amount, smooth;
    std::atomic<float>* syncParam;
};

class MacroPanel : public Panel, private juce::Timer
{
public:
    explicit MacroPanel (MetalMachineAudioProcessor&);
    ~MacroPanel() override { stopTimer(); }
    void resized() override;

    std::function<void (bool)> onEditMacros;   // liga/desliga filtro "so macros" na matriz

private:
    void timerCallback() override;

    MetalMachineAudioProcessor& processor;
    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<juce::Label> names;
    juce::OwnedArray<ModSourceChip> chips;
    juce::TextButton editButton { "EDIT MACRO" };
};

class ModMatrixPanel : public Panel, private juce::Timer
{
public:
    explicit ModMatrixPanel (APVTS&);
    ~ModMatrixPanel() override { stopTimer(); }
    void resized() override;
    void paint (juce::Graphics&) override;
    void setMacroFilter (bool onlyMacros);

private:
    struct Row : public juce::Component
    {
        Row (APVTS&, int slot);
        void resized() override;
        void paint (juce::Graphics&) override;
        int slot;
        juce::ComboBox source, dest, curve, mode;
        juce::Slider amount;
        juce::TextButton clear { "X" };
        std::unique_ptr<APVTS::ComboBoxAttachment> srcA, dstA, curveA, modeA;
        std::unique_ptr<APVTS::SliderAttachment> amtA;
    };

    void timerCallback() override;
    void updateRows();
    bool isUsed (int slot) const;

    APVTS& state;
    juce::OwnedArray<Row> rows;
    std::array<bool, numModSlots> revealed {};
    juce::TextButton addButton { "+ ADD MODULATION" };
    bool macroFilter = false;
    float lastHash = -1.0f;
};

// ENV 3: envelope de modulacao livre (fonte "Mod Env" na matriz)
class ModEnvPanel : public Panel
{
public:
    explicit ModEnvPanel (APVTS&);
    void resized() override;

private:
    ModSourceChip chip;
    EnvelopeDisplay display;
    Knob attack, decay, sustain, release;
};

class ModPage : public juce::Component
{
public:
    explicit ModPage (MetalMachineAudioProcessor&);
    void resized() override;

private:
    juce::OwnedArray<LfoPanel> lfos;
    MacroPanel macros;
    ModEnvPanel modEnv;
    ModMatrixPanel matrix;
};
} // namespace mm
