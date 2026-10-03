#pragma once
#include "Controls.h"
#include "PluginProcessor.h"
#include "DrumsPage.h"   // DragMidiButton

namespace mm
{
// Linha do tempo: secoes (largura = compassos, barra = energia) + 12 trilhas em mini piano-roll.
// Clique numa secao = PREVIEW (carrega bateria/808 e o riff no motor; de play no REAPER).
class SongTimeline : public juce::Component
{
public:
    explicit SongTimeline (MetalMachineAudioProcessor& p) : processor (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    int selected = -1;
    std::function<void (int)> onSelect;
private:
    MetalMachineAudioProcessor& processor;
    static constexpr int labelW = 92, headerH = 46;
};

// botao que arrasta a MUSICA inteira (.mid multitrilha) para o REAPER
class DragSongButton : public juce::TextButton
{
public:
    explicit DragSongButton (MetalMachineAudioProcessor& p) : juce::TextButton ("DRAG SONG MIDI"), processor (p) {}
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 6) return;
        dragging = true;
        const auto f = processor.exportSongMidi (true);
        if (f.existsAsFile())
            juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this, [this] { dragging = false; });
        else dragging = false;
    }
private:
    MetalMachineAudioProcessor& processor;
    bool dragging = false;
};

class SongPage : public juce::Component, private juce::Timer
{
public:
    explicit SongPage (MetalMachineAudioProcessor&);
    ~SongPage() override { stopTimer(); }
    void resized() override;
    void paintOverChildren (juce::Graphics&) override;
private:
    void timerCallback() override;
    void refresh();
    MetalMachineAudioProcessor& processor;
    Panel dnaPanel { "SONG GENERATOR" }, arrangePanel { "ARRANGEMENT" };
    ChoiceBox style, mood, structure;
    Knob heaviness, electronic, trap, darkness, melodic, modernity, texture, fx;
    juce::TextButton vocalButton { "VOCAL SPACE" }, quickButton { "QUICK IDEA" },
                     generateButton { "GENERATE EVERYTHING" }, regenButton { "REGENERATE" }, seedButton { "NEW SEED" },
                     inspireButton { "INSPIRE ME" },
                     exportButton { "EXPORT SONG MIDI" };
    DragSongButton dragButton;
    std::unique_ptr<APVTS::ButtonAttachment> vocalAtt, quickAtt;
    juce::OwnedArray<juce::TextButton> lockButtons;
    SongTimeline timeline;
    juce::Rectangle<int> seedArea, lockArea, infoArea;
    juce::String info;
};
} // namespace mm
