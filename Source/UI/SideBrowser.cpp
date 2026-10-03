#include "SideBrowser.h"
#include "Hardware.h"

using namespace juce;

namespace mm
{
static const StringArray kTags { "Metal", "Metalcore", "Nu Metal", "Industrial", "Trap", "Dubstep", "Cinematic", "Dark",
                                 "Aggressive", "Atmospheric", "Heavy", "Digital", "Glitch", "Breakdown", "808", "Growl" };

SideBrowser::SideBrowser (MetalMachineAudioProcessor& p) : processor (p)
{
    search.setTextToShowWhenEmpty ("Search presets...", Colour (0xff7a8089));
    // busca = display embutido escuro (nao campo de formulario web)
    search.setColour (TextEditor::backgroundColourId, Colour (0xff060708));
    search.setColour (TextEditor::outlineColourId, Colours::black);
    search.setColour (TextEditor::focusedOutlineColourId, Colour (0xffff2030).withAlpha (0.6f));
    search.setColour (TextEditor::textColourId, Colour (0xffd9dce1));
    search.setFont (FontOptions (12.0f));
    search.setTooltip ("Digite e tecle ENTER: lista os presets encontrados (nome, categoria, autor)");
    search.onReturnKey = [this]
    {
        showList (processor.getPresetManager().filter ("ALL", search.getText()), "\"" + search.getText() + "\"", &search);
    };
    addAndMakeVisible (search);
    for (auto& c : PresetManager::categories()) items.push_back ({ c, "cat", c, {} });
    for (auto& t : kTags) items.push_back ({ t, "tag", t, {} });
    for (auto s : { "Factory", "User", "Reference", "Favorites" }) items.push_back ({ s, "src", s, {} });
    setRepaintsOnMouseActivity (false);
    startTimerHz (4);
}

void SideBrowser::timerCallback()
{
    const auto n = processor.getPresetManager().getCurrentName();
    if (n != lastName) { lastName = n; repaint(); }
}

Array<int> SideBrowser::sourceList (const String& src) const
{
    auto& pm = processor.getPresetManager();
    if (src == "Favorites") return pm.filter ("FAVORITES", {});
    Array<int> out;
    const auto& ps = pm.getPresets();
    for (int i = 0; i < ps.size(); ++i)
    {
        const auto& p = ps.getReference (i);
        if (src == "Factory" && p.isFactory) out.add (i);
        if (src == "User" && ! p.isFactory && p.category != "REFERENCE") out.add (i);
        if (src == "Reference" && p.category == "REFERENCE") out.add (i);
    }
    return out;
}

void SideBrowser::showList (const Array<int>& indices, const String& title, Component* target)
{
    auto& pm = processor.getPresetManager();
    PopupMenu menu;
    menu.addSectionHeader (title.toUpperCase() + "  (" + String (indices.size()) + ")");
    if (indices.isEmpty()) menu.addItem (-1, "Nenhum preset", false);
    String cat;
    for (int idx : indices)
    {
        const auto& p = pm.getPresets().getReference (idx);
        if (p.category != cat && indices.size() > 12) { cat = p.category; menu.addSectionHeader (cat); }
        menu.addItem (idx + 1, (pm.isFavorite (idx) ? "* " : "") + p.name, true, idx == pm.getCurrentIndex());
    }
    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (target).withMaximumNumColumns (3),
                        [this] (int r) { if (r > 0) processor.getPresetManager().load (r - 1); });
}

void SideBrowser::mouseMove (const MouseEvent& e)
{
    int h = -1;
    for (size_t i = 0; i < items.size(); ++i) if (items[i].bounds.contains (e.getPosition())) h = (int) i;
    if (cardStar.contains (e.getPosition())) h = -2;
    if (h != hoverItem) { hoverItem = h; repaint(); }
}

void SideBrowser::mouseUp (const MouseEvent& e)
{
    auto& pm = processor.getPresetManager();
    if (cardStar.contains (e.getPosition()) && pm.getCurrentIndex() >= 0) { pm.toggleFavorite (pm.getCurrentIndex()); repaint(); return; }
    for (auto& it : items)
        if (it.bounds.contains (e.getPosition()))
        {
            if (it.kind == "cat")
            {
                pm.setBrowseFilter (it.value, {});   // setas < > passam a navegar nesta categoria
                showList (pm.filter (it.value, {}), it.label, this);
            }
            else if (it.kind == "tag") showList (pm.filter ("ALL", it.value), "TAG " + it.label, this);
            else                       showList (sourceList (it.value), it.label, this);
            return;
        }
}

void SideBrowser::resized()
{
    auto r = getLocalBounds().reduced (10, 10);
    search.setBounds (r.removeFromTop (28));
    r.removeFromTop (10);
    card = r.removeFromBottom (78);
    cardStar = card.withTrimmedLeft (card.getWidth() - 30).removeFromTop (30);
    r.removeFromBottom (8);

    int idx = 0;
    catHeader = r.removeFromTop (18);
    const int nCat = PresetManager::categories().size();
    for (int i = 0; i < nCat; ++i) items[(size_t) idx++].bounds = r.removeFromTop (21);
    r.removeFromTop (8);
    tagHeader = r.removeFromTop (18);
    for (int row = 0; row < (kTags.size() + 2) / 3; ++row)
    {
        auto line = r.removeFromTop (24);
        const int w = line.getWidth() / 3;
        for (int c = 0; c < 3 && idx < nCat + kTags.size(); ++c)
            items[(size_t) idx++].bounds = line.removeFromLeft (w).reduced (2, 2);
    }
    r.removeFromTop (8);
    srcHeader = r.removeFromTop (18);
    for (int i = 0; i < 4; ++i) items[(size_t) idx++].bounds = r.removeFromTop (20);
}

