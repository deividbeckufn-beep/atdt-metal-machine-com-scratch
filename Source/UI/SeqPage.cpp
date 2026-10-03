#include "SeqPage.h"

using namespace juce;

namespace mm
{
// ======================================================================== GRID
StepGrid::StepGrid (MetalMachineAudioProcessor& p)
    : processor (p), pattern (p.getPattern()),
      length (p.apvts.getRawParameterValue (ParamIDs::seqLength)),
      root (p.apvts.getRawParameterValue (ParamIDs::keyRoot)),
      scale (p.apvts.getRawParameterValue (ParamIDs::keyScale)),
      mode (p.apvts.getRawParameterValue (ParamIDs::seqMode))
{
    startTimerHz (30);
}

void StepGrid::timerCallback()
{
    const auto rev = pattern.revision.load();
    const int step = processor.getSequencerStep();
    const int len = (int) length->load(), r = (int) root->load(), sc = (int) scale->load(), m = (int) mode->load();
    if (rev != lastRevision || step != lastStep || len != lastLength || r != lastRoot || sc != lastScale || m != lastMode)
    {
        lastRevision = rev; lastStep = step; lastLength = len; lastRoot = r; lastScale = sc; lastMode = m;
        repaint();
    }
}

int StepGrid::visibleSteps() const { return (int) length->load() <= 16 ? 16 : 32; }

Rectangle<int> StepGrid::rowArea (int row) const
{
    auto r = getLocalBounds().withTrimmedLeft (labelWidth);
    const int h = r.getHeight();
    const int noteH = h * 44 / 100, velH = h * 19 / 100, gateH = h * 15 / 100;
    const int toggleH = (h - noteH - velH - gateH) / 3;
    const int heights[] { noteH, velH, gateH, toggleH, toggleH, toggleH };
    for (int i = 0; i < row; ++i) r.removeFromTop (heights[i]);
    return r.withHeight (heights[row]);
}

int StepGrid::rowAt (int y) const
{
    for (int i = 0; i < NumRows; ++i)
        if (rowArea (i).expanded (0, 0).contains (labelWidth + 1, y))
            return i;
    return NoRow;
}

int StepGrid::columnAt (int x) const
{
    const float w = (float) (getWidth() - labelWidth) / (float) visibleSteps();
    return jlimit (0, visibleSteps() - 1, (int) ((float) (x - labelWidth) / w));
}

void StepGrid::applyAt (Point<int> p)
{
    if (dragRow == NoRow || p.x < labelWidth)
        return;
    const int col = columnAt (p.x);
    auto& s = pattern.steps[(size_t) stepAt (col)];
    const auto area = rowArea (dragRow);
    const float frac = 1.0f - jlimit (0.0f, 1.0f, (float) (p.y - area.getY()) / (float) area.getHeight());

    switch (dragRow)
    {
        case NoteRow:
        {
            const int range = SequencePattern::maxDegree - SequencePattern::minDegree;
            s.degree = jlimit (SequencePattern::minDegree, SequencePattern::maxDegree,
                               SequencePattern::minDegree + roundToInt (frac * (float) range));
            s.on = true;
            break;
        }
        case VelRow:    s.velocity = jmax (0.05f, frac); break;
        case GateRow:   s.gate = jlimit (0.05f, 1.0f, frac); break;
        case AccentRow: s.accent = paintValue; break;
        case SlideRow:  s.slide = paintValue; break;
        case OnRow:     s.on = paintValue; break;
        default: break;
    }
    pattern.touch();
}

void StepGrid::mouseDown (const MouseEvent& e)
{
    dragRow = rowAt (e.y);
    if (dragRow == NoRow || e.x < labelWidth)
        return;
    auto& s = pattern.steps[(size_t) stepAt (columnAt (e.x))];
    if (dragRow == AccentRow) paintValue = ! s.accent.load();
    if (dragRow == SlideRow)  paintValue = ! s.slide.load();
    if (dragRow == OnRow)     paintValue = ! s.on.load();
    applyAt (e.getPosition());
}

void StepGrid::mouseDrag (const MouseEvent& e) { applyAt (e.getPosition()); }

void StepGrid::mouseDoubleClick (const MouseEvent& e)
{
    if (rowAt (e.y) == NoteRow && e.x >= labelWidth)
    {
        pattern.steps[(size_t) stepAt (columnAt (e.x))].degree = 0;
        pattern.touch();
    }
}

void StepGrid::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    const int n = visibleSteps();
    const int len = jlimit (1, SequencePattern::maxSteps, (int) length->load());
    const int current = processor.getSequencerStep();
    const bool arpMode = (int) mode->load() == 2;
    const float colW = (float) (getWidth() - labelWidth) / (float) n;
    const int r = (int) root->load(), sc = (int) scale->load();

