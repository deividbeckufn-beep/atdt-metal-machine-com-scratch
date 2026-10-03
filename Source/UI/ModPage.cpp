#include "ModPage.h"

using namespace juce;

namespace mm
{
// ====================================================================== LFO DISPLAY
LfoDisplay::LfoDisplay (APVTS& s, const LfoEngine& e, int i)
    : engine (e), index (i),
      shape (s.getRawParameterValue (ParamIDs::lfo (i + 1, "shape"))),
      amount (s.getRawParameterValue (ParamIDs::lfo (i + 1, "amount")))
{
    startTimerHz (30);
}

void LfoDisplay::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    auto r = getLocalBounds().toFloat();
    g.setColour (t.display);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (t.edge.withAlpha (0.6f));
    for (int i = 1; i < 4; ++i)
        g.drawVerticalLine (roundToInt (r.getX() + r.getWidth() * (float) i / 4.0f), r.getY(), r.getBottom());
    g.drawHorizontalLine (roundToInt (r.getCentreY()), r.getX(), r.getRight());

    auto area = r.reduced (6.0f, 8.0f);
    const int sh = jlimit (0, 6, (int) shape->load());

    // valores fixos so para desenhar S&H / Random de forma legivel
    static const float steps[] { 0.6f, -0.3f, 0.9f, -0.7f, 0.2f, -0.9f, 0.4f, -0.1f, 0.6f };
    auto valueAt = [&] (float p)
    {
        if (sh < 4 || sh == 6)
            return LfoEngine::shapeValue (sh, p, 0.0f, 0.0f, 0.0f);
        const float x = p * 8.0f;
        const int k = jlimit (0, 7, (int) x);
        if (sh == 4)
            return steps[k];
        const float f = x - (float) k;
        return steps[k] + (steps[k + 1] - steps[k]) * (0.5f - 0.5f * std::cos (MathConstants<float>::pi * f));
    };
    auto yOf = [&] (float v) { return area.getCentreY() - v * area.getHeight() * 0.5f; };

    Path path;
    const int n = jmax (2, (int) area.getWidth());
    for (int i = 0; i <= n; ++i)
    {
        const float p = (float) i / (float) n;
        const auto pt = Point<float> (area.getX() + p * area.getWidth(), yOf (valueAt (jmin (p, 0.9999f))));
        if (i == 0) path.startNewSubPath (pt); else path.lineTo (pt);
    }
    const float depth = jlimit (0.0f, 1.0f, amount->load());
    strokeNeon (g, path, t.accent.withMultipliedAlpha (0.35f + 0.65f * depth), 1.6f);

    // cursor: posicao real do LFO no motor de audio
    const float x = area.getX() + engine.getUiPhase (index) * area.getWidth();
    g.setColour (t.accentSoft.withAlpha (0.6f));
    g.drawVerticalLine (roundToInt (x), r.getY() + 2.0f, r.getBottom() - 2.0f);
    const float y = yOf (engine.getUiValue (index));
    g.setColour (t.text);
    g.fillEllipse (x - 3.0f, y - 3.0f, 6.0f, 6.0f);

    g.setFont (FontOptions (10.0f));
    g.setColour (t.textDim);
    g.drawText ("DEPTH " + String (roundToInt (depth * 100.0f)) + "%", getLocalBounds().reduced (6, 3),
                Justification::topRight);
}

// ====================================================================== LFO PANEL
LfoPanel::LfoPanel (MetalMachineAudioProcessor& p, int i)
    : Panel ("LFO " + String (i + 1)), state (p.apvts), index (i),
      chip ((int) ModSource::Lfo1 + i),
      shape (p.apvts, ParamIDs::lfo (i + 1, "shape")),
      display (p.apvts, p.getLfos(), i),
      rate (p.apvts, ParamIDs::lfo (i + 1, "rate"), "Rate"),
      division (p.apvts, ParamIDs::lfo (i + 1, "div"), "Div"),
      phase (p.apvts, ParamIDs::lfo (i + 1, "phase"), "Phase"),
      fade (p.apvts, ParamIDs::lfo (i + 1, "fade"), "Fade"),
      amount (p.apvts, ParamIDs::lfo (i + 1, "amount"), "Amount"),
      smooth (p.apvts, ParamIDs::lfo (i + 1, "smooth"), "Smooth"),
      syncParam (p.apvts.getRawParameterValue (ParamIDs::lfo (i + 1, "sync")))
{
    syncButton.setClickingTogglesState (true);
    syncButton.setTooltip ("SYNC: segue o BPM do REAPER (travado no compasso quando tocando)");
    syncAttachment = std::make_unique<APVTS::ButtonAttachment> (state, ParamIDs::lfo (i + 1, "sync"), syncButton);

    for (auto* c : std::initializer_list<Component*> { &chip, &shape, &display, &syncButton, &rate, &division, &phase, &fade, &amount, &smooth })
        addAndMakeVisible (c);

    timerCallback();
    startTimerHz (10);
}

