#include "MainView.h"
#include "Hardware.h"

using namespace juce;

namespace mm
{
// ======================================================================== PRESET BOX
void PresetBox::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    auto r = getLocalBounds().toFloat();
    g.setColour (t.display);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (hover ? t.accent : t.edge.brighter (0.2f));
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);

    auto area = getLocalBounds().reduced (9, 0);
    const auto cat = manager.getCurrentCategory();
    g.setFont (FontOptions (10.0f));
    g.setColour (t.accentSoft);
    if (cat.isNotEmpty())
        g.drawText (cat, area.removeFromRight (70), Justification::centredRight);
    g.setFont (FontOptions (12.0f, Font::bold));
    g.setColour (t.text);
    g.drawText (manager.getCurrentName(), area, Justification::centredLeft);
}

// ======================================================================== HEADER
HeaderBar::HeaderBar (MetalMachineAudioProcessor& p) : processor (p), presetBox (p.getPresetManager())
{
    presetBox.onClick = [this] { if (onPresetClicked) onPresetClicked(); };
    prevButton.setTooltip ("Preset anterior");
    nextButton.setTooltip ("Proximo preset");
    prevButton.onClick = [this] { processor.getPresetManager().step (-1); };
    nextButton.onClick = [this] { processor.getPresetManager().step (+1); };

    initButton.setTooltip ("Volta todos os parametros ao patch inicial");
    initButton.onClick = [this]
    {
        processor.resetToInitPatch();
        processor.getPresetManager().markInit();
    };
    saveButton.setTooltip ("Salva por cima do preset do usuario atual (ou pede um nome)");
    saveButton.onClick = [this] { save(); };
    saveAsButton.setTooltip ("Salva como novo preset do usuario");
    saveAsButton.onClick = [this] { showSaveAsDialog(); };
    settingsButton.setTooltip ("Escala da interface e tema");
    settingsButton.onClick = [this] { showSettingsMenu(); };

    randomButton.setTooltip ("Preset aleatorio da mesma categoria");
    randomButton.onClick = [this]
    {
        auto& pm = processor.getPresetManager();
        auto list = pm.filter (pm.getCurrentCategory().isNotEmpty() ? pm.getCurrentCategory() : String ("ALL"), {});
        if (list.isEmpty()) list = pm.filter ("ALL", {});
        if (! list.isEmpty()) pm.load (list[Random::getSystemRandom().nextInt (list.size())]);
    };
    star.isOn = [this] { auto& pm = processor.getPresetManager(); return pm.getCurrentIndex() >= 0 && pm.isFavorite (pm.getCurrentIndex()); };
    star.onClick = [this] { auto& pm = processor.getPresetManager(); if (pm.getCurrentIndex() >= 0) pm.toggleFavorite (pm.getCurrentIndex()); };
    for (auto* c : std::initializer_list<Component*> { &presetBox, &prevButton, &nextButton, &initButton,
                                                       &saveButton, &saveAsButton, &settingsButton, &randomButton, &star })
        addAndMakeVisible (c);
    startTimerHz (30);
}

void HeaderBar::save()
{
    auto& pm = processor.getPresetManager();
    if (pm.getCurrentIndex() >= 0 && ! pm.isCurrentFactory())
        pm.saveUserPreset (pm.getCurrentName(), pm.getCurrentCategory());
    else
        showSaveAsDialog();
}

