#include "SongPage.h"
#include "Hardware.h"

using namespace juce;

namespace mm
{
// ====================================================================== TIMELINE
void SongTimeline::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    auto r = getLocalBounds().toFloat();
    hw::inset (g, r, t, 3.0f);
    const auto& song = processor.getSong();
    if (! song.valid || song.totalBars == 0)
    {
        hw::micro (g, "Clique em GENERATE EVERYTHING para criar a ideia musical", getLocalBounds(), Justification::centred, t.textDim, 11.0f);
        return;
    }
    const float x0 = (float) labelW, w = (float) getWidth() - x0 - 6.0f;
    const float barW = w / (float) song.totalBars;
    const float laneH = ((float) getHeight() - headerH - 6.0f) / (float) Song::NumTracks;

    // secoes: bloco + curva de energia
    for (size_t i = 0; i < song.sections.size(); ++i)
    {
        const auto& sec = song.sections[i];
        Rectangle<float> blk (x0 + sec.startBar * barW + 1.0f, 4.0f, sec.bars * barW - 2.0f, (float) headerH - 8.0f);
        const bool sel = (int) i == selected;
        g.setColour (sel ? t.accent.withAlpha (0.25f) : t.panelRaised.withAlpha (0.8f));
        g.fillRoundedRectangle (blk, 2.0f);
        g.setColour (sel ? t.accent : Colours::black);
        g.drawRoundedRectangle (blk, 2.0f, sel ? 1.4f : 1.0f);
        auto eb = blk.withTrimmedTop (blk.getHeight() * (1.0f - sec.energy * 0.45f)).reduced (2.0f, 2.0f);
        g.setColour (t.accent.withAlpha (0.25f + 0.5f * sec.energy));
        g.fillRect (eb.withY (blk.getBottom() - 6.0f).withHeight (3.0f).withWidth (eb.getWidth() * sec.energy));
        g.setColour (sel ? t.text : t.text.withAlpha (0.8f));
        g.setFont (FontOptions (9.5f, Font::bold).withKerningFactor (0.05f));
        g.drawText (Song::secNames()[sec.type] + (sec.repeat > 0 ? " " + String (sec.repeat + 1) : ""),
                    blk.reduced (4.0f, 3.0f).withTrimmedBottom (8.0f).toNearestInt(), Justification::topLeft, true);
        hw::micro (g, String (sec.bars) + " BARS  E" + String (roundToInt (sec.energy * 100)), blk.reduced (4.0f, 3.0f).toNearestInt(),
                   Justification::bottomRight, t.textDim, 8.0f);
    }
    // trilhas: mini piano-roll (densidade de notas por compasso)
    for (int tr = 0; tr < Song::NumTracks; ++tr)
    {
        const float y = (float) headerH + tr * laneH;
        hw::micro (g, Song::trackNames()[tr], { 6, (int) y, labelW - 8, (int) laneH }, Justification::centredLeft,
                   tr == Song::B808 || tr == Song::Melody || tr == Song::Riff ? t.accentSoft : t.textDim, 8.5f);
        g.setColour (Colours::white.withAlpha (tr % 2 ? 0.0f : 0.02f));
        g.fillRect (x0, y, w, laneH);
        const auto& notes = song.tracks[(size_t) tr];
        int lo = 127, hi = 0; for (auto& n : notes) { lo = jmin (lo, n.note); hi = jmax (hi, n.note); }
        const float ticksToX = barW / (float) SongGenerator::barTicks;
        for (auto& n : notes)
        {
            const float nx = x0 + n.tick * ticksToX;
            const float ny = hi > lo ? y + laneH - 2.0f - (float) (n.note - lo) / (float) (hi - lo) * (laneH - 4.0f) : y + laneH * 0.5f;
            g.setColour (t.accent.withAlpha (0.35f + 0.5f * n.vel / 127.0f));
            g.fillRect (nx, ny - 0.8f, jmax (1.0f, n.len * ticksToX), 1.6f);
        }
    }
    g.setColour (Colours::black.withAlpha (0.6f));
    for (const auto& sec : song.sections)
        g.drawVerticalLine ((int) (x0 + sec.startBar * barW), (float) headerH, (float) getHeight() - 4.0f);
}

