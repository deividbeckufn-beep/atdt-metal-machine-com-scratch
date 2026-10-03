#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include "Sequencer/SequencePattern.h"
#include "Drums/DrumPattern.h"

namespace mm
{
// =============================================================================
//  PresetManager
//  - Presets de fabrica: definidos no codigo (FactoryPresets.cpp), sempre presentes.
//  - Presets do usuario: arquivos .atdtpreset em
//      Documentos/ATDT Synth Machine/Presets/<CATEGORIA>/<nome>.atdtpreset
//  - Favoritos: Documentos/ATDT Synth Machine/favorites.xml
//  Tudo roda no message thread (interface). O audio thread so ve os parametros.
// =============================================================================
struct PresetInfo
{
    juce::String name, category, author;
    bool isFactory = false;
    int factoryIndex = -1;
    juce::File file;

    juce::String key() const { return isFactory ? "factory:" + name : "user:" + file.getFullPathName(); }
};

class PresetManager : public juce::ChangeBroadcaster
{
public:
    PresetManager (juce::AudioProcessor&, juce::AudioProcessorValueTreeState&);

    static const juce::StringArray& categories();   // sem ALL / FAVORITES
    static juce::File getBaseFolder();
    static juce::File getUserPresetFolder()          { return getBaseFolder().getChildFile ("Presets"); }
    static constexpr const char* fileExtension = ".atdtpreset";

    void setPattern (SequencePattern* p) noexcept { pattern = p; }   // a sequencia faz parte do preset
    void setDrumPattern (DrumPattern* p) noexcept { drumPattern = p; } // e o padrao de bateria tambem

    void rescan();
    const juce::Array<PresetInfo>& getPresets() const noexcept { return presets; }

    // category: "ALL", "FAVORITES" ou uma das categories()
    juce::Array<int> filter (const juce::String& category, const juce::String& search) const;
    void setBrowseFilter (const juce::String& category, const juce::String& search);

    bool load (int index);
    void step (int delta);                 // setas < > do header (respeitam o filtro do browser)
    void markInit();                       // chamado pelo botao INIT

    int getCurrentIndex() const;
    juce::String getCurrentName() const;
    juce::String getCurrentCategory() const;
    bool isCurrentFactory() const;

    bool saveUserPreset (const juce::String& name, const juce::String& category);
    bool deleteUserPreset (int index);

    bool isFavorite (int index) const;
    void toggleFavorite (int index);

private:
    void applyValues (const juce::StringPairArray& values, const juce::StringArray& macroNames);
    void setCurrent (const PresetInfo&);
    void loadFavorites();
    void saveFavorites() const;

    juce::AudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    juce::Array<PresetInfo> presets;
    juce::StringArray favorites;
    juce::String browseCategory { "ALL" }, browseSearch;
    SequencePattern* pattern = nullptr;
    DrumPattern* drumPattern = nullptr;
};
} // namespace mm
