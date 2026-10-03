#include "ScratchPage.h"
#include "Hardware.h"

using namespace juce;

namespace mm
{
ScratchPage::ScratchPage (MetalMachineAudioProcessor& p)
    : processor (p), pad (p),
      pattern (p.apvts, "scr_move", "Pattern"), rate (p.apvts, "scr_rate", "Rate"), trigMode (p.apvts, "scr_mode", "Mode"),
      source (p.apvts, "scr_source", "Synth Source"), direction (p.apvts, "scr_direction", "Direction"),
      intensity (p.apvts, "scr_intensity", "Intensity")
{
    onButton.setClickingTogglesState (true);
    onAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, "scr_on", onButton);
    holdButton.setClickingTogglesState (true);
    holdAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, "scr_hold", holdButton);
    loopButton.setClickingTogglesState (true);
    loopAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, "scr_loop", loopButton);
    synthMode.onClick = [this] { setReal ("scr_engine", 0.0f); };
    sampleMode.onClick = [this] { setReal ("scr_engine", 1.0f); };
    trigButton.setTooltip ("Dispara o padrao selecionado");
    trigButton.onClick = [this] { setReal ("scr_on", 1.0f); processor.getScratch().uiTrigger = (int) processor.apvts.getRawParameterValue ("scr_move")->load(); };
    releaseButton.setTooltip ("Para o golpe atual");
    releaseButton.onClick = [this] { processor.getScratch().uiRelease = true; };
    loadButton.setTooltip ("Carrega o seu WAV/AIFF/FLAC/MP3 (ate 30 s) e liga o modo SAMPLE");
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> ("Sample para o Scratch Machine", File::getSpecialLocation (File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.mp3");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
        {
            if (! fc.getResult().existsAsFile()) return;
            String err;
            if (processor.loadScratchSample (fc.getResult(), err)) { setReal ("scr_engine", 1.0f); message = "Sample: " + fc.getResult().getFileName(); }
            else message = "Erro: " + err;
        });
    };
    struct K { const char* id; const char* name; };
    for (auto k : { K { "scr_swing", "Swing" }, K { "scr_repeat", "Repeat" }, K { "scr_variation", "Variation" } })
        addAndMakeVisible (patKnobs.add (new Knob (p.apvts, k.id, k.name)));
    for (auto k : { K { "scr_speed", "Speed" }, K { "scr_depth", "Depth" }, K { "scr_curve", "Curve" }, K { "scr_attack", "Attack" },
                    K { "scr_release", "Release" }, K { "scr_gate", "Gate" }, K { "scr_resonance", "Reso" }, K { "scr_tone", "Tone" },
                    K { "scr_noise", "Noise" }, K { "scr_vinyl", "Vinyl" }, K { "scr_pitch", "Pitch" }, K { "scr_width", "Width" },
                    K { "scr_momentum", "Momentum" }, K { "scr_accel", "Accel" }, K { "scr_decel", "Decel" }, K { "scr_random", "Random" } })
        addAndMakeVisible (engKnobs.add (new Knob (p.apvts, k.id, k.name, String (k.id) == "scr_pitch")));
    for (auto k : { K { "scr_start", "Start" }, K { "scr_end", "End" }, K { "scr_center", "Center" }, K { "scr_fadein", "Fade In" },
                    K { "scr_fadeout", "Fade Out" } })
        addAndMakeVisible (srcKnobs.add (new Knob (p.apvts, k.id, k.name)));
    for (auto k : { K { "scr_fx_filter", "Filter" }, K { "scr_fx_drive", "Drive" }, K { "scr_fx_sat", "Satur" }, K { "scr_fx_crush", "Crush" },
                    K { "scr_fx_delay", "Delay" }, K { "scr_fx_reverb", "Reverb" }, K { "scr_fx_width", "Stereo" }, K { "scr_fx_vinyl", "Vinyl" },
                    K { "scr_fx_mix", "FX Mix" }, K { "scr_master", "Master" }, K { "scr_level", "Level" } })
        addAndMakeVisible (fxKnobs.add (new Knob (p.apvts, k.id, k.name, String (k.id) == "scr_master")));
    static const char* qn[] { "BABY", "CHIRP", "FLARE", "TRANSFORM", "CRAB", "REVERSE", "STAB", "BRAKE" };
    for (int i = 0; i < 8; ++i)
    {
        auto* b = quickButtons.add (new TextButton (qn[i]));
        b->setTooltip ("Dispara agora (tecla " + String (i == 0 ? "C#4" : StringArray { "C#4", "D4", "D#4", "E4", "F4", "F#4", "G4", "G#4" }[i]) + ")");
        b->onClick = [this, i] { setReal ("scr_on", 1.0f); processor.getScratch().uiTrigger = ScratchEngine::quick[i]; };
        addAndMakeVisible (b);
    }
    for (auto n : { "PATTERN", "MOTION", "TONE", "FX" })
    {
        auto* b = lockButtons.add (new TextButton (n));
        b->setClickingTogglesState (true);
        b->setTooltip ("Trava: RANDOM SCRATCH nao altera este grupo");
        addAndMakeVisible (b);
    }
    randomButton.onClick = [this] { randomScratch(); };
    for (auto* c : std::initializer_list<Component*> { &padPanel, &patPanel, &engPanel, &srcPanel, &fxPanel, &quickPanel, &randPanel,
                                                       &pad, &onButton, &synthMode, &sampleMode, &loadButton, &trigButton, &holdButton,
                                                       &releaseButton, &loopButton, &randomButton, &pattern, &rate, &trigMode, &source,
                                                       &direction, &intensity })
        addAndMakeVisible (c);
    for (auto* arr : { &patKnobs, &engKnobs, &srcKnobs, &fxKnobs }) for (auto* k : *arr) k->toFront (false);
    for (auto* arr : { &quickButtons, &lockButtons }) for (auto* b : *arr) b->toFront (false);
    startTimerHz (8);
}

