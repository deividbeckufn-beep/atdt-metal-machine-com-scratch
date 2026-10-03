#include "DrumsPage.h"
#include "Hardware.h"

using namespace juce;

namespace mm
{
// ====================================================================== GRID
DrumGrid::DrumGrid (MetalMachineAudioProcessor& p)
    : processor (p), pattern (p.getDrumPattern()),
      length (p.apvts.getRawParameterValue (ParamIDs::drumLength))
{
    for (int r = 0; r < DrumPattern::numRows; ++r)
        for (auto* n : { "level", "pan", "pitch", "decay" })
            addAndMakeVisible (knobs.add (new MiniKnob (p.apvts, ParamIDs::drum (r, n))));
    startTimerHz (30);
}

int DrumGrid::visibleSteps() const { return (int) length->load() <= 16 ? 16 : 32; }

void DrumGrid::timerCallback()
{
    const auto rev = pattern.revision.load();
    const int step = processor.getDrumStep(), len = (int) length->load();
    const auto locks = processor.getBeatLocks();
    if (rev != lastRevision || step != lastStep || len != lastLength || locks != lastLocks)
    {
        lastRevision = rev; lastStep = step; lastLength = len; lastLocks = locks;
        repaint();
    }
}

void DrumGrid::resized()
{
    const float rowH = (float) (getHeight() - headerHeight) / (float) DrumPattern::numRows;
    for (int r = 0; r < DrumPattern::numRows; ++r)
        for (int k = 0; k < 4; ++k)
        {
            const int size = jmin (30, (int) rowH - 2);
            const int x = 86 + k * 39, y = headerHeight + (int) (r * rowH + (rowH - (float) size) * 0.5f);
            knobs[r * 4 + k]->setBounds (x, y, size, size);
        }
}

Rectangle<float> DrumGrid::cellBounds (int row, int col) const
{
    const float rowH = (float) (getHeight() - headerHeight) / (float) DrumPattern::numRows;
    const float colW = (float) (getWidth() - headerWidth) / (float) visibleSteps();
    return { (float) headerWidth + col * colW + 1.5f, (float) headerHeight + row * rowH + 2.0f, colW - 3.0f, rowH - 4.0f };
}

bool DrumGrid::cellAt (Point<int> p, int& row, int& step) const
{
    if (p.x < headerWidth || p.y < headerHeight) return false;
    const float rowH = (float) (getHeight() - headerHeight) / (float) DrumPattern::numRows;
    const float colW = (float) (getWidth() - headerWidth) / (float) visibleSteps();
    row = jlimit (0, DrumPattern::numRows - 1, (int) ((float) (p.y - headerHeight) / rowH));
    step = page * 32 + jlimit (0, visibleSteps() - 1, (int) ((float) (p.x - headerWidth) / colW));
    return true;
}

void DrumGrid::mouseDown (const MouseEvent& e)
{
    // clique no nome do canal: trava/destrava para o gerador
    if (e.x < 84 && e.y >= headerHeight)
    {
        const float rowH = (float) (getHeight() - headerHeight) / (float) DrumPattern::numRows;
        const int row = jlimit (0, DrumPattern::numRows - 1, (int) ((float) (e.y - headerHeight) / rowH));
        processor.setBeatLocks (processor.getBeatLocks() ^ (1u << row));
        repaint();
        return;
    }
    int row, step;
    if (! cellAt (e.getPosition(), row, step)) return;
    auto& cell = pattern.cells[(size_t) row][(size_t) step];
    if (e.mods.isAltDown())   // ALT+clique: roll (ratchet 1 -> 2 -> 3 -> 4 -> 1)
    {
        auto& rt = pattern.ratchet[(size_t) row][(size_t) step];
        rt.store ((uint8_t) (rt.load() % 4 + 1));
        if (cell.load() == 0) cell.store (105);
        paintValue = 255;
        pattern.touch();
        return;
    }
    const uint8_t cur = cell.load();
    const uint8_t target = e.mods.isPopupMenu() ? (uint8_t) 55 : e.mods.isShiftDown() ? (uint8_t) 127 : (uint8_t) 105;
    paintValue = (cur == target) ? (uint8_t) 0 : target;
    cell.store (paintValue);
    pattern.touch();
}

void DrumGrid::mouseDrag (const MouseEvent& e)
{
    if (paintValue == 255) return;
    int row, step;
    if (! cellAt (e.getPosition(), row, step)) return;
    auto& cell = pattern.cells[(size_t) row][(size_t) step];
    if (cell.load() != paintValue) { cell.store (paintValue); pattern.touch(); }
}

void DrumGrid::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    const int n = visibleSteps(), len = (int) length->load(), current = processor.getDrumStep();
    const float rowH = (float) (getHeight() - headerHeight) / (float) DrumPattern::numRows;
    const uint32_t locks = processor.getBeatLocks();
    static const char* noteNames[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int root = (int) processor.apvts.getRawParameterValue (ParamIDs::keyRoot)->load();

    for (int k = 0; k < 4; ++k)
        hw::micro (g, StringArray { "LVL", "PAN", "TUNE", "DECAY" }[k], { 82 + k * 39, 0, 38, headerHeight }, Justification::centred, t.textDim, 8.5f);

    for (int c = 0; c < n; ++c)
    {
        const int si = page * 32 + c;
        if (si % 4 == 0)
            hw::micro (g, String (si / 4 + 1), cellBounds (0, c).toNearestInt().withY (0).withHeight (headerHeight),
                       Justification::centredLeft, si < len ? t.text.withAlpha (0.7f) : t.textDim.withAlpha (0.4f), 9.0f);
    }

    const float colW = (float) (getWidth() - headerWidth) / (float) n;
    for (int r = 0; r < DrumPattern::numRows; ++r)
    {
        const float y = (float) headerHeight + r * rowH;
        const bool locked = (locks >> r) & 1u;
        g.setColour (r % 2 == 0 ? Colours::white.withAlpha (0.015f) : Colours::transparentBlack);
        g.fillRect (0.0f, y, (float) getWidth(), rowH);
        // nome + cadeado
        g.setFont (FontOptions (10.5f, Font::bold).withKerningFactor (0.06f));
        g.setColour (r == DrumRow::Bass808 ? t.accentSoft : t.text.withAlpha (0.9f));
        g.drawText (drumRowNames()[r], 6, (int) y, 60, (int) rowH, Justification::centredLeft);
        {
            const float lx = 68.0f, ly = y + rowH * 0.5f;
            g.setColour (locked ? t.accent : t.textDim.withAlpha (0.35f));
            g.drawRoundedRectangle (lx, ly - 1.5f, 9.0f, 7.0f, 1.0f, 1.2f);
            Path shackle; shackle.addCentredArc (lx + 4.5f, ly - 1.5f, 3.0f, 3.2f, 0.0f, -MathConstants<float>::halfPi,
                                                  MathConstants<float>::halfPi, true);
            g.strokePath (shackle, PathStrokeType (1.2f));
        }

        for (int c = 0; c < n; ++c)
        {
            const int si = page * 32 + c;
            auto cb = cellBounds (r, c);
            const bool inRange = si < len;
            const uint8_t v = pattern.cells[(size_t) r][(size_t) si].load();
            if (si == current && inRange)
            {
                g.setColour (t.accent.withAlpha (0.14f));
                g.fillRect (cb.expanded (1.5f, 2.0f));
            }
            // passo: aco escuro (vazio) | vermelho escuro (ghost) | neon (normal) | vermelho brilhante (acento)
            if (v == 0)
                g.setColour (t.track.withAlpha (inRange ? (si % 4 == 0 ? 1.0f : 0.75f) : 0.25f));
            else
                g.setColour ((v < 80 ? t.accentDark.brighter (0.4f) : v < 120 ? t.accent : t.accentSoft).withAlpha (inRange ? 1.0f : 0.3f));
            g.fillRoundedRectangle (cb, 2.0f);
            if (v >= 120 && inRange)
            {
                g.setColour (t.accent.withAlpha (0.35f));
                g.drawRoundedRectangle (cb.expanded (1.0f), 2.5f, 1.2f);
            }
            if (v == 0) continue;
            const int rt = (int) pattern.ratchet[(size_t) r][(size_t) si].load();
            if (rt > 1)   // roll: tracos verticais
            {
                g.setColour (Colours::black.withAlpha (0.6f));
                for (int j = 1; j < rt; ++j)
                    g.drawVerticalLine ((int) (cb.getX() + cb.getWidth() * (float) j / (float) rt), cb.getY() + 2.0f, cb.getBottom() - 2.0f);
            }
            if (r == DrumRow::Bass808 && colW > 24.0f)   // nota do 808
            {
                const int nn = ((root + pattern.bassNote[(size_t) si].load()) % 12 + 12) % 12;
                g.setColour (Colours::black.withAlpha (0.85f));
                g.setFont (FontOptions (9.0f, Font::bold));
                g.drawText (noteNames[nn], cb.toNearestInt(), Justification::centred);
            }
        }
    }
    g.setColour (Colours::black.withAlpha (0.7f));
    for (int c = 0; c <= n; c += 4)
        g.drawVerticalLine (headerWidth + (int) (c * colW), (float) headerHeight, (float) getHeight());
}

// ====================================================================== PAGE
DrumsPage::DrumsPage (MetalMachineAudioProcessor& p)
    : processor (p),
      style (p.apvts, "beat_style", "Style"), section (p.apvts, "beat_section", "Section"),
      halfTime (p.apvts, "beat_halftime", "Half-Time"), doubleKick (p.apvts, "beat_doublekick", "Double Kick"),
      bars (p.apvts, "beat_bars", "Bars"), randMode (p.apvts, "beat_randmode", "Random"),
      energy (p.apvts, "beat_energy", "Energy"), density (p.apvts, "beat_density", "Density"),
      aggression (p.apvts, "beat_aggression", "Aggr"), electronic (p.apvts, "beat_electronic", "Electr"),
      trap (p.apvts, "beat_trap", "Trap"), glitch (p.apvts, "beat_glitch", "Glitch"),
      modernity (p.apvts, "beat_modernity", "Modern"), syncopation (p.apvts, "beat_syncopation", "Syncop"),
      space (p.apvts, "beat_space", "Space"), complexity (p.apvts, "beat_complexity", "Complex"),
      type808 (p.apvts, "drum808_type"), drive808 (p.apvts, "drum808_drive"), glide808 (p.apvts, "drum808_glide"),
      dragButton (p),
      playMode (p.apvts, ParamIDs::drumMode, "Play"),
      length (p.apvts, ParamIDs::drumLength, "Length"),
      swing (p.apvts, ParamIDs::drumSwing, "Swing"),
      volume (p.apvts, ParamIDs::drumVolume, "Volume"),
      grid (p)
{
    onButton.setClickingTogglesState (true);
    onAttachment = std::make_unique<APVTS::ButtonAttachment> (p.apvts, ParamIDs::drumOn, onButton);
    keysButton.setClickingTogglesState (true);
    keysButton.setTooltip ("Teclado toca a bateria (mapa GM: 36 kick, 38 snare, 39 clap, 42 hat, 46 open hat, 49 crash, 51 ride)");
    keysAttachment = std::make_unique<APVTS::ButtonAttachment> (p.apvts, ParamIDs::drumKeys, keysButton);

    generateButton.setTooltip ("Gera um beat NOVO no estilo/secao escolhidos (canais travados ficam como estao)");
    generateButton.onClick = [this] { processor.generateBeat (true); };
    fillButton.setTooltip ("Reescreve o ultimo tempo com um fill");
    fillButton.onClick = [this] { processor.makeBeatFill(); };
    randomButton.setTooltip ("Nova variacao respeitando estilo, secao, BPM, KEY e travas");
    randomButton.onClick = [this] { processor.randomizeBeat(); };
    for (int i = 0; i < 4; ++i)
    {
        varButtons[i].setTooltip ("Variacao " + String::charToString ((juce_wchar) ('A' + i)) + " do mesmo beat");
        varButtons[i].onClick = [this, i]
        {
            if (auto* prm = processor.apvts.getParameter ("beat_variation"))
                prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i));
            processor.generateBeat (false);   // mesma seed: variacao coerente
        };
    }
    clearButton.onClick = [this] { processor.clearDrums(); };
    page1Button.onClick = [this] { grid.setPage (0); };
    page2Button.onClick = [this] { grid.setPage (1); };
    exportButton.setTooltip ("Salva o beat (.mid) em Documentos/ATDT Synth Machine/MIDI");
    exportButton.onClick = [this] { exportMidi (false); };
    multiButton.setTooltip ("MIDI multitrilha: Kick, Snare, Hats, Perc, 808... em trilhas separadas");
    multiButton.onClick = [this] { exportMidi (true); };

    const auto& lib = getDrumLibrary();
    String genre;
    for (int i = 0; i < (int) lib.size(); ++i)
    {
        if (genre != lib[(size_t) i].genre) { genre = lib[(size_t) i].genre; patternBox.addSectionHeading (genre); }
        patternBox.addItem (lib[(size_t) i].name, i + 1);
    }
    patternBox.setTextWhenNothingSelected ("LIBRARY");
    patternBox.onChange = [this]
    {
        if (const int id = patternBox.getSelectedId(); id > 0)
            processor.loadDrumPattern (id - 1);
    };

    for (auto* c : std::initializer_list<Component*> { &beatPanel, &gridPanel, &style, &section, &halfTime, &doubleKick, &bars,
                                                       &randMode, &generateButton, &fillButton, &randomButton, &energy,
                                                       &density, &aggression, &electronic, &trap, &glitch, &onButton,
                                                       &keysButton, &clearButton, &exportButton, &multiButton, &dragButton,
                                                       &playMode, &length, &swing, &volume, &patternBox, &grid })
        addAndMakeVisible (c);
    for (auto& b : varButtons) addAndMakeVisible (b);
    for (auto* c : std::initializer_list<Component*> { &modernity, &syncopation, &space, &complexity, &type808, &drive808, &glide808 })
        addAndMakeVisible (c);
    static const char* tips[] { "808 no kick, rolls antes da caixa, ghosts, dinamica, perc digital",
                                "Acentos, kick no 3, double kick no fim, crash", "Hats 1/16, rolls 1/64, mais 808, clap",
                                "Clap, perc digital, open hats, zaps", "808 mais grave, menos brilho, mais espaco",
                                "Crash por compasso, impacto, open hat, clap", "Sem ghosts, velocity firme, 808 so no kick",
                                "Ghosts, acentos de hat, antecipacao" };
    for (int i = 0; i < BeatGenerator::transformNames().size(); ++i)
    {
        auto* b = makeItButtons.add (new TextButton (BeatGenerator::transformNames()[i]));
        b->setTooltip (tips[i]);
        b->onClick = [this, i] { processor.applyBeatTransform (i); };
        addAndMakeVisible (b);
    }
    drive808.setTooltip ("808 DRIVE"); glide808.setTooltip ("808 GLIDE (tempo do slide)");
    addChildComponent (page1Button);
    addChildComponent (page2Button);
    startTimerHz (5);
}

