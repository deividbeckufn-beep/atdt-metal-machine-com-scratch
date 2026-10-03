#pragma once
#include "Controls.h"
#include "PluginProcessor.h"

namespace mm
{
// =============================================================================
//  Grade de passos: NOTE (grau da escala), VELOCITY, GATE, ACCENT, SLIDE, ON.
//  Clique/arraste para desenhar. Duplo clique na nota = volta para a tonica.
//  Coluna destacada = passo tocando agora (lido do motor de audio).
// =============================================================================
class StepGrid : public juce::Component, private juce::Timer
{
public:
    explicit StepGrid (MetalMachineAudioProcessor&);
    ~StepGrid() override { stopTimer(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void setPage (int p) { page = juce::jlimit (0, 1, p); lastStep = -2; repaint(); }
    int  getPage() const noexcept { return page; }

private:
    enum Row { NoteRow = 0, VelRow, GateRow, AccentRow, SlideRow, OnRow, NumRows, NoRow = -1 };
    static constexpr int labelWidth = 78;

    void timerCallback() override;
    juce::Rectangle<int> rowArea (int row) const;
    int rowAt (int y) const;
    int columnAt (int x) const;
    int visibleSteps() const;
    void applyAt (juce::Point<int>);

    MetalMachineAudioProcessor& processor;
    SequencePattern& pattern;
    std::atomic<float>* length;
    std::atomic<float>* root;
    std::atomic<float>* scale;
    std::atomic<float>* mode;

    int dragRow = NoRow;
    int page = 0;                                   // 0 = passos 1-32, 1 = passos 33-64
    int stepAt (int column) const noexcept { return juce::jlimit (0, SequencePattern::maxSteps - 1, page * 32 + column); }
    bool paintValue = true;
    uint32_t lastRevision = 0xffffffff;
    int lastStep = -2, lastLength = -1, lastRoot = -1, lastScale = -1, lastMode = -1;
};

class SeqPage : public juce::Component, private juce::Timer
{
public:
    explicit SeqPage (MetalMachineAudioProcessor&);
    ~SeqPage() override { stopTimer(); }
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    void showTempoPresets();

    MetalMachineAudioProcessor& processor;

    Panel seqPanel { "SEQUENCER" }, keyPanel { "KEY / SCALE" }, arpPanel { "ARPEGGIATOR" },
          humPanel { "GROOVE" }, tempoPanel { "TEMPO" }, gridPanel { "STEPS" };

    ChoiceBox mode, trigger, rate, feel;
    Knob length, gate, swing, transpose;
    ChoiceBox root, scale;
    Knob octave;
    juce::TextButton lockButton { "SCALE LOCK" };
    ChoiceBox arpMode;
    Knob arpOctaves;
    Knob humVel, humTime;
    ChoiceBox tempoSource;
    Knob bpm;

    juce::ComboBox patternBox;
    juce::TextButton velRandomButton { "VEL RANDOM" }, page1Button { "1-32" }, page2Button { "33-64" };
    juce::TextButton randomButton { "SMART RANDOM" }, clearButton { "CLEAR" },
                     midiOutButton { "MIDI OUT" }, tempoPresetButton { "TEMPO PRESETS" };
    std::unique_ptr<APVTS::ButtonAttachment> lockAttachment, midiOutAttachment;

    StepGrid grid;
    uint32_t lastRevision = 0xffffffff;
    juce::Rectangle<int> barArea;
};
} // namespace mm
