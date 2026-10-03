#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters/Parameters.h"
#include "Synth/SynthEngine.h"
#include "Sequencer/TempoSync.h"
#include "Effects/FxEngine.h"
#include "Drums/DrumEngine.h"
#include "Drums/BeatGenerator.h"
#include "Drums/BeatMidi.h"
#include "Song/SongGenerator.h"
#include "Reference/ReferenceAnalyzer.h"
#include "Scratch/ScratchEngine.h"
#include "MIDI/MidiActivity.h"
#include "MIDI/MidiLearn.h"
#include "DSP/StateVariableFilter.h"
#include "Presets/PresetManager.h"
#include "Sequencer/StepSequencer.h"

// =============================================================================
//  ATDT SYNTH MACHINE - Processor
//  MIDI do REAPER -> SynthEngine -> Master -> Saida. Salva/restaura estado.
// =============================================================================
class MetalMachineAudioProcessor : public juce::AudioProcessor,
                                   public mm::MidiLearnProvider
{
public:
    MetalMachineAudioProcessor();
    ~MetalMachineAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }   // saida MIDI opcional (MIDI OUT)
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }   // reverb/delay: o render offline nao corta a cauda

    // Presets de fabrica expostos como "programs" -> aparecem no menu de presets do REAPER
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Botao INIT: todos os parametros voltam ao padrao (com notificacao ao REAPER)
    void resetToInitPatch();

    // Preferencias de interface salvas junto com o projeto
    float getUiScale() const;
    void  setUiScale (float scale);
    juce::String getThemeName() const;
    void  setThemeName (const juce::String& name);

    mm::MidiLearn& getMidiLearn() override { return midiLearn; }

    mm::PresetManager& getPresetManager() noexcept { return *presetManager; }
    const mm::LfoEngine& getLfos() const noexcept   { return synth.getLfos(); }

    // Nomes das macros (salvos no projeto e nos presets)
    juce::String getMacroName (int index) const;
    void setMacroName (int index, const juce::String& name);

    // ---- sequencer (chamados pela interface) ----
    mm::SequencePattern& getPattern() noexcept             { return pattern; }
    int  getSequencerStep() const noexcept                 { return sequencer.getCurrentStep(); }
    void loadLibraryPattern (int index);

    // ---- bateria (chamados pela interface) ----
    mm::DrumPattern& getDrumPattern() noexcept { return drumPattern; }
    int  getDrumStep() const noexcept          { return drums.getCurrentStep(); }
    void loadDrumPattern (int libraryIndex);
    void clearDrums();

    // ---- BEAT ENGINE (message thread) ----
    void generateBeat (bool newSeed);           // GENERATE BEAT (nova seed) / variacoes A-D (mesma seed)
    void makeBeatFill();
    void randomizeBeat();                       // SUBTLE / MEDIUM / EXTREME, respeita as travas
    void applyBeatTransform (int kind);         // MAKE IT: MODERN, HEAVIER, MORE TRAP, ELECTRONIC, DARKER, BIGGER, TIGHTER, GROOVIER
    uint32_t getBeatLocks() const;
    void setBeatLocks (uint32_t mask);
    juce::File exportBeatMidi (bool multiTrack, bool toTempFolder);   // arquivo .mid real (ou File{} se falhar)

    // ---- SONG GENERATOR (message thread) ----
    void generateSong (bool newSeed);                 // GENERATE EVERYTHING / REGENERATE (mesma seed)
    const mm::SongData& getSong() const noexcept { return song; }
    int  getSongSeed() const;
    uint32_t getSongLocks() const;
    void setSongLocks (uint32_t mask);
    void previewSongSection (int index);              // carrega bateria + riff da secao no motor (play no REAPER)
    juce::File exportSongMidi (bool toTempFolder);
    juce::String inspireMe();   // PRODUCER MODE: estilo + clima + KEY + synth + beat + musica coerentes (retorna o resumo)

    // ---- REFERENCE LAB (message thread; a analise roda numa thread propria na interface) ----
    void setReference (const mm::ReferenceResult& r);       // guarda (e salva no projeto)
    const mm::ReferenceResult& getReference() const noexcept { return reference; }
    void applyReferenceKey();
    void generateGrooveFromReference();                    // Beat DNA -> groove ORIGINAL
    void generateSongFromReference();                      // DNA -> musica ORIGINAL
    bool createPresetFromReference (int target, const juce::String& name);   // 0 BASS, 1 LEAD, 2 PAD

    // ---- SCRATCH ----
    mm::ScratchEngine& getScratch() noexcept { return scratch; }
    bool loadScratchSample (const juce::File& f, juce::String& error);
    void randomizeSequence();
    void clearSequence();
    void randomizeVelocities();   // so velocity: tempos fortes mais altos, acentos preservados
    void setParamReal (const juce::String& id, float realValue);

    double getCpuLoadPercent() const { return loadMeasurer.getLoadAsPercentage(); }
    double getCurrentBpm() const { return tempo.getBpm(); }

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    mm::TempoSync tempo;
    mm::MidiActivity midiActivity;
    std::atomic<float> peakLeft { 0.0f }, peakRight { 0.0f };
    std::atomic<int> activeVoices { 0 };

private:
    mm::SynthParams synthParams;
    mm::SynthEngine synth;
    mm::FxEngine::Params fxParams;
    mm::FxEngine fx { fxParams };
    mm::DrumPattern drumPattern;
    mm::DrumEngine::Params drumParams;
    mm::DrumEngine drums { drumParams, drumPattern };
    mm::SongData song;   // ideia musical atual (so message thread)
    mm::ReferenceResult reference;
    mm::ScratchEngine::Params scratchParams;
    mm::ScratchEngine scratch { scratchParams };
public:
    // OSCILOSCOPIO (barra de preset): ultimos 512 pontos da saida, atomicos (audio escreve, UI le, sem travas)
    static constexpr int scopeSize = 512;
    std::array<std::atomic<float>, scopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };
    bool isHostPlaying() const noexcept { return tempo.isPlaying(); }
private:
    std::atomic<float>* humVelParam = nullptr;
    std::atomic<float>* humTimeParam = nullptr;
    std::atomic<float>* masterGainParam = nullptr;
    juce::SmoothedValue<float> masterGain;
    juce::AudioProcessLoadMeasurer loadMeasurer;

    void applyBassMono (juce::AudioBuffer<float>&, int numSamples);
    std::atomic<float>* bassMonoParam = nullptr;
    mm::StateVariableFilter bassMonoFilters[2];
    float lastBassMonoFreq = -1.0f;

    mm::SequencePattern pattern;
    mm::SeqParams seqParams;
    mm::StepSequencer sequencer { seqParams, pattern };
    juce::MidiBuffer seqMidi;
    std::atomic<float>* midiOutParam = nullptr;
    std::atomic<float>* tempoSourceParam = nullptr;
    std::atomic<float>* internalBpmParam = nullptr;
    std::atomic<float>* timeFeelParam = nullptr;
    juce::Random random;

    std::unique_ptr<mm::PresetManager> presetManager;
    mm::MidiLearn midiLearn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MetalMachineAudioProcessor)
};