void HeaderBar::showSaveAsDialog()
{
    auto& pm = processor.getPresetManager();
    auto* w = new AlertWindow ("SALVAR PRESET", "Nome e categoria do novo preset:", MessageBoxIconType::NoIcon);
    w->setLookAndFeel (&getLookAndFeel());
    const auto current = pm.getCurrentName();
    w->addTextEditor ("name", current == "INIT" ? String ("Meu Preset") : current, "NOME");
    w->addComboBox ("cat", PresetManager::categories(), "CATEGORIA");
    if (auto* box = w->getComboBoxComponent ("cat"))
    {
        const int idx = PresetManager::categories().indexOf (pm.getCurrentCategory());
        box->setSelectedItemIndex (idx >= 0 ? idx : 0, dontSendNotification);
    }
    w->addButton ("SALVAR", 1, KeyPress (KeyPress::returnKey));
    w->addButton ("CANCELAR", 0, KeyPress (KeyPress::escapeKey));

    SafePointer<HeaderBar> self (this);
    w->enterModalState (true, ModalCallbackFunction::create ([self, w] (int result)
    {
        if (result != 1 || self == nullptr)
            return;
        const auto name = w->getTextEditorContents ("name").trim();
        const auto cat  = w->getComboBoxComponent ("cat")->getText();
        if (name.isEmpty() || ! self->processor.getPresetManager().saveUserPreset (name, cat))
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "SALVAR PRESET",
                                              "Nao foi possivel salvar. Verifique o nome e a permissao da pasta Documentos.");
    }), true);
}

void HeaderBar::showSettingsMenu()
{
    PopupMenu scaleMenu, themeMenu;
    const float current = processor.getUiScale();
    for (float s : { 0.8f, 1.0f, 1.25f, 1.5f })
        scaleMenu.addItem (String (roundToInt (s * 100)) + "%", true, std::abs (current - s) < 0.01f,
                           [this, s] { if (onScaleChosen) onScaleChosen (s); });

    for (auto* t : Theme::all())
    {
        const auto name = t->name;
        themeMenu.addItem (name, true, processor.getThemeName() == name,
                           [this, name] { if (onThemeChosen) onThemeChosen (name); });
    }

    PopupMenu menu;
    menu.addSectionHeader ("ATDT SYNTH MACHINE  v" JucePlugin_VersionString);
    menu.addSubMenu ("UI SCALE", scaleMenu);
    menu.addSubMenu ("THEME", themeMenu);
    menu.addSeparator();
    menu.addItem ("Abrir pasta de presets do usuario", []
    {
        auto f = PresetManager::getUserPresetFolder();
        f.createDirectory();
        f.startAsProcess();
    });
    menu.addItem ("Recarregar lista de presets", [this] { processor.getPresetManager().rescan(); });
    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&settingsButton));
}

void HeaderBar::timerCallback()
{
    const auto notes = processor.midiActivity.getNoteOnCount();
    midiLed = (notes != lastNotes) ? 1.0f : midiLed * 0.82f;
    lastNotes = notes;
    meterL = jmax (processor.peakLeft.load(),  meterL * 0.86f);
    meterR = jmax (processor.peakRight.load(), meterR * 0.86f);
    repaint();
    presetBox.repaint();
}

void StarButton::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    const bool on = isOn && isOn();
    auto r = getLocalBounds().toFloat().reduced (3.0f);
    Path star;
    const auto c = r.getCentre();
    const float ro = jmin (r.getWidth(), r.getHeight()) * 0.5f, ri = ro * 0.45f;
    for (int i = 0; i < 10; ++i)
    {
        const float a = -MathConstants<float>::halfPi + i * MathConstants<float>::pi / 5.0f;
        const float rad = (i % 2 == 0) ? ro : ri;
        const Point<float> pt (c.x + std::cos (a) * rad, c.y + std::sin (a) * rad);
        if (i == 0) star.startNewSubPath (pt); else star.lineTo (pt);
    }
    star.closeSubPath();
    if (on) { g.setColour (t.accent.withAlpha (0.25f)); g.fillPath (star, AffineTransform::scale (1.25f, 1.25f, c.x, c.y)); g.setColour (t.accent); g.fillPath (star); }
    else    { g.setColour (t.textDim); g.strokePath (star, PathStrokeType (1.2f)); }
}