void LfoPanel::timerCallback()
{
    const bool sync = syncParam->load() > 0.5f;
    rate.setVisible (! sync);
    division.setVisible (sync);
}

void LfoPanel::resized()
{
    // barra de titulo: [LFO n] ... [SYNC] [chip] [forma]
    auto bar = getTitleBar();
    shape.setBounds (bar.removeFromRight (96));
    bar.removeFromRight (6);
    chip.setBounds (bar.removeFromRight (44).reduced (0, 1));
    bar.removeFromRight (6);
    syncButton.setBounds (bar.removeFromRight (48).reduced (0, 1));

    auto r = getContentArea();
    auto controls = r.removeFromBottom (92);
    r.removeFromBottom (6);
    display.setBounds (r);

    // RATE (livre) ou DIV (sync) | PHASE | FADE | SMOOTH | AMOUNT
    const int w = controls.getWidth() / 5;
    auto rateArea = controls.removeFromLeft (w).reduced (2, 0);
    rate.setBounds (rateArea);
    division.setBounds (rateArea.withSizeKeepingCentre (rateArea.getWidth(), 40).translated (0, -6));
    phase.setBounds (controls.removeFromLeft (w).reduced (2, 0));
    fade.setBounds (controls.removeFromLeft (w).reduced (2, 0));
    smooth.setBounds (controls.removeFromLeft (w).reduced (2, 0));
    amount.setBounds (controls.reduced (2, 0));
}

// ====================================================================== MACROS
MacroPanel::MacroPanel (MetalMachineAudioProcessor& p) : Panel ("MACROS"), processor (p)
{
    for (int i = 0; i < numMacros; ++i)
    {
        auto* k = knobs.add (new Knob (p.apvts, ParamIDs::macro (i + 1), {}));
        addAndMakeVisible (k);

        auto* l = names.add (new Label());
        l->setJustificationType (Justification::centred);
        l->setEditable (false, true, false);
        l->setTooltip ("Duplo clique para renomear");
        l->setFont (FontOptions (12.0f, Font::bold));
        l->setText (p.getMacroName (i), dontSendNotification);
        l->onTextChange = [this, i, l]
        {
            processor.setMacroName (i, l->getText());
            l->setText (processor.getMacroName (i), dontSendNotification);
        };
        addAndMakeVisible (l);

        auto* c = chips.add (new ModSourceChip ((int) ModSource::Macro1 + i));
        addAndMakeVisible (c);
    }

    editButton.setClickingTogglesState (true);
    editButton.setTooltip ("Mostra na matriz so o que cada macro controla");
    editButton.onClick = [this] { if (onEditMacros) onEditMacros (editButton.getToggleState()); };
    addAndMakeVisible (editButton);

    startTimerHz (4);
}

void MacroPanel::timerCallback()
{
    for (int i = 0; i < names.size(); ++i)
        if (! names[i]->isBeingEdited())
            names[i]->setText (processor.getMacroName (i), dontSendNotification);
}

void MacroPanel::resized()
{
    editButton.setBounds (getTitleBar().removeFromRight (96));

    auto r = getContentArea();
    const int w = r.getWidth() / numMacros;
    for (int i = 0; i < numMacros; ++i)
    {
        auto col = r.removeFromLeft (w).reduced (4, 0);
        chips[i]->setBounds (col.removeFromBottom (20).withSizeKeepingCentre (44, 18));
        col.removeFromBottom (4);
        names[i]->setBounds (col.removeFromBottom (22));
        knobs[i]->setBounds (col);
    }
}