void SideBrowser::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    hw::plate (g, getLocalBounds().toFloat().reduced (1.0f), t, 8.0f, false);
    auto& pm = processor.getPresetManager();
    const auto current = pm.getCurrentCategory();

    auto header = [&] (Rectangle<int> a, const String& txt)
    {
        hw::micro (g, txt, a, Justification::centredLeft, t.textDim, 9.0f);
        g.setColour (Colours::black.withAlpha (0.5f));
        g.drawHorizontalLine (a.getBottom() - 1, (float) a.getX() + 70.0f, (float) a.getRight());
    };
    header (catHeader, "Library");
    header (tagHeader, "Tags");
    header (srcHeader, "Browser");

    for (size_t i = 0; i < items.size(); ++i)
    {
        const auto& it = items[i];
        const bool hover = (int) i == hoverItem;
        if (it.kind == "cat")
        {
            const bool active = it.value == current;
            if (active || hover)
            {
                g.setColour (active ? t.accent.withAlpha (0.16f) : Colours::white.withAlpha (0.04f));
                g.fillRect (it.bounds);
                if (active) { g.setColour (t.accent); g.fillRect (it.bounds.withWidth (2)); }
            }
            // indicador: quantos presets
            g.setColour (active ? t.accentSoft : t.text.withAlpha (hover ? 0.95f : 0.8f));
            g.setFont (FontOptions (11.5f, Font::bold).withKerningFactor (0.06f));
            g.drawText (it.label, it.bounds.withTrimmedLeft (12), Justification::centredLeft);
            hw::micro (g, String (pm.filter (it.value, {}).size()), it.bounds.withTrimmedRight (6), Justification::centredRight, t.textDim, 8.5f);
        }
        else if (it.kind == "tag")
        {
            g.setColour (hover ? t.accent.withAlpha (0.18f) : t.display);
            g.fillRoundedRectangle (it.bounds.toFloat(), 2.0f);
            g.setColour (hover ? t.accent : Colours::black);
            g.drawRoundedRectangle (it.bounds.toFloat(), 2.0f, 1.0f);
            g.setColour (hover ? t.text : t.textDim.brighter (0.2f));
            g.setFont (FontOptions (9.5f, Font::bold));
            g.drawText (it.label, it.bounds, Justification::centred);
        }
        else
        {
            if (hover) { g.setColour (Colours::white.withAlpha (0.04f)); g.fillRect (it.bounds); }
            hw::led (g, { (float) it.bounds.getX() + 8.0f, (float) it.bounds.getCentreY() }, 2.6f, hover ? 1.0f : 0.0f, t);
            g.setColour (t.text.withAlpha (0.85f));
            g.setFont (FontOptions (11.0f));
            g.drawText (it.label, it.bounds.withTrimmedLeft (20), Justification::centredLeft);
            hw::micro (g, String (sourceList (it.value).size()), it.bounds.withTrimmedRight (6), Justification::centredRight, t.textDim, 8.5f);
        }
    }

    // card do preset atual
    hw::inset (g, card.toFloat(), t, 3.0f);
    auto c = card.reduced (10, 8);
    g.setColour (t.text);
    g.setFont (FontOptions (13.0f, Font::bold).withKerningFactor (0.04f));
    g.drawText (pm.getCurrentName().isNotEmpty() ? pm.getCurrentName().toUpperCase() : String ("INIT"), c.removeFromTop (18).withTrimmedRight (26),
                Justification::centredLeft, true);
    hw::micro (g, (current.isNotEmpty() ? current : String ("Init")) + (pm.isCurrentFactory() ? "  |  Factory" : "  |  User"),
               c.removeFromTop (14), Justification::centredLeft, t.textDim, 8.5f);
    hw::micro (g, "Click a category, tag or source", c, Justification::bottomLeft, t.textDim.withAlpha (0.6f), 8.0f);
    {   // estrela
        const bool fav = pm.getCurrentIndex() >= 0 && pm.isFavorite (pm.getCurrentIndex());
        Path star; const auto ctr = cardStar.toFloat().getCentre(); const float ro = 7.0f, ri = 3.2f;
        for (int i = 0; i < 10; ++i)
        {
            const float a = -MathConstants<float>::halfPi + i * MathConstants<float>::pi / 5.0f; const float rad = i % 2 ? ri : ro;
            const Point<float> pt (ctr.x + std::cos (a) * rad, ctr.y + std::sin (a) * rad);
            if (i == 0) star.startNewSubPath (pt); else star.lineTo (pt);
        }
        star.closeSubPath();
        if (fav) { g.setColour (t.accent); g.fillPath (star); }
        else     { g.setColour (hoverItem == -2 ? t.accentSoft : t.textDim); g.strokePath (star, PathStrokeType (1.2f)); }
    }
}
} // namespace mm