void HeaderBar::resized()
{
    // faixa 2 (coluna de conteudo): < [preset] > * categoria | SAVE SAVE AS INIT RANDOM MENU | osciloscopio
    auto r = getLocalBounds().withTrimmedTop (Layout::row1 + 4).withTrimmedLeft (Layout::sidebar + 8).withTrimmedRight (10).reduced (8, 6);
    prevButton.setBounds (r.removeFromLeft (26)); r.removeFromLeft (2);
    nextButton.setBounds (r.removeFromLeft (26)); r.removeFromLeft (6);
    presetBox.setBounds (r.removeFromLeft (260)); r.removeFromLeft (4);
    star.setBounds (r.removeFromLeft (24)); r.removeFromLeft (8);
    categoryArea = r.removeFromLeft (150);
    r.removeFromLeft (8);
    for (auto* b : { &saveButton, &saveAsButton, &initButton, &randomButton, &settingsButton })
    {
        b->setBounds (r.removeFromLeft (b == &saveAsButton || b == &randomButton ? 76 : 60));
        r.removeFromLeft (4);
    }
    r.removeFromLeft (8);
    scopeArea = r;
}

void HeaderBar::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    auto h = getLocalBounds();
    // ---------------- faixa 1: painel frontal (marca + navegacao + status)
    auto top = h.removeFromTop (Layout::row1);
    g.setGradientFill (ColourGradient (t.panelRaised.brighter (0.06f), 0, 0, t.panel.darker (0.35f), 0, (float) top.getBottom(), false));
    g.fillRect (top);
    hw::brushed (g, top.toFloat(), 1.2f);
    g.setColour (Colours::white.withAlpha (0.07f)); g.drawHorizontalLine (0, 0.0f, (float) top.getRight());
    g.setColour (Colours::black); g.fillRect (0, top.getBottom() - 2, top.getWidth(), 2);
    for (auto pt : { Point<float> (8, 9), Point<float> (8, (float) top.getBottom() - 10), Point<float> ((float) top.getRight() - 8, 9),
                     Point<float> ((float) top.getRight() - 8, (float) top.getBottom() - 10) })
        hw::screw (g, pt, 2.4f);

    // marca: emblema + ATDT SYNTH MACHINE + tagline
    {
        auto logo = top.withWidth (Layout::logoW).reduced (18, 12);
        auto emblem = logo.removeFromLeft (50).toFloat().reduced (2.0f);
        hw::plate (g, emblem, t, 6.0f, true);
        Path mark;   // "///" em neon
        for (int i = 0; i < 3; ++i)
        {
            const float x = emblem.getX() + 13.0f + i * 8.0f;
            mark.startNewSubPath (x, emblem.getBottom() - 12.0f); mark.lineTo (x + 8.0f, emblem.getY() + 12.0f);
        }
        strokeNeon (g, mark, t.accent, 2.6f);
        logo.removeFromLeft (12);
        g.setColour (t.text);
        g.setFont (FontOptions (24.0f, Font::bold).withKerningFactor (0.04f));
        auto line1 = logo.removeFromTop (logo.getHeight() * 6 / 10);
        g.drawText ("ATDT", line1.removeFromLeft (66), Justification::bottomLeft);
        g.setColour (t.accent);
        g.drawText ("/", line1.removeFromLeft (14), Justification::bottomLeft);
        g.setColour (t.text);
        g.setFont (FontOptions (19.0f, Font::bold).withKerningFactor (0.05f));
        g.drawText ("SYNTH MACHINE", line1, Justification::bottomLeft, false);
        hw::micro (g, "Modern Metal Synthesis Engine", logo, Justification::topLeft, t.textDim, 9.5f);
    }

    // status: CPU | VOICE | BPM | KEY + LEDs
    {
        auto st = top.removeFromRight (Layout::statusW).reduced (10, 12);
        hw::inset (g, st.toFloat(), t, 3.0f);
        auto leds = st.removeFromRight (86).reduced (6, 4);
        auto cell = [&] (Rectangle<int> a, const String& label, const String& value)
        {
            g.setColour (Colours::black.withAlpha (0.6f));
            g.drawVerticalLine (a.getX(), (float) a.getY() + 6.0f, (float) a.getBottom() - 6.0f);
            hw::micro (g, label, a.removeFromTop (a.getHeight() / 2 - 2).withTrimmedTop (4), Justification::centredBottom, t.textDim, 9.0f);
            g.setColour (t.text);
            g.setFont (FontOptions (15.0f, Font::bold));
            g.drawText (value, a, Justification::centredTop);
        };
        const int vc[] { 1, 2, 4, 8, 16, 32 };
        const int maxV = vc[jlimit (0, 5, (int) processor.apvts.getRawParameterValue (ParamIDs::voiceCount)->load())];
        const int root = (int) processor.apvts.getRawParameterValue (ParamIDs::keyRoot)->load();
        const int scl = (int) processor.apvts.getRawParameterValue (ParamIDs::keyScale)->load();
        const int w = st.getWidth() / 4;
        cell (st.removeFromLeft (w - 6), "CPU", String (roundToInt (processor.getCpuLoadPercent())) + "%");
        cell (st.removeFromLeft (w - 6), "VOICE", String (processor.activeVoices.load()) + "/" + String (maxV));
        cell (st.removeFromLeft (w + 2), "BPM", String (processor.getCurrentBpm(), 2));
        cell (st, "KEY", noteNames()[root % 12] + " " + scaleNames()[scl].upToFirstOccurrenceOf (" ", false, false));
        const float out = jmax (meterL, meterR);
        const std::pair<const char*, float> l3[] { { "MIDI", midiLed }, { "SYNC", processor.isHostPlaying() ? 1.0f : 0.0f },
                                                   { "OUTPUT", out > 0.001f ? jmin (1.0f, 0.4f + out) : 0.0f } };
        for (int i = 0; i < 3; ++i)
        {
            auto row = leds.removeFromTop (leds.getHeight() / (3 - i));
            hw::led (g, { (float) row.getX() + 5.0f, (float) row.getCentreY() }, 3.0f, l3[i].second, t);
            hw::micro (g, l3[i].first, row.withTrimmedLeft (14), Justification::centredLeft, l3[i].second > 0.1f ? t.accentSoft : t.textDim, 8.5f);
        }
    }

    // ---------------- faixa 2 (coluna de conteudo): barra do preset + osciloscopio
    auto bar = getLocalBounds().withTrimmedTop (Layout::row1 + 4).withTrimmedLeft (Layout::sidebar + 8).withTrimmedRight (10).toFloat();
    hw::plate (g, bar, t, 6.0f, false);
    auto& pm = processor.getPresetManager();
    hw::micro (g, pm.getCurrentCategory().isNotEmpty() ? pm.getCurrentCategory() : String ("INIT"), categoryArea, Justification::centredLeft, t.textDim, 9.5f);
    hw::inset (g, scopeArea.toFloat(), t, 3.0f);
    {
        auto sc = scopeArea.toFloat().reduced (4.0f, 3.0f);
        g.setColour (t.accentDark.withAlpha (0.35f));
        g.drawHorizontalLine ((int) sc.getCentreY(), sc.getX(), sc.getRight());
        Path wave;
        const int n = MetalMachineAudioProcessor::scopeSize, start = processor.scopeWrite.load();
        float mx = 0.05f;
        for (int i = 0; i < n; ++i) mx = jmax (mx, std::abs (processor.scope[(size_t) i].load()));
        for (int i = 0; i < n; ++i)
        {
            const float v = processor.scope[(size_t) ((start + i) % n)].load() / mx;
            const float x = sc.getX() + sc.getWidth() * (float) i / (float) (n - 1), y = sc.getCentreY() - v * sc.getHeight() * 0.45f;
            if (i == 0) wave.startNewSubPath (x, y); else wave.lineTo (x, y);
        }
        strokeNeon (g, wave, t.accent, 1.2f);
    }
}