void SongTimeline::mouseDown (const MouseEvent& e)
{
    const auto& song = processor.getSong();
    if (! song.valid || song.totalBars == 0) return;
    const float barW = ((float) getWidth() - labelW - 6.0f) / (float) song.totalBars;
    const int bar = (int) ((e.x - labelW) / barW);
    for (size_t i = 0; i < song.sections.size(); ++i)
        if (bar >= song.sections[i].startBar && bar < song.sections[i].startBar + song.sections[i].bars)
        {
            selected = (int) i;
            if (onSelect) onSelect ((int) i);
            repaint();
            return;
        }
}

// ====================================================================== PAGE
SongPage::SongPage (MetalMachineAudioProcessor& p)
    : processor (p),
      style (p.apvts, "song_style", "Style"), mood (p.apvts, "song_mood", "Mood"), structure (p.apvts, "song_structure", "Structure"),
      heaviness (p.apvts, "song_heaviness", "Heavy"), electronic (p.apvts, "song_electronic", "Electr"),
      trap (p.apvts, "song_trap", "Trap"), darkness (p.apvts, "song_darkness", "Dark"), melodic (p.apvts, "song_melodic", "Melodic"),
      modernity (p.apvts, "song_modernity", "Modern"), texture (p.apvts, "song_texture", "Texture"), fx (p.apvts, "song_fx", "FX"),
      dragButton (p), timeline (p)
{
    vocalButton.setClickingTogglesState (true);
    vocalAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, "song_vocalspace", vocalButton);
    quickButton.setClickingTogglesState (true);
    quickAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, "song_quick", quickButton);
    generateButton.setTooltip ("Gera a ideia completa: bateria, 808, baixo, riff, melodia, arp, pad, FX e arranjo (seed nova)");
    generateButton.onClick = [this] { processor.generateSong (true); timeline.selected = -1; refresh(); };
    inspireButton.setTooltip ("PRODUCER MODE: escolhe estilo, clima e KEY coerentes, carrega um synth que combina, gera o beat e a musica");
    inspireButton.onClick = [this]
    {
        const auto summary = processor.inspireMe();
        timeline.selected = -1;
        refresh();
        info = "INSPIRE ME: " + summary;
        repaint();
    };
    regenButton.setTooltip ("Gera de novo com a MESMA seed (use depois de mudar o DNA ou as travas)");
    regenButton.onClick = [this] { processor.generateSong (false); refresh(); };
    seedButton.setTooltip ("Seed nova: ideia completamente diferente (elementos travados ficam)");
    seedButton.onClick = [this] { processor.generateSong (true); refresh(); };
    exportButton.setTooltip ("MIDI multitrilha: 12 trilhas + marcadores das secoes, em Documentos/ATDT Synth Machine/MIDI");
    exportButton.onClick = [this]
    {
        const auto f = processor.exportSongMidi (false);
        if (f.existsAsFile())
        {
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "EXPORT SONG MIDI",
                "MIDI salvo:\n" + f.getFullPathName() + "\n\nArraste para o REAPER: ele cria as 12 trilhas e os marcadores das secoes."
                "\nFX: C3 riser, D3 impact, E3 sub drop, F3 downlifter, G3 stutter, A3 reverse, B3 textura.");
            f.revealToUser();
        }
    };
    static const char* lockNames[] { "DRUMS", "808", "BASS", "RIFF", "MELODY", "ARP", "PAD", "FX" };
    for (int i = 0; i < Song::NumLocks; ++i)
    {
        auto* b = lockButtons.add (new TextButton (lockNames[i]));
        b->setClickingTogglesState (true);
        b->setToggleState ((processor.getSongLocks() >> i) & 1u, dontSendNotification);
        b->setTooltip ("Trava: REGENERATE / NEW SEED nao altera este elemento");
        b->onClick = [this, i, b]
        {
            const auto m = processor.getSongLocks();
            processor.setSongLocks (b->getToggleState() ? (m | (1u << i)) : (m & ~(1u << i)));
        };
        addAndMakeVisible (b);
    }
    timeline.onSelect = [this] (int i)
    {
        processor.previewSongSection (i);
        const auto& sec = processor.getSong().sections[(size_t) i];
        info = "PREVIEW: " + Song::secNames()[sec.type] + " carregado (DRUMS + 808 + SYNTH RIFF no SEQ). De PLAY no REAPER.";
        repaint();
    };
    for (auto* c : std::initializer_list<Component*> { &dnaPanel, &arrangePanel, &style, &mood, &structure, &heaviness, &electronic,
                                                       &trap, &darkness, &melodic, &modernity, &texture, &fx, &vocalButton, &quickButton,
                                                       &generateButton, &regenButton, &seedButton, &exportButton, &dragButton, &timeline, &inspireButton })
        addAndMakeVisible (c);
    for (auto* b : lockButtons) b->toFront (false);   // os paineis foram adicionados depois: cadeados por cima
    // RECALL: a ideia e deterministica (DNA + seed salvos no projeto) -> reconstruida ao reabrir
    if (! processor.getSong().valid && processor.apvts.state.hasProperty ("songSeed"))
        processor.generateSong (false);
    startTimerHz (4);
    refresh();
}

