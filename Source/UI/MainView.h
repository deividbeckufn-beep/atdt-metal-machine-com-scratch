#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "SynthPage.h"
#include "ModPage.h"
#include "FxPage.h"
#include "DrumsPage.h"
#include "SongPage.h"
#include "RefLabPage.h"
#include "SideBrowser.h"
#include "ScratchPage.h"
#include "SeqPage.h"
#include "PresetBrowser.h"

namespace mm
{
// -----------------------------------------------------------------------------
// Header: logo, preset, BPM/SYNC, MIDI, VOICES, CPU, medidor, INIT, SETTINGS
// -----------------------------------------------------------------------------
// Caixa do preset atual no header (clique = abre/fecha o browser)
class PresetBox : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PresetBox (PresetManager& m) : manager (m) { setTooltip ("Clique para abrir o navegador de presets"); }
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true;  repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override { if (e.mouseWasClicked() && onClick) onClick(); }
    std::function<void()> onClick;

private:
    PresetManager& manager;
    bool hover = false;
};

// Estrela de favorito do preset atual (desenhada; vermelho = favorito)
class StarButton : public juce::Component
{
public:
    std::function<bool()> isOn;
    std::function<void()> onClick;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override { if (e.mouseWasClicked() && onClick) { onClick(); repaint(); } }
};

// Layout compartilhado (cabecalho / barra lateral / paginas)
namespace Layout
{
    constexpr int row1 = 74;          // faixa da marca + navegacao + status
    constexpr int row2 = 44;          // faixa do preset (coluna de conteudo)
    constexpr int sidebar = 250;      // barra lateral de presets
    constexpr int statusW = 410;      // bloco de status (direita)
    constexpr int logoW = 372;        // marca (esquerda)
}

class HeaderBar : public juce::Component, private juce::Timer
{
public:
    explicit HeaderBar (MetalMachineAudioProcessor&);
    ~HeaderBar() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void (float)> onScaleChosen;
    std::function<void (const juce::String&)> onThemeChosen;
    std::function<void()> onPresetClicked;

private:
    void timerCallback() override;
    void showSettingsMenu();
    void save();
    void showSaveAsDialog();

    MetalMachineAudioProcessor& processor;
    PresetBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };
    juce::TextButton initButton { "INIT" }, saveButton { "SAVE" }, saveAsButton { "SAVE AS" }, settingsButton { "MENU" },
                     randomButton { "RANDOM" };
    StarButton star;
    juce::Rectangle<int> scopeArea, categoryArea;
    uint32_t lastNotes = 0;
    float midiLed = 0.0f, meterL = 0.0f, meterR = 0.0f;
};

// -----------------------------------------------------------------------------
// Pagina de modulo ainda nao implementado: explica o que vem e quando.
// Nao exibe nenhum controle falso.
// -----------------------------------------------------------------------------
class UpcomingPage : public juce::Component
{
public:
    UpcomingPage (juce::String title, juce::String phase, juce::StringArray features);
    void paint (juce::Graphics&) override;

private:
    juce::String title, phase;
    juce::StringArray features;
};

class MainView : public juce::Component, public juce::DragAndDropContainer
{
public:
    static constexpr int baseWidth = 1520, baseHeight = 940;

    explicit MainView (MetalMachineAudioProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

    HeaderBar header;

private:
    void showTab (int index);
    void setBrowserVisible (bool);

    MetalMachineAudioProcessor& processor;
    juce::OwnedArray<juce::TextButton> tabs;
    juce::OwnedArray<ModSourceChip> sourceChips;
    PresetBrowser browser;
    std::vector<std::unique_ptr<juce::Component>> pages;
    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<juce::Component> sideBrowser;
    int currentTab = 0;
    juce::Rectangle<int> chipLabelArea, footerArea;
};
} // namespace mm
