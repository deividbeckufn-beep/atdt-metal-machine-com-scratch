#pragma once
#include "Controls.h"
#include "PluginProcessor.h"

namespace mm
{
// Analise numa thread propria (nunca no audio thread), com cache em disco
class ReferenceJob : public juce::Thread
{
public:
    explicit ReferenceJob (juce::File f) : juce::Thread ("ATDT Reference Analysis"), file (std::move (f)) {}
    ~ReferenceJob() override { stopThread (10000); }
    void run() override;
    static juce::File cacheFileFor (const juce::File& audio);
    juce::File file;
    std::atomic<float> progress { 0.0f };
    std::atomic<bool> done { false };
    bool fromCache = false;
    ReferenceResult result;
};

class RefLabPage : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit RefLabPage (MetalMachineAudioProcessor&);
    ~RefLabPage() override;
    void paintOverChildren (juce::Graphics&) override;   // por cima dos paineis
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
private:
    void timerCallback() override;
    void startAnalysis (const juce::File&);
    void drawBars (juce::Graphics&, juce::Rectangle<int>, const juce::StringArray&, const std::vector<float>&, bool percent);

    MetalMachineAudioProcessor& processor;
    Panel mainPanel { "REFERENCE LAB" }, analysisPanel { "FULL MIX ANALYSIS" }, dnaPanel { "REFERENCE DNA" };
    juce::TextButton importButton { "IMPORT" }, keyButton { "APPLY KEY" }, grooveButton { "GENERATE ORIGINAL GROOVE" },
                     songButton { "GENERATE ORIGINAL SONG" }, presetButton { "CREATE PRESET" };
    juce::ComboBox presetTarget;
    std::unique_ptr<ReferenceJob> job;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> dropArea, readArea, timelineArea, spectrumArea, chromaArea, beatArea, soundArea;
    juce::String status { "Arraste uma musica (WAV, AIFF, FLAC, MP3) ou clique em IMPORT" };
    bool dragOver = false;
};
} // namespace mm