void DrumsPage::exportMidi (bool multi)
{
    const auto f = processor.exportBeatMidi (multi, false);
    if (f.existsAsFile())
    {
        AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "EXPORT MIDI",
            "MIDI salvo:\n" + f.getFullPathName() + "\n\nArraste o arquivo para o REAPER"
            + String (multi ? " (ele cria uma trilha por instrumento)." : "."));
        f.revealToUser();
    }
    else
        AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "EXPORT MIDI",
                                          "Nao foi possivel salvar o MIDI. Verifique a permissao da pasta Documentos.");
}

void DrumsPage::timerCallback()
{
    const auto rev = processor.getDrumPattern().revision.load();
    if (rev != lastRevision)
    {
        lastRevision = rev;
        patternBox.setText (processor.getDrumPattern().name.toUpperCase(), dontSendNotification);
    }
    const int var = (int) processor.apvts.getRawParameterValue ("beat_variation")->load();
    for (int i = 0; i < 4; ++i) varButtons[i].setToggleState (i == var, dontSendNotification);
    const bool paged = (int) processor.apvts.getRawParameterValue (ParamIDs::drumLength)->load() > 32;
    if (! paged && grid.getPage() != 0) grid.setPage (0);
    page1Button.setVisible (paged); page2Button.setVisible (paged);
    page1Button.setToggleState (grid.getPage() == 0, dontSendNotification);
    page2Button.setToggleState (grid.getPage() == 1, dontSendNotification);
}

