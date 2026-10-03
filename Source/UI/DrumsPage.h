#pragma once
#include "Controls.h"
#include "PluginProcessor.h"

namespace mm
{
// Knob compacto (sem caixa de texto): mostra o valor num popup enquanto arrasta
class MiniKnob : public juce::Slider
{
public:
    MiniKnob (APVTS& s, const juce::String& paramID)
    {
        setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setPopupDisplayEnabled (true, false, nullptr);
        attachment = std::make_unique<APVTS::SliderAttachment> (s, paramID, *this);
        if (auto* p = s.getParameter (paramID))
        {
            setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
            setTooltip (p->getName (40));
        }
    }
private:
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// =============================================================================
//  Grade do BEAT ENGINE: 11 canais (inclui 808) x 16/32 passos visiveis (paginas ate 64)
//  Clique = liga/desliga | SHIFT+clique = acento | clique direito = ghost | ALT+clique = roll (ratchet 1-4)
//  Clique no NOME do canal = trava (o gerador nao altera canais travados)
// =============================================================================
class DrumGrid : public juce::Component, private juce::Timer
{
public:
    explicit DrumGrid (MetalMachineAudioProcessor&);
    ~DrumGrid() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void setPage (int p) { page = juce::jlimit (0, 1, p); repaint(); }
    int  getPage() const noexcept { return page; }

    static constexpr int headerWidth = 246, headerHeight = 16;

private:
    void timerCallback() override;
    int visibleSteps() const;
    bool cellAt (juce::Point<int>, int& row, int& step) const;
    juce::Rectangle<float> cellBounds (int row, int col) const;

    MetalMachineAudioProcessor& processor;
    DrumPattern& pattern;
    std::atomic<float>* length;
    juce::OwnedArray<MiniKnob> knobs;   // 4 por canal
    int page = 0;
    uint8_t paintValue = 0;
    uint32_t lastRevision = 0xffffffff, lastLocks = 0xffffffff;
    int lastStep = -2, lastLength = -1;
};

// Botao que arrasta o beat como arquivo .mid para fora do plugin (ex.: trilha do REAPER)
class DragMidiButton : public juce::TextButton
{
public:
    explicit DragMidiButton (MetalMachineAudioProcessor& p) : juce::TextButton ("DRAG MIDI"), processor (p)
    {
        setTooltip ("Arraste para uma trilha do REAPER: cria o MIDI do beat");
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 6) return;
        dragging = true;
        const auto f = processor.exportBeatMidi (false, true);
        if (f.existsAsFile())
            juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this,
                                                                         [this] { dragging = false; });
        else dragging = false;
    }
    void mouseUp (const juce::MouseEvent& e) override { juce::TextButton::mouseUp (e); }
private:
    MetalMachineAudioProcessor& processor;
    bool dragging = false;
};

class DrumsPage : public juce::Component, private juce::Timer
{
public:
    explicit DrumsPage (MetalMachineAudioProcessor&);
    ~DrumsPage() override { stopTimer(); }
    void resized() override;
    void paintOverChildren (juce::Graphics&) override;

private:
    void timerCallback() override;
    void exportMidi (bool multi);
    juce::Rectangle<int> makeItArea;

    MetalMachineAudioProcessor& processor;
    Panel beatPanel { "BEAT ENGINE" };
    // gerador
    ChoiceBox style, section, halfTime, doubleKick, bars, randMode;
    juce::TextButton varButtons[4] { juce::TextButton ("A"), juce::TextButton ("B"), juce::TextButton ("C"), juce::TextButton ("D") };
    juce::TextButton generateButton { "GENERATE BEAT" }, fillButton { "MAKE A FILL" }, randomButton { "RANDOMIZE" };
    Knob energy, density, aggression, electronic, trap, glitch, modernity, syncopation, space, complexity;
    juce::OwnedArray<juce::TextButton> makeItButtons;   // MAKE IT: MODERN, HEAVIER, MORE TRAP...
    ChoiceBox type808;
    MiniKnob drive808, glide808;
    // maquina
    juce::TextButton onButton { "ON" }, keysButton { "DRUM KEYS" }, clearButton { "CLEAR" },
                     page1Button { "1-32" }, page2Button { "33-64" },
                     exportButton { "EXPORT MIDI" }, multiButton { "MULTI-TRACK" };
    DragMidiButton dragButton;
    std::unique_ptr<APVTS::ButtonAttachment> onAttachment, keysAttachment;
    ChoiceBox playMode;
    Knob length, swing, volume;
    juce::ComboBox patternBox;
    Panel gridPanel { "PATTERN" };
    DrumGrid grid;
    uint32_t lastRevision = 0xffffffff;
};
} // namespace mm