    g.setColour (t.display);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);

    // rotulos das linhas
    static const char* labels[] { "NOTE", "VELOCITY", "GATE", "ACCENT", "SLIDE", "ON" };
    g.setFont (FontOptions (10.0f, Font::bold));
    for (int row = 0; row < NumRows; ++row)
    {
        auto a = rowArea (row);
        g.setColour (t.textDim);
        g.drawText (row == NoteRow && arpMode ? "NOTE (ARP)" : labels[row],
                    Rectangle<int> (6, a.getY(), labelWidth - 10, a.getHeight()), Justification::centredLeft);
        g.setColour (t.edge.withAlpha (0.5f));
        g.drawHorizontalLine (a.getBottom(), (float) labelWidth, (float) getWidth());
    }

    for (int i = 0; i < n; ++i)
    {
        const int si = stepAt (i);                       // indice real (pagina 33-64 soma 32)
        const auto& s = pattern.steps[(size_t) si];
        const float x = (float) labelWidth + (float) i * colW;
        const bool inRange = si < len;
        const bool on = s.on.load();
        const float alpha = inRange ? (on ? 1.0f : 0.35f) : 0.12f;

        // grupos de 4 passos (tempos)
        if (i % 4 == 0)
        {
            g.setColour (t.edge.withAlpha (i % 16 == 0 ? 0.9f : 0.5f));
            g.drawVerticalLine (roundToInt (x), 0.0f, (float) getHeight());
        }
        if (si == current && inRange)
        {
            g.setColour (t.accent.withAlpha (0.18f));
            g.fillRect (x, 0.0f, colW, (float) getHeight());
        }

        auto cell = [&] (int row) { auto a = rowArea (row); return Rectangle<float> (x + 2.0f, (float) a.getY() + 3.0f, colW - 4.0f, (float) a.getHeight() - 6.0f); };

        // NOTE: barra a partir da tonica (linha do zero)
        {
            auto c = cell (NoteRow);
            const int range = SequencePattern::maxDegree - SequencePattern::minDegree;
            auto yOf = [&] (int d) { return c.getBottom() - (float) (d - SequencePattern::minDegree) / (float) range * c.getHeight(); };
            const float y0 = yOf (0), y = yOf (s.degree.load());
            g.setColour (t.accent.withAlpha (0.25f * alpha * (arpMode ? 0.4f : 1.0f)));
            g.fillRect (Rectangle<float>::leftTopRightBottom (c.getX(), jmin (y, y0), c.getRight(), jmax (y, y0) + 1.0f));
            g.setColour (t.accent.withAlpha (alpha * (arpMode ? 0.4f : 1.0f)));
            g.fillRect (c.getX(), y - 2.0f, c.getWidth(), 4.0f);
            if (colW > 22.0f && inRange && ! arpMode)
            {
                const int note = degreeToNote (48 + r, s.degree.load(), r, sc);
                g.setFont (FontOptions (9.0f));
                g.setColour (t.text.withAlpha (alpha));
                g.drawText (noteNames()[note % 12], Rectangle<float> (c.getX(), c.getBottom() - 12.0f, c.getWidth(), 12.0f),
                            Justification::centred);
            }
            if (i == 0)
            {
                g.setColour (t.textDim.withAlpha (0.5f));
                g.drawHorizontalLine (roundToInt (y0), (float) labelWidth, (float) getWidth());
            }
        }

        auto bar = [&] (int row, float v)
        {
            auto c = cell (row);
            g.setColour (t.track.withAlpha (alpha));
            g.fillRect (c);
            g.setColour (t.accent.withAlpha (0.8f * alpha));
            g.fillRect (c.withTop (c.getBottom() - c.getHeight() * v));
        };
        bar (VelRow, s.accent.load() ? 1.0f : s.velocity.load());
        bar (GateRow, s.gate.load());

        auto toggle = [&] (int row, bool v)
        {
            auto c = cell (row).reduced (jmax (0.0f, (colW - 22.0f) * 0.5f), 1.0f);
            g.setColour (v ? t.accent.withAlpha (inRange ? 1.0f : 0.3f) : t.track.withAlpha (inRange ? 1.0f : 0.4f));
            g.fillRoundedRectangle (c, 2.0f);
        };
        toggle (AccentRow, s.accent.load());
        toggle (SlideRow, s.slide.load());
        toggle (OnRow, on);
    }
}