void SongPage::refresh()
{
    const auto& song = processor.getSong();
    if (song.valid)
    {
        int notes = 0; for (auto& t : song.tracks) notes += (int) t.size();
        static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        info = String (song.totalBars) + " compassos | " + String ((int) song.sections.size()) + " secoes | " + String (notes) + " notas | "
             + names[song.settings.root % 12] + " " + scaleNames()[song.settings.scale] + " | " + String (roundToInt (song.settings.bpm)) + " BPM"
             + " | clique numa secao para ouvir";
    }
    timeline.repaint();
    repaint();
}

void SongPage::timerCallback()
{
    for (int i = 0; i < lockButtons.size(); ++i)
        lockButtons[i]->setToggleState ((processor.getSongLocks() >> i) & 1u, dontSendNotification);
}

void SongPage::paintOverChildren (Graphics& g)
{
    const auto& t = themeOf (*this);
    auto sa = seedArea;   // copia: desenhar nao pode alterar o layout
    hw::micro (g, "Seed", sa.removeFromTop (14), Justification::centred, t.textDim, 9.0f);
    g.setColour (t.accentSoft);
    g.setFont (FontOptions (15.0f, Font::bold).withKerningFactor (0.08f));
    g.drawText (String (processor.getSongSeed()), sa, Justification::centred);
    hw::micro (g, "Lock", lockArea, Justification::centredLeft, t.accentSoft, 9.5f);
    hw::micro (g, info, infoArea, Justification::centredLeft, t.textDim, 9.0f);
}

void SongPage::resized()
{
    constexpr int gap = 6;
    auto r = getLocalBounds();
    auto top = r.removeFromTop (196);
    r.removeFromTop (gap);
    dnaPanel.setBounds (top);
    arrangePanel.setBounds (r);
    auto ac = arrangePanel.getContentArea().translated (r.getX(), r.getY());
    infoArea = ac.removeFromTop (16).withTrimmedLeft (4);
    timeline.setBounds (ac);

    auto c = dnaPanel.getContentArea().translated (top.getX(), top.getY()).reduced (4, 0);
    auto take = [&] (Rectangle<int>& row, int w) { auto a = row.removeFromLeft (w); row.removeFromLeft (gap); return a; };
    auto l1 = c.removeFromTop (44);
    style.setBounds (take (l1, 180));
    mood.setBounds (take (l1, 120));
    structure.setBounds (take (l1, 110));
    vocalButton.setBounds (take (l1, 110).withTrimmedTop (14));
    quickButton.setBounds (take (l1, 100).withTrimmedTop (14));
    seedArea = take (l1, 80);
    seedButton.setBounds (take (l1, 90).withTrimmedTop (14));
    regenButton.setBounds (take (l1, 100).withTrimmedTop (14));
    inspireButton.setBounds (l1.removeFromRight (120).withTrimmedTop (14));
    l1.removeFromRight (gap);
    generateButton.setBounds (l1.withTrimmedTop (14));
    c.removeFromTop (4);
    auto l2 = c.removeFromTop (78);
    for (auto* k : { &heaviness, &electronic, &trap, &darkness, &melodic, &modernity, &texture, &fx })
        k->setBounds (take (l2, 70));
    c.removeFromTop (4);
    auto l3 = c.removeFromTop (28);
    dragButton.setBounds (l3.removeFromRight (150)); l3.removeFromRight (gap);
    exportButton.setBounds (l3.removeFromRight (150)); l3.removeFromRight (gap * 3);
    lockArea = l3.removeFromLeft (44);
    const int lw = (l3.getWidth() - gap * (lockButtons.size() - 1)) / jmax (1, lockButtons.size());
    for (auto* b : lockButtons) { b->setBounds (l3.removeFromLeft (lw)); l3.removeFromLeft (gap); }
    // o espaco ao lado dos knobs mostra a ajuda do fluxo
}
} // namespace mm