void DrumsPage::resized()
{
    constexpr int gap = 6;
    auto r = getLocalBounds();
    auto top = r.removeFromTop (204);
    r.removeFromTop (gap);
    beatPanel.setBounds (top);
    gridPanel.setBounds (r);
    grid.setBounds (gridPanel.getContentArea().translated (r.getX(), r.getY()));
    {   // 808 na barra do painel PATTERN: tipo + drive + glide
        auto bar = gridPanel.getTitleBar().translated (r.getX(), r.getY());
        glide808.setBounds (bar.removeFromRight (24));
        bar.removeFromRight (4);
        drive808.setBounds (bar.removeFromRight (24));
        bar.removeFromRight (6);
        type808.setBounds (bar.removeFromRight (110));
    }

    auto c = beatPanel.getContentArea().translated (top.getX(), top.getY()).reduced (4, 0);
    auto take = [&] (Rectangle<int>& row, int w) { auto a = row.removeFromLeft (w); row.removeFromLeft (gap); return a; };
    // linha 1: gerador
    auto l1 = c.removeFromTop (42);
    style.setBounds (take (l1, 150));
    section.setBounds (take (l1, 110));
    {
        auto vb = take (l1, 132).withTrimmedTop (14);
        for (auto& b : varButtons) { b.setBounds (vb.removeFromLeft (31).reduced (1, 1)); vb.removeFromLeft (2); }
    }
    halfTime.setBounds (take (l1, 100));
    doubleKick.setBounds (take (l1, 100));
    bars.setBounds (take (l1, 80));
    randMode.setBounds (l1.removeFromRight (92));
    l1.removeFromRight (gap);
    auto btns = l1.withTrimmedTop (14);
    const int bw = (btns.getWidth() - 2 * gap) / 3;
    generateButton.setBounds (btns.removeFromLeft (bw)); btns.removeFromLeft (gap);
    fillButton.setBounds (btns.removeFromLeft (bw)); btns.removeFromLeft (gap);
    randomButton.setBounds (btns);
    c.removeFromTop (4);

    // linha 2: carater (10 knobs) + maquina
    auto l2 = c.removeFromTop (76);
    for (auto* k : { &energy, &density, &aggression, &electronic, &trap, &glitch, &modernity, &syncopation, &space, &complexity })
        k->setBounds (take (l2, 56));
    {
        auto col = take (l2, 88).reduced (0, 8);
        onButton.setBounds (col.removeFromTop (col.getHeight() / 2).reduced (0, 2));
        keysButton.setBounds (col.reduced (0, 2));
    }
    playMode.setBounds (take (l2, 92).withSizeKeepingCentre (92, 44));
    for (auto* k : { &length, &swing, &volume })
        k->setBounds (take (l2, 56));
    {
        auto col = l2.reduced (0, 8);
        patternBox.setBounds (col.removeFromTop (col.getHeight() / 2).reduced (0, 2));
        auto row = col.reduced (0, 2);
        clearButton.setBounds (row.removeFromLeft (60)); row.removeFromLeft (4);
        page1Button.setBounds (row.removeFromLeft (52)); row.removeFromLeft (2);
        page2Button.setBounds (row.removeFromLeft (52));
    }
    c.removeFromTop (4);

    // linha 3: MAKE IT ... + exportacao
    auto l3 = c.removeFromTop (28);
    dragButton.setBounds (l3.removeFromRight (104)); l3.removeFromRight (gap);
    multiButton.setBounds (l3.removeFromRight (104)); l3.removeFromRight (gap);
    exportButton.setBounds (l3.removeFromRight (104)); l3.removeFromRight (gap * 3);
    makeItArea = l3.removeFromLeft (64);
    const int mw = (l3.getWidth() - gap * (makeItButtons.size() - 1)) / jmax (1, makeItButtons.size());
    for (auto* b : makeItButtons) { b->setBounds (l3.removeFromLeft (mw)); l3.removeFromLeft (gap); }
}

void DrumsPage::paintOverChildren (Graphics& g)
{
    const auto& t = themeOf (*this);
    hw::micro (g, "Make it", makeItArea, Justification::centredLeft, t.accentSoft, 9.5f);
    auto bar = gridPanel.getTitleBar().translated (gridPanel.getX(), gridPanel.getY());
    hw::micro (g, "808", bar.withTrimmedRight (164).removeFromRight (30), Justification::centredRight, t.accentSoft, 9.5f);
}
} // namespace mm