// ======================================================================== PAGE
SeqPage::SeqPage (MetalMachineAudioProcessor& p)
    : processor (p),
      mode (p.apvts, ParamIDs::seqMode, "Mode"), trigger (p.apvts, ParamIDs::seqTrigger, "Trigger"),
      rate (p.apvts, ParamIDs::seqRate, "Rate"), feel (p.apvts, ParamIDs::timeFeel, "Time Feel"),
      length (p.apvts, ParamIDs::seqLength, "Length"), gate (p.apvts, ParamIDs::seqGate, "Gate"),
      swing (p.apvts, ParamIDs::seqSwing, "Swing"), transpose (p.apvts, ParamIDs::seqTranspose, "Transp", true),
      root (p.apvts, ParamIDs::keyRoot, "Root"), scale (p.apvts, ParamIDs::keyScale, "Scale"),
      octave (p.apvts, ParamIDs::seqOctave, "Octave", true),
      arpMode (p.apvts, ParamIDs::arpMode, "Mode"), arpOctaves (p.apvts, ParamIDs::arpOctaves, "Octaves"),
      humVel (p.apvts, ParamIDs::humanizeVel, "Vel"), humTime (p.apvts, ParamIDs::humanizeTime, "Timing"),
      tempoSource (p.apvts, ParamIDs::tempoSource, "Source"), bpm (p.apvts, ParamIDs::internalBpm, "BPM"),
      grid (p)
{
    for (auto* panel : { &seqPanel, &keyPanel, &arpPanel, &humPanel, &tempoPanel, &gridPanel })
        addAndMakeVisible (panel);

    for (auto* c : std::initializer_list<Component*> { &swing, &transpose, &velRandomButton })
        addAndMakeVisible (c);
    for (auto* b : { &page1Button, &page2Button })
        addChildComponent (b);   // estado de pagina e controlado pelo timer (setToggleState)
    velRandomButton.setTooltip ("Sorteia so as velocities (tempos fortes mais altos)");
    velRandomButton.onClick = [this] { processor.randomizeVelocities(); };
    page1Button.onClick = [this] { grid.setPage (0); };
    page2Button.onClick = [this] { grid.setPage (1); };

    for (auto* c : std::initializer_list<Component*> { &mode, &trigger, &rate, &feel, &length, &gate, &root, &scale,
                                                       &octave, &lockButton, &arpMode, &arpOctaves, &humVel, &humTime,
                                                       &tempoSource, &bpm, &patternBox, &randomButton, &clearButton,
                                                       &midiOutButton, &tempoPresetButton, &grid })
        addAndMakeVisible (c);

    lockButton.setClickingTogglesState (true);
    lockButton.setTooltip ("SCALE LOCK: as notas do teclado sao corrigidas para a escala");
    lockAttachment = std::make_unique<APVTS::ButtonAttachment> (p.apvts, ParamIDs::scaleLock, lockButton);

    midiOutButton.setClickingTogglesState (true);
    midiOutButton.setTooltip ("Envia as notas geradas pelo sequencer/arp para o REAPER (gravar no piano roll)");
    midiOutAttachment = std::make_unique<APVTS::ButtonAttachment> (p.apvts, ParamIDs::midiOut, midiOutButton);

    // biblioteca de padroes agrupada por secao da musica
    String section;
    const auto& lib = getPatternLibrary();
    for (int i = 0; i < (int) lib.size(); ++i)
    {
        if (section != lib[(size_t) i].section)
        {
            section = lib[(size_t) i].section;
            patternBox.addSectionHeading (section);
        }
        patternBox.addItem (lib[(size_t) i].name, i + 1);
    }
    patternBox.setTextWhenNothingSelected ("PATTERN LIBRARY");
    patternBox.onChange = [this]
    {
        if (const int id = patternBox.getSelectedId(); id > 0)
            processor.loadLibraryPattern (id - 1);
    };

    randomButton.setTooltip ("Gera um padrao novo dentro da escala (tonica forte, acentos nos tempos)");
    randomButton.onClick = [this] { processor.randomizeSequence(); };
    clearButton.onClick  = [this] { processor.clearSequence(); };
    tempoPresetButton.setTooltip ("BPM interno (usado quando TEMPO SOURCE = Internal)");
    tempoPresetButton.onClick = [this] { showTempoPresets(); };

    startTimerHz (5);
}