// ====================================================================== MATRIX
ModMatrixPanel::Row::Row (APVTS& s, int slotIndex) : slot (slotIndex)
{
    source.addItemList (modSourceNames(), 1);
    dest.addItemList (modDestNames(), 1);
    curve.addItemList (modCurveNames(), 1);
    mode.addItemList (modModeNames(), 1);

    amount.setSliderStyle (Slider::LinearBar);
    amount.getProperties().set ("bipolar", true);
    amount.setTextBoxIsEditable (false);

    srcA   = std::make_unique<APVTS::ComboBoxAttachment> (s, ParamIDs::mod (slot, "src"), source);
    dstA   = std::make_unique<APVTS::ComboBoxAttachment> (s, ParamIDs::mod (slot, "dst"), dest);
    curveA = std::make_unique<APVTS::ComboBoxAttachment> (s, ParamIDs::mod (slot, "curve"), curve);
    modeA  = std::make_unique<APVTS::ComboBoxAttachment> (s, ParamIDs::mod (slot, "mode"), mode);
    amtA   = std::make_unique<APVTS::SliderAttachment> (s, ParamIDs::mod (slot, "amt"), amount);
    amount.setDoubleClickReturnValue (true, 0.0);

    clear.setTooltip ("Remover esta modulacao");
    clear.onClick = [this, &s] { Knob::clearSlot (s, slot); };

    for (auto* c : std::initializer_list<Component*> { &source, &dest, &curve, &mode, &amount, &clear })
        addAndMakeVisible (c);
}

void ModMatrixPanel::Row::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    g.setColour (t.textDim);
    g.setFont (FontOptions (11.0f, Font::bold));
    g.drawText (String (slot), getLocalBounds().removeFromLeft (22), Justification::centred);
    g.setColour (t.accent);
    g.drawText ("->", source.getBounds().withX (source.getRight()).withWidth (22), Justification::centred);
}

void ModMatrixPanel::Row::resized()
{
    auto r = getLocalBounds().reduced (0, 2);
    r.removeFromLeft (24);
    clear.setBounds (r.removeFromRight (26));
    r.removeFromRight (6);
    mode.setBounds (r.removeFromRight (84));
    r.removeFromRight (4);
    curve.setBounds (r.removeFromRight (78));
    r.removeFromRight (8);
    source.setBounds (r.removeFromLeft (140));
    r.removeFromLeft (22);
    dest.setBounds (r.removeFromLeft (160));
    r.removeFromLeft (8);
    amount.setBounds (r);
}

ModMatrixPanel::ModMatrixPanel (APVTS& s) : Panel ("MOD MATRIX"), state (s)
{
    for (int i = 1; i <= numModSlots; ++i)
        addChildComponent (rows.add (new Row (s, i)));

    addButton.onClick = [this]
    {
        for (int i = 0; i < numModSlots; ++i)
            if (! isUsed (i + 1) && ! revealed[(size_t) i])
            {
                revealed[(size_t) i] = true;
                if (macroFilter)   // em modo macro, o slot novo ja nasce com MACRO 1
                    if (auto* p = state.getParameter (ParamIDs::mod (i + 1, "src")))
                        p->setValueNotifyingHost (p->convertTo0to1 ((float) ModSource::Macro1));
                break;
            }
        updateRows();
    };
    addAndMakeVisible (addButton);

    updateRows();
    startTimerHz (5);
}

bool ModMatrixPanel::isUsed (int slot) const
{
    return state.getRawParameterValue (ParamIDs::mod (slot, "src"))->load() > 0.5f
        || state.getRawParameterValue (ParamIDs::mod (slot, "dst"))->load() > 0.5f;
}

void ModMatrixPanel::setMacroFilter (bool onlyMacros)
{
    macroFilter = onlyMacros;
    updateRows();
    repaint();
}

void ModMatrixPanel::timerCallback()
{
    float h = 0.0f;
    for (int i = 1; i <= numModSlots; ++i)
        h += (float) i * (state.getRawParameterValue (ParamIDs::mod (i, "src"))->load() * 3.0f
                          + state.getRawParameterValue (ParamIDs::mod (i, "dst"))->load() * 97.0f);
    if (std::abs (h - lastHash) > 1.0e-6f)
    {
        lastHash = h;
        updateRows();
    }
}

void ModMatrixPanel::updateRows()
{
    int free = 0;
    for (int i = 0; i < numModSlots; ++i)
    {
        const bool used = isUsed (i + 1);
        if (used) revealed[(size_t) i] = false;   // slot em uso nao precisa mais do "revelado"

        const int src = (int) state.getRawParameterValue (ParamIDs::mod (i + 1, "src"))->load();
        const bool isMacro = src >= (int) ModSource::Macro1 && src <= (int) ModSource::Macro4;
        const bool show = (used || revealed[(size_t) i]) && (! macroFilter || isMacro || ! used);
        rows[i]->setVisible (show);
        if (! used && ! revealed[(size_t) i])
            ++free;
    }
    addButton.setVisible (free > 0);
    resized();
    repaint();
}