UpcomingPage::UpcomingPage (String t, String p, StringArray f)
    : title (std::move (t)), phase (std::move (p)), features (std::move (f)) {}

void UpcomingPage::paint (Graphics& g)
{
    const auto& th = themeOf (*this);
    auto r = getLocalBounds().toFloat();
    g.setColour (th.panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (th.edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

    auto area = getLocalBounds().withSizeKeepingCentre (560, 300);
    g.setFont (FontOptions (30.0f, Font::bold));
    g.setColour (th.text);
    g.drawText (title, area.removeFromTop (44), Justification::centredLeft);
    g.setFont (FontOptions (13.0f, Font::bold));
    g.setColour (th.accent);
    g.drawText ("EM DESENVOLVIMENTO  //  " + phase, area.removeFromTop (26), Justification::centredLeft);
    area.removeFromTop (14);
    g.setFont (FontOptions (14.0f));
    for (auto& f : features)
    {
        auto line = area.removeFromTop (26);
        g.setColour (th.accentDark.brighter (0.4f));
        g.fillRect (line.getX(), line.getCentreY() - 1, 10, 2);
        g.setColour (th.textDim);
        g.drawText (f, line.withTrimmedLeft (20), Justification::centredLeft);
    }
}

// ======================================================================== MAIN VIEW
// Aba da navegacao superior: icone + rotulo; ativa = neon com faixa luminosa embaixo
class NavTab : public TextButton
{
public:
    explicit NavTab (const String& name) : TextButton (name) {}
    void paintButton (Graphics& g, bool over, bool) override
    {
        const auto& t = themeOf (*this);
        const bool on = getToggleState();
        auto r = getLocalBounds().toFloat();
        if (on)
        {
            g.setGradientFill (ColourGradient (t.accent.withAlpha (0.0f), 0, r.getY(), t.accent.withAlpha (0.22f), 0, r.getBottom(), false));
            g.fillRect (r);
            g.setColour (t.accent); g.fillRect (r.removeFromBottom (3.0f).reduced (6.0f, 0.0f));
        }
        else if (over) { g.setColour (Colours::white.withAlpha (0.04f)); g.fillRect (r); }
        g.setColour (Colours::black.withAlpha (0.5f));
        g.drawVerticalLine (getWidth() - 1, 10.0f, (float) getHeight() - 10.0f);
        const auto col = on ? t.accent : (over ? t.text : t.textDim.brighter (0.25f));
        auto ic = getLocalBounds().toFloat().withTrimmedBottom (22.0f).withSizeKeepingCentre (26.0f, 22.0f);
        Path p; const auto n = getButtonText();
        const float x = ic.getX(), y = ic.getY(), w = ic.getWidth(), h = ic.getHeight(), cy = ic.getCentreY();
        if (n == "SYNTH")      { p.startNewSubPath (x, cy + h * 0.35f);   // dente de serra limpo (2 ciclos)
                                 for (int c = 0; c < 2; ++c) { const float x0 = x + c * w * 0.5f; p.lineTo (x0 + w * 0.5f, cy - h * 0.35f); p.lineTo (x0 + w * 0.5f, cy + h * 0.35f); } }
        else if (n == "MOD")   { p.startNewSubPath (x, cy); for (int i = 0; i <= 40; ++i) { const float u = (float) i / 40.0f; p.lineTo (x + u * w, cy - std::sin (u * 6.283f) * h * 0.38f); } p.addEllipse (x + w * 0.22f - 2, cy - h * 0.38f - 2, 4, 4); }
        else if (n == "FX")    { for (int i = 0; i < 3; ++i) { const float xx = x + 4 + i * 9.0f; p.startNewSubPath (xx, y + 2); p.lineTo (xx, y + h - 2); p.addRectangle (xx - 3, y + 4 + ((i * 7) % 12), 6, 4); } }
        else if (n == "SEQUENCER") { for (int i = 0; i < 4; ++i) for (int j = 0; j < 3; ++j) p.addRectangle (x + 2 + i * 6.0f, y + 3 + j * 6.0f, 4.0f, 4.0f); }
        else if (n == "DRUMS") { p.addEllipse (x + 3, y + 5, w - 6, h * 0.45f); p.startNewSubPath (x + 3, y + 5 + h * 0.22f); p.lineTo (x + 3, y + h - 3); p.lineTo (x + w - 3, y + h - 3); p.lineTo (x + w - 3, y + 5 + h * 0.22f); }
        else if (n == "PRODUCER") { p.addRectangle (x + 3, y + 3, w - 6, h - 6); for (int i = 0; i < 3; ++i) { p.startNewSubPath (x + 7 + i * 6.0f, y + h - 6); p.lineTo (x + 7 + i * 6.0f, y + 6 + ((i * 5) % 9)); } }
        else if (n == "SCRATCH") { p.addEllipse (x + 2, y + 1, h - 2, h - 2); p.addEllipse (x + h * 0.5f - 1, y + h * 0.5f - 2, 3, 3);
                                   p.startNewSubPath (x + w - 2, y + 2); p.lineTo (x + w - 4, y + h * 0.6f); p.lineTo (x + h * 0.7f, y + h * 0.75f); }
        else                   { p.addEllipse (x + w * 0.5f - 9, cy - 9, 18, 18); p.addEllipse (x + w * 0.5f - 4, cy - 4, 8, 8); p.startNewSubPath (x + w * 0.5f, cy - 11); p.lineTo (x + w * 0.5f, cy + 11); }
        g.setColour (col);
        g.strokePath (p, PathStrokeType (1.4f));
        g.setFont (FontOptions (10.5f, Font::bold).withKerningFactor (0.08f));
        g.drawText (n, getLocalBounds().removeFromBottom (24), Justification::centred);
    }
};

MainView::MainView (MetalMachineAudioProcessor& p)
    : header (p), processor (p), browser (p.getPresetManager()),
      keyboard (p.keyboardState, MidiKeyboardComponent::horizontalKeyboard)
{
    addAndMakeVisible (header);

    pages.push_back (std::make_unique<SynthPage> (p.apvts));
    pages.push_back (std::make_unique<ModPage> (p));
    pages.push_back (std::make_unique<FxPage> (p.apvts));
    pages.push_back (std::make_unique<SeqPage> (p));
    pages.push_back (std::make_unique<DrumsPage> (p));
    pages.push_back (std::make_unique<ScratchPage> (p));   // mesma ordem dos botoes: ... DRUMS, SCRATCH, PRODUCER, REFERENCE
    pages.push_back (std::make_unique<SongPage> (p));
    pages.push_back (std::make_unique<RefLabPage> (p));

    const StringArray names { "SYNTH", "MOD", "FX", "SEQUENCER", "DRUMS", "SCRATCH", "PRODUCER", "REFERENCE" };
    for (int i = 0; i < names.size(); ++i)
    {
        auto* b = tabs.add (new NavTab (names[i]));
        b->setClickingTogglesState (false);
        b->onClick = [this, i] { showTab (i); };
        addAndMakeVisible (b);
        addChildComponent (*pages[(size_t) i]);
    }

    // fontes de modulacao arrastaveis (visiveis em qualquer aba)
    for (int src : { (int) ModSource::Lfo1, (int) ModSource::Lfo2, (int) ModSource::Lfo3, (int) ModSource::Lfo4,
                     (int) ModSource::FilterEnv, (int) ModSource::AmpEnv, (int) ModSource::Velocity,
                     (int) ModSource::ModWheel, (int) ModSource::Aftertouch, (int) ModSource::Note,
                     (int) ModSource::Macro1, (int) ModSource::Macro2, (int) ModSource::Macro3, (int) ModSource::Macro4,
                     (int) ModSource::Random, (int) ModSource::ModEnv })
        addAndMakeVisible (sourceChips.add (new ModSourceChip (src)));

    header.onPresetClicked = [this] { setBrowserVisible (! browser.isVisible()); };
    browser.onClose = [this] { setBrowserVisible (false); };
    addChildComponent (browser);

    keyboard.setAvailableRange (24, 108);
    keyboard.setLowestVisibleKey (36);
    keyboard.setKeyWidth (22.0f);
    addAndMakeVisible (keyboard);
    sideBrowser = std::make_unique<SideBrowser> (p);
    addAndMakeVisible (*sideBrowser);

    showTab (0);
}

void MainView::setBrowserVisible (bool show)
{
    browser.setVisible (show);
    if (show) browser.toFront (true);
    for (int i = 0; i < (int) pages.size(); ++i)
        pages[(size_t) i]->setVisible (! show && i == currentTab);
}

void MainView::showTab (int index)
{
    browser.setVisible (false);
    currentTab = index;
    for (int i = 0; i < tabs.size(); ++i)
    {
        tabs[i]->setToggleState (i == index, dontSendNotification);
        pages[(size_t) i]->setVisible (i == index);
    }
}

void MainView::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    g.fillAll (t.background);
    hw::brushed (g, getLocalBounds().toFloat(), 0.6f);          // chassis de aco escuro

    keyboard.setColour (MidiKeyboardComponent::keyDownOverlayColourId, t.accent);
    keyboard.setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, t.accent.withAlpha (0.3f));
    keyboard.setColour (MidiKeyboardComponent::shadowColourId, Colours::transparentBlack);
    keyboard.setColour (MidiKeyboardComponent::keySeparatorLineColourId, t.background);
    keyboard.setColour (MidiKeyboardComponent::whiteNoteColourId, Colour (0xffb8bcc3));   // teclas em aluminio
    keyboard.setColour (MidiKeyboardComponent::blackNoteColourId, t.panelRaised);
    keyboard.setColour (MidiKeyboardComponent::upDownButtonBackgroundColourId, t.panel);
    keyboard.setColour (MidiKeyboardComponent::upDownButtonArrowColourId, t.accent);

    g.setFont (FontOptions (10.0f, Font::bold));
    g.setColour (t.textDim);
    g.drawText ("MOD SOURCES", chipLabelArea, Justification::centredRight);

    // rodape do chassis
    auto f = footerArea;
    g.setColour (Colours::black.withAlpha (0.6f)); g.fillRect (f);
    g.setColour (Colours::white.withAlpha (0.05f)); g.drawHorizontalLine (f.getY(), 0.0f, (float) f.getRight());
    hw::screw (g, { 10.0f, (float) f.getCentreY() }, 2.4f);
    hw::screw (g, { (float) f.getRight() - 10.0f, (float) f.getCentreY() }, 2.4f);
    auto fl = f.reduced (24, 0);
    g.setColour (t.text.withAlpha (0.75f));
    g.setFont (FontOptions (12.0f, Font::bold).withKerningFactor (0.1f));
    g.drawText ("SM-01", fl.removeFromLeft (60), Justification::centredLeft);
    hw::micro (g, "|  VST3  |  v" JucePlugin_VersionString "  |  ATDT Audio Tools", fl.removeFromLeft (360), Justification::centredLeft, t.textDim, 9.0f);
    hw::micro (g, "Built for modern metal  ///", fl, Justification::centredRight, t.textDim, 9.0f);
}

