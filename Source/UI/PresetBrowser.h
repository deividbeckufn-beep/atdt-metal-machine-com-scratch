#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MetalLookAndFeel.h"
#include "Presets/PresetManager.h"

namespace mm
{
// =============================================================================
//  Navegador de presets: categorias, busca, favoritos, autor.
//  Clique = carrega (ouvir na hora) | setas cima/baixo = navega e carrega
//  Duplo clique / Enter = carrega e fecha | Esc = fecha | estrela = favorito
// =============================================================================
class PresetBrowser : public juce::Component,
                      private juce::ListBoxModel,
                      private juce::ChangeListener
{
public:
    explicit PresetBrowser (PresetManager&);
    ~PresetBrowser() override;

    std::function<void()> onClose;

    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void visibilityChanged() override;

private:
    int getNumRows() override { return shown.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void returnKeyPressed (int) override { if (onClose) onClose(); }
    void deleteKeyPressed (int) override { confirmDelete(); }
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }

    void setCategory (const juce::String&);
    void confirmDelete();

    PresetManager& manager;
    juce::String category { "ALL" };
    juce::OwnedArray<juce::TextButton> categoryButtons;
    juce::TextEditor search;
    juce::ListBox list { "presets", this };
    juce::TextButton deleteButton { "DELETE" }, closeButton { "CLOSE" };
    juce::Array<int> shown;
    bool refreshing = false;
};
} // namespace mm