void SeqPage::showTempoPresets()
{
    PopupMenu m;
    m.addSectionHeader ("TEMPO INTERNO (ativa Source = Internal)");
    for (int b = 120; b <= 180; b += 5)
        m.addItem (String (b) + " BPM", [this, b]
        {
            processor.setParamReal (ParamIDs::internalBpm, (float) b);
            processor.setParamReal (ParamIDs::tempoSource, 1.0f);
        });
    m.addSeparator();
    m.addItem ("Voltar a seguir o REAPER (Host)", [this] { processor.setParamReal (ParamIDs::tempoSource, 0.0f); });
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&tempoPresetButton));
}

void SeqPage::timerCallback()
{
    const auto rev = processor.getPattern().revision.load();
    if (rev != lastRevision)
    {
        lastRevision = rev;
        patternBox.setText (processor.getPattern().name.toUpperCase(), dontSendNotification);
    }

    // paginas da grade: so existem quando LENGTH > 32
    const bool paged = (int) processor.apvts.getRawParameterValue (ParamIDs::seqLength)->load() > 32;
    if (! paged && grid.getPage() != 0)
        grid.setPage (0);
    page1Button.setVisible (paged);
    page2Button.setVisible (paged);
    page1Button.setToggleState (grid.getPage() == 0, dontSendNotification);
    page2Button.setToggleState (grid.getPage() == 1, dontSendNotification);
}

void SeqPage::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    g.setFont (FontOptions (10.0f));
    g.setColour (t.textDim);
    g.drawText ("SEGURE UMA TECLA  |  ARRASTE NA GRADE",
                barArea.withLeft (page2Button.getRight() + 12).withRight (midiOutButton.getX() - 12)
                       .withRight (midiOutButton.getX() - 10),
                Justification::centred);
}

void SeqPage::resized()
{
    constexpr int gap = 8;
    auto r = getLocalBounds();
    auto top = r.removeFromTop (132);
    r.removeFromTop (gap);
    barArea = r.removeFromTop (28);
    r.removeFromTop (gap);
    gridPanel.setBounds (r);
    grid.setBounds (gridPanel.getContentArea().translated (gridPanel.getX(), gridPanel.getY()));

    auto place = [] (Panel& panel, Rectangle<int> bounds, std::initializer_list<Component*> items)
    {
        panel.setBounds (bounds);
        Panel::layoutRow (panel.getContentArea().translated (bounds.getX(), bounds.getY()), items);
    };

    const int total = top.getWidth() - 4 * gap;
    place (seqPanel,   top.removeFromLeft (total * 36 / 100), { &mode, &trigger, &rate, &feel, &length, &gate });
    top.removeFromLeft (gap);
    place (keyPanel,   top.removeFromLeft (total * 29 / 100), { &root, &scale, &octave, &transpose, &lockButton });
    top.removeFromLeft (gap);
    place (arpPanel,   top.removeFromLeft (total * 11 / 100), { &arpMode, &arpOctaves });
    top.removeFromLeft (gap);
    place (humPanel,   top.removeFromLeft (total * 13 / 100), { &swing, &humVel, &humTime });
    top.removeFromLeft (gap);
    place (tempoPanel, top, { &tempoSource, &bpm });

    // botao SCALE LOCK com altura de botao, centralizado na coluna dele
    lockButton.setBounds (lockButton.getBounds().withSizeKeepingCentre (lockButton.getWidth(), 26));

    auto bar = barArea;
    patternBox.setBounds (bar.removeFromLeft (260));
    bar.removeFromLeft (6);
    randomButton.setBounds (bar.removeFromLeft (120));
    bar.removeFromLeft (6);
    clearButton.setBounds (bar.removeFromLeft (70));
    bar.removeFromLeft (6);
    velRandomButton.setBounds (bar.removeFromLeft (100));
    bar.removeFromLeft (14);
    page1Button.setBounds (bar.removeFromLeft (56));
    bar.removeFromLeft (2);
    page2Button.setBounds (bar.removeFromLeft (56));
    tempoPresetButton.setBounds (bar.removeFromRight (120));
    bar.removeFromRight (6);
    midiOutButton.setBounds (bar.removeFromRight (90));
}
} // namespace mm