void ScratchPage::setReal (const char* id, float v)
{
    if (auto* prm = processor.apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

void ScratchPage::timerCallback()
{
    const bool sampleOn = processor.apvts.getRawParameterValue ("scr_engine")->load() > 0.5f;
    synthMode.setToggleState (! sampleOn, dontSendNotification);
    sampleMode.setToggleState (sampleOn, dontSendNotification);
    source.setAlpha (sampleOn ? 0.45f : 1.0f);
    for (auto* k : srcKnobs) k->setAlpha (1.0f);
    repaint (headerArea);
}

// RANDOM SCRATCH: musical (faixas uteis por grupo) e respeitando as travas
void ScratchPage::randomScratch()
{
    auto& r = Random::getSystemRandom();
    auto rnd = [&r] (float a, float b) { return a + (b - a) * r.nextFloat(); };
    auto locked = [this] (int i) { return lockButtons[i]->getToggleState(); };
    if (! locked (0))
    {
        static const int musical[] { 0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 16, 17 };
        setReal ("scr_move", (float) musical[r.nextInt ((int) std::size (musical))]);
        static const int rates[] { 0, 1, 2, 6, 7, 8 };
        setReal ("scr_rate", (float) rates[r.nextInt ((int) std::size (rates))]);
        setReal ("scr_swing", r.nextFloat() < 0.3f ? rnd (0.2f, 0.5f) : 0.0f);
        setReal ("scr_repeat", r.nextFloat() < 0.25f ? 2.0f : 1.0f);
        setReal ("scr_variation", rnd (0.2f, 0.9f));
    }
    if (! locked (1))
    {
        setReal ("scr_speed", rnd (0.7f, 1.8f)); setReal ("scr_depth", rnd (0.4f, 0.9f)); setReal ("scr_curve", rnd (0.0f, 0.7f));
        setReal ("scr_momentum", rnd (0.0f, 0.6f)); setReal ("scr_accel", rnd (0.0f, 0.5f)); setReal ("scr_decel", rnd (0.0f, 0.5f));
        setReal ("scr_random", rnd (0.0f, 0.5f));
    }
    if (! locked (2))
    {
        setReal ("scr_tone", rnd (0.55f, 1.0f)); setReal ("scr_resonance", rnd (0.0f, 0.5f)); setReal ("scr_gate", r.nextFloat() < 0.3f ? rnd (0.2f, 0.6f) : 0.0f);
        setReal ("scr_noise", rnd (0.0f, 0.3f)); setReal ("scr_vinyl", rnd (0.0f, 0.5f));
        static const float steps[] { -12, -7, -5, 0, 0, 0, 5, 7 };
        setReal ("scr_pitch", steps[r.nextInt ((int) std::size (steps))]);
    }
    if (! locked (3))
    {
        setReal ("scr_fx_drive", r.nextFloat() < 0.4f ? rnd (0.2f, 0.7f) : 0.0f); setReal ("scr_fx_sat", rnd (0.0f, 0.4f));
        setReal ("scr_fx_crush", r.nextFloat() < 0.2f ? rnd (0.1f, 0.5f) : 0.0f); setReal ("scr_fx_delay", r.nextFloat() < 0.35f ? rnd (0.1f, 0.3f) : 0.0f);
        setReal ("scr_fx_reverb", rnd (0.0f, 0.25f)); setReal ("scr_fx_width", rnd (0.4f, 0.9f));
    }
    setReal ("scr_on", 1.0f);
    processor.getScratch().uiTrigger = (int) processor.apvts.getRawParameterValue ("scr_move")->load();
    message = "RANDOM: " + ScratchEngine::moveNames()[(int) processor.apvts.getRawParameterValue ("scr_move")->load()];
}

void ScratchPage::resized()
{
    constexpr int gap = 6;
    auto r = getLocalBounds();
    headerArea = r.removeFromTop (34);
    {
        auto h = headerArea.reduced (4, 3);
        onButton.setBounds (h.removeFromRight (56));
        h.removeFromRight (8);
        sampleMode.setBounds (h.removeFromRight (84));
        synthMode.setBounds (h.removeFromRight (84));
    }
    r.removeFromTop (4);
    auto rowA = r.removeFromTop (262); r.removeFromTop (gap);
    auto rowB = r.removeFromTop (128); r.removeFromTop (gap);
    auto rowC = r.removeFromTop (128); r.removeFromTop (gap);
    auto rowD = r;

    // A: SCRATCH PAD + PATTERN
    auto patB = rowA.removeFromRight (360); rowA.removeFromRight (gap);
    padPanel.setBounds (rowA); patPanel.setBounds (patB);
    pad.setBounds (padPanel.getContentArea().translated (rowA.getX(), rowA.getY()).reduced (4, 2));
    {
        auto c = patPanel.getContentArea().translated (patB.getX(), patB.getY()).reduced (6, 2);
        auto l1 = c.removeFromTop (46);
        const int w = (l1.getWidth() - 2 * gap) / 3;
        pattern.setBounds (l1.removeFromLeft (w)); l1.removeFromLeft (gap);
        rate.setBounds (l1.removeFromLeft (w)); l1.removeFromLeft (gap);
        trigMode.setBounds (l1);
        c.removeFromTop (4);
        auto l2 = c.removeFromTop (92);
        const int kw = l2.getWidth() / 3;
        for (auto* k : patKnobs) k->setBounds (l2.removeFromLeft (kw));
        c.removeFromTop (6);
        auto l3 = c.removeFromTop (30);
        const int bw = (l3.getWidth() - 3 * gap) / 4;
        for (auto* b : { &trigButton, &holdButton, &releaseButton, &loopButton }) { b->setBounds (l3.removeFromLeft (bw)); l3.removeFromLeft (gap); }
    }
    // B: ENGINE (16 knobs + direcao)
    engPanel.setBounds (rowB);
    {
        auto c = engPanel.getContentArea().translated (rowB.getX(), rowB.getY()).reduced (4, 0);
        direction.setBounds (c.removeFromLeft (100).withSizeKeepingCentre (100, 44));
        c.removeFromLeft (gap);
        const int kw = c.getWidth() / engKnobs.size();
        for (auto* k : engKnobs) k->setBounds (c.removeFromLeft (kw));
    }
    // C: SOURCE + FX
    auto srcB = rowC.removeFromLeft (rowC.getWidth() * 40 / 100); rowC.removeFromLeft (gap);
    srcPanel.setBounds (srcB); fxPanel.setBounds (rowC);
    {
        auto c = srcPanel.getContentArea().translated (srcB.getX(), srcB.getY()).reduced (4, 0);
        auto col = c.removeFromLeft (120);
        source.setBounds (col.removeFromTop (46));
        loadButton.setBounds (col.removeFromTop (30).reduced (0, 2));
        c.removeFromLeft (gap);
        const int kw = c.getWidth() / srcKnobs.size();
        for (auto* k : srcKnobs) k->setBounds (c.removeFromLeft (kw));
    }
    {
        auto c = fxPanel.getContentArea().translated (rowC.getX(), rowC.getY()).reduced (4, 0);
        const int kw = c.getWidth() / fxKnobs.size();
        for (auto* k : fxKnobs) k->setBounds (c.removeFromLeft (kw));
    }
    // D: QUICK SCRATCH + RANDOM
    auto randB = rowD.removeFromRight (380); rowD.removeFromRight (gap);   // QUICK mais largo: "TRANSFORM" cabe inteiro
    quickPanel.setBounds (rowD); randPanel.setBounds (randB);
    {
        auto c = quickPanel.getContentArea().translated (rowD.getX(), rowD.getY()).reduced (6, 4);
        intensity.setBounds (c.removeFromRight (84));
        c.removeFromRight (gap);
        const int bw = (c.getWidth() - 7 * gap) / 8;
        for (auto* b : quickButtons) { b->setBounds (c.removeFromLeft (bw)); c.removeFromLeft (gap); }
    }
    {
        auto c = randPanel.getContentArea().translated (randB.getX(), randB.getY()).reduced (6, 4);
        randomButton.setBounds (c.removeFromLeft (140));
        c.removeFromLeft (gap);
        const int lw = (c.getWidth() - 3 * gap) / 4;
        for (auto* b : lockButtons) { b->setBounds (c.removeFromLeft (lw).withSizeKeepingCentre (lw, 30)); c.removeFromLeft (gap); }
    }
}

void ScratchPage::paintOverChildren (Graphics& g)
{
    const auto& t = themeOf (*this);
    auto h = headerArea.reduced (8, 0);
    hw::plate (g, headerArea.toFloat().withTrimmedRight (240.0f), t, 6.0f, false);
    g.setColour (t.text);
    g.setFont (FontOptions (17.0f, Font::bold).withKerningFactor (0.08f));
    g.drawText ("SCRATCH MACHINE", h.removeFromLeft (200), Justification::centredLeft);
    hw::micro (g, "Vinyl // Sample // Synth // FX", h.removeFromLeft (250), Justification::centredLeft, t.textDim, 9.5f);
    const bool on = processor.apvts.getRawParameterValue ("scr_on")->load() > 0.5f;
    auto led = h.removeFromLeft (240);
    hw::led (g, { (float) led.getX() + 6.0f, (float) led.getCentreY() }, 3.5f, on ? 1.0f : 0.0f, t);
    hw::micro (g, on ? "Scratch engine // ready" : "Scratch engine // standby", led.withTrimmedLeft (16), Justification::centredLeft, on ? t.accentSoft : t.textDim, 9.0f);
    hw::micro (g, message.isNotEmpty() ? message : String ("Keys: C4 = pattern | C#4-G#4 = quick"), h.withTrimmedRight (240), Justification::centredLeft, t.textDim, 8.5f);
}
} // namespace mm