void ModMatrixPanel::paint (Graphics& g)
{
    Panel::paint (g);
    const auto& t = themeOf (*this);

    auto bar = getTitleBar();
    g.setFont (FontOptions (10.0f));
    g.setColour (macroFilter ? t.accent : t.textDim);
    g.drawText (macroFilter ? "MOSTRANDO: MACROS" : "ARRASTE UM CHIP SOBRE QUALQUER KNOB", bar, Justification::centredRight);

    auto head = getContentArea().removeFromTop (16);
    g.setColour (t.textDim);
    head.removeFromLeft (24);
    g.drawText ("SOURCE", head.removeFromLeft (140), Justification::centredLeft);
    head.removeFromLeft (22);
    g.drawText ("DESTINATION", head.removeFromLeft (160), Justification::centredLeft);
    head.removeFromRight (26 + 6);
    g.drawText ("MODE", head.removeFromRight (84), Justification::centredLeft);
    head.removeFromRight (4);
    g.drawText ("CURVE", head.removeFromRight (78), Justification::centredLeft);
    head.removeFromRight (8);
    head.removeFromLeft (8);
    g.drawText ("AMOUNT", head, Justification::centred);

    bool any = false;
    for (auto* r : rows) any = any || r->isVisible();
    if (! any)
    {
        g.setFont (FontOptions (12.0f));
        g.drawText (macroFilter ? "Nenhuma macro atribuida. Clique em + ADD MODULATION."
                                : "Nenhuma modulacao. Clique em + ADD MODULATION ou arraste um chip.",
                    getContentArea().withTrimmedTop (40).removeFromTop (40), Justification::centred);
    }
}

void ModMatrixPanel::resized()
{
    auto r = getContentArea();
    r.removeFromTop (18);
    const int rowH = jmin (26, (r.getHeight() - 26) / numModSlots);
    for (auto* row : rows)
        if (row->isVisible())
            row->setBounds (r.removeFromTop (rowH));
    if (addButton.isVisible())
        addButton.setBounds (r.removeFromTop (24).removeFromLeft (170).translated (24, 2));
}

// ====================================================================== PAGE
ModEnvPanel::ModEnvPanel (APVTS& s)
    : Panel ("ENV 3  MOD ENV"), chip ((int) ModSource::ModEnv),
      display (s, ParamIDs::menvAttack, ParamIDs::menvDecay, ParamIDs::menvSustain, ParamIDs::menvRelease),
      attack (s, ParamIDs::menvAttack, "Attack"), decay (s, ParamIDs::menvDecay, "Decay"),
      sustain (s, ParamIDs::menvSustain, "Sustain"), release (s, ParamIDs::menvRelease, "Release")
{
    for (auto* c : std::initializer_list<Component*> { &chip, &display, &attack, &decay, &sustain, &release })
        addAndMakeVisible (c);
}

void ModEnvPanel::resized()
{
    chip.setBounds (getTitleBar().removeFromRight (52).reduced (0, 1));
    auto r = getContentArea();
    auto knobs = r.removeFromBottom (82);
    r.removeFromBottom (6);
    display.setBounds (r);
    layoutRow (knobs, { &attack, &decay, &sustain, &release });
}

ModPage::ModPage (MetalMachineAudioProcessor& p) : macros (p), modEnv (p.apvts), matrix (p.apvts)
{
    addAndMakeVisible (modEnv);
    for (int i = 0; i < numLfos; ++i)
        addAndMakeVisible (lfos.add (new LfoPanel (p, i)));
    addAndMakeVisible (macros);
    addAndMakeVisible (matrix);
    macros.onEditMacros = [this] (bool on) { matrix.setMacroFilter (on); };
}

void ModPage::resized()
{
    constexpr int gap = 8;
    auto r = getLocalBounds();
    auto top = r.removeFromTop ((r.getHeight() - gap) * 46 / 100);
    r.removeFromTop (gap);

    const int w = (top.getWidth() - 3 * gap) / numLfos;
    for (auto* l : lfos)
    {
        l->setBounds (top.removeFromLeft (w));
        top.removeFromLeft (gap);
    }

    macros.setBounds (r.removeFromLeft (330));
    r.removeFromLeft (gap);
    modEnv.setBounds (r.removeFromLeft (270));
    r.removeFromLeft (gap);
    matrix.setBounds (r);
}
} // namespace mm