void MainView::resized()
{
    auto r = getLocalBounds();
    footerArea = r.removeFromBottom (24);
    header.setBounds (getLocalBounds().withHeight (Layout::row1 + 4 + Layout::row2));
    // navegacao no meio da faixa 1 (entre a marca e o status)
    {
        auto nav = getLocalBounds().withHeight (Layout::row1).withTrimmedLeft (Layout::logoW).withTrimmedRight (Layout::statusW).reduced (0, 6);
        const int w = jmin (104, nav.getWidth() / jmax (1, tabs.size()));
        for (auto* b : tabs) b->setBounds (nav.removeFromLeft (w));
    }
    r.removeFromTop (Layout::row1 + 4);
    sideBrowser->setBounds (r.removeFromLeft (Layout::sidebar).withTrimmedLeft (8).withTrimmedBottom (6));
    r.removeFromLeft (8);
    r.removeFromRight (10);
    r.removeFromTop (Layout::row2 + 6);
    // fontes de modulacao arrastaveis (patch-points)
    auto chipRow = r.removeFromTop (26);
    for (int i = sourceChips.size(); --i >= 0;)
    {
        sourceChips[i]->setBounds (chipRow.removeFromRight (44).reduced (0, 3));
        chipRow.removeFromRight (2);
    }
    chipLabelArea = chipRow.removeFromRight (90);
    r.removeFromTop (6);
    keyboard.setBounds (r.removeFromBottom (58));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 50.0f);
    r.removeFromBottom (6);
    for (auto& pg : pages) pg->setBounds (r);
    browser.setBounds (r);
}
} // namespace mm
