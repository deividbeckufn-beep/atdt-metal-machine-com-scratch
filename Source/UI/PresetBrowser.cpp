#include "PresetBrowser.h"

using namespace juce;

namespace mm
{
namespace { constexpr int starWidth = 30; }

PresetBrowser::PresetBrowser (PresetManager& m) : manager (m)
{
    StringArray cats { "ALL", "FAVORITES" };
    cats.addArray (PresetManager::categories());
    for (auto& c : cats)
    {
        auto* b = categoryButtons.add (new TextButton (c));
        b->setComponentID (c);
        b->onClick = [this, c] { setCategory (c); };
        addAndMakeVisible (b);
    }

    search.setTextToShowWhenEmpty ("Buscar presets (nome, categoria, autor)", Colours::grey);
    search.onTextChange = [this] { refresh(); };
    search.onEscapeKey  = [this] { if (onClose) onClose(); };
    search.onReturnKey  = [this] { list.grabKeyboardFocus(); };
    addAndMakeVisible (search);

    list.setRowHeight (28);
    list.setColour (ListBox::backgroundColourId, Colours::transparentBlack);
    list.setWantsKeyboardFocus (true);
    addAndMakeVisible (list);

    deleteButton.setTooltip ("Envia o preset do usuario para a lixeira");
    deleteButton.onClick = [this] { confirmDelete(); };
    closeButton.onClick  = [this] { if (onClose) onClose(); };
    addAndMakeVisible (deleteButton);
    addAndMakeVisible (closeButton);

    setWantsKeyboardFocus (true);
    manager.addChangeListener (this);
    refresh();
}

PresetBrowser::~PresetBrowser()
{
    manager.removeChangeListener (this);
}

void PresetBrowser::visibilityChanged()
{
    if (isShowing())
    {
        refresh();
        list.grabKeyboardFocus();
    }
}

void PresetBrowser::setCategory (const String& c)
{
    category = c;
    refresh();
}

void PresetBrowser::refresh()
{
    const ScopedValueSetter<bool> guard (refreshing, true);

    manager.setBrowseFilter (category, search.getText());
    shown = manager.filter (category, search.getText());

    for (auto* b : categoryButtons)
    {
        const auto id = b->getComponentID();
        b->setButtonText (id + "  " + String (manager.filter (id, {}).size()));
        b->setToggleState (id == category, dontSendNotification);
    }

    list.updateContent();
    const int row = shown.indexOf (manager.getCurrentIndex());
    if (row >= 0) list.selectRow (row);
    else          list.deselectAllRows();

    const int cur = manager.getCurrentIndex();
    deleteButton.setEnabled (cur >= 0 && ! manager.getPresets()[cur].isFactory);
    repaint();
}

void PresetBrowser::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    auto r = getLocalBounds().toFloat();
    g.setColour (t.panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (t.edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

    g.setColour (t.accent);
    g.fillRect (12, 17, 3, 12);
    g.setFont (FontOptions (13.0f, Font::bold));
    g.setColour (t.text);
    g.drawText ("PRESET BROWSER", 22, 10, 200, 26, Justification::centredLeft);

    auto head = list.getBounds().withHeight (18).translated (0, -20);
    g.setFont (FontOptions (10.0f));
    g.setColour (t.textDim);
    head.removeFromLeft (starWidth);
    g.drawText ("NAME", head.removeFromLeft (head.getWidth() / 2), Justification::centredLeft);
    g.drawText ("CATEGORY", head.removeFromLeft (head.getWidth() / 2), Justification::centredLeft);
    g.drawText ("AUTHOR", head, Justification::centredLeft);

    g.drawText (String (shown.size()) + " presets  |  pasta do usuario: Documentos/ATDT Synth Machine/Presets",
                getLocalBounds().removeFromBottom (34).reduced (14, 0), Justification::centredLeft);
}

void PresetBrowser::resized()
{
    auto r = getLocalBounds().reduced (12);
    r.removeFromTop (30);

    auto bottom = r.removeFromBottom (26);
    closeButton.setBounds (bottom.removeFromRight (90));
    bottom.removeFromRight (6);
    deleteButton.setBounds (bottom.removeFromRight (90));
    r.removeFromBottom (8);

    auto left = r.removeFromLeft (170);
    const int buttonH = jmin (28, left.getHeight() / jmax (1, categoryButtons.size()));
    for (auto* b : categoryButtons)
        b->setBounds (left.removeFromTop (buttonH).reduced (0, 1));
    r.removeFromLeft (12);

    search.setBounds (r.removeFromTop (28));
    r.removeFromTop (26);
    list.setBounds (r);
}

void PresetBrowser::paintListBoxItem (int row, Graphics& g, int width, int height, bool selected)
{
    if (! isPositiveAndBelow (row, shown.size()))
        return;

    const auto& t = themeOf (*this);
    const int index = shown[row];
    const auto& p = manager.getPresets()[index];

    if (selected)
    {
        g.setColour (t.accent.withAlpha (0.18f));
        g.fillRoundedRectangle (0.0f, 1.0f, (float) width, (float) height - 2.0f, 3.0f);
        g.setColour (t.accent);
        g.fillRect (0, 4, 2, height - 8);
    }
    else if (row % 2 == 1)
    {
        g.setColour (t.panelRaised.withAlpha (0.5f));
        g.fillRect (0, 0, width, height);
    }

    // estrela de favorito desenhada (sem depender de fonte com simbolos)
    Path star;
    star.addStar ({ starWidth * 0.5f, height * 0.5f }, 5, 3.2f, 7.5f);
    if (manager.isFavorite (index)) { g.setColour (t.accent); g.fillPath (star); }
    else                            { g.setColour (t.textDim.withAlpha (0.6f)); g.strokePath (star, PathStrokeType (1.0f)); }

    auto r = Rectangle<int> (starWidth, 0, width - starWidth, height);
    const int col = r.getWidth() / 2;
    g.setFont (FontOptions (13.0f, selected ? Font::bold : Font::plain));
    g.setColour (t.text);
    g.drawText (p.name, r.removeFromLeft (col), Justification::centredLeft);
    g.setFont (FontOptions (11.0f));
    g.setColour (t.accentSoft);
    g.drawText (p.category, r.removeFromLeft (r.getWidth() / 2), Justification::centredLeft);
    g.setColour (t.textDim);
    g.drawText (p.isFactory ? "ATDT (fabrica)" : p.author, r, Justification::centredLeft);
}

void PresetBrowser::listBoxItemClicked (int row, const MouseEvent& e)
{
    if (! isPositiveAndBelow (row, shown.size()))
        return;
    if (e.x < starWidth)
        manager.toggleFavorite (shown[row]);
    else if (shown[row] != manager.getCurrentIndex())
        manager.load (shown[row]);
}

void PresetBrowser::listBoxItemDoubleClicked (int row, const MouseEvent&)
{
    if (isPositiveAndBelow (row, shown.size()))
    {
        if (shown[row] != manager.getCurrentIndex())
            manager.load (shown[row]);
        if (onClose) onClose();
    }
}

void PresetBrowser::selectedRowsChanged (int row)
{
    // navegacao pelo teclado: carrega o preset selecionado (sem loop no refresh)
    if (! refreshing && isPositiveAndBelow (row, shown.size()) && shown[row] != manager.getCurrentIndex())
        manager.load (shown[row]);
}

bool PresetBrowser::keyPressed (const KeyPress& key)
{
    if (key == KeyPress::escapeKey) { if (onClose) onClose(); return true; }
    return false;
}

void PresetBrowser::confirmDelete()
{
    const int cur = manager.getCurrentIndex();
    if (cur < 0 || manager.getPresets()[cur].isFactory)
        return;

    auto* w = new AlertWindow ("APAGAR PRESET",
                               "Enviar \"" + manager.getPresets()[cur].name + "\" para a lixeira?",
                               MessageBoxIconType::WarningIcon);
    w->setLookAndFeel (&getLookAndFeel());
    w->addButton ("APAGAR", 1);
    w->addButton ("CANCELAR", 0, KeyPress (KeyPress::escapeKey));
    SafePointer<PresetBrowser> self (this);
    w->enterModalState (true, ModalCallbackFunction::create ([self, cur] (int result)
    {
        if (result == 1 && self != nullptr)
            self->manager.deleteUserPreset (cur);
    }), true);
}
} // namespace mm
