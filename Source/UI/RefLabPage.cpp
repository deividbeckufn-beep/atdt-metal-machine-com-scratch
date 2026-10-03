#include "RefLabPage.h"
#include "Hardware.h"

using namespace juce;

namespace mm
{
static const char* kNoteLetters[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

// ====================================================================== JOB
File ReferenceJob::cacheFileFor (const File& audio)
{
    // chave = arquivo (caminho, tamanho, data) + VERSAO do algoritmo (melhorias invalidam o cache antigo)
    const auto key = String (audio.getFullPathName() + String (audio.getSize()) + String (audio.getLastModificationTime().toMilliseconds())
                             + "v" + String (ReferenceAnalyzer::algorithmVersion)).hashCode64();
    return PresetManager::getBaseFolder().getChildFile ("ReferenceCache").getChildFile (String::toHexString (key) + ".json");
}

void ReferenceJob::run()
{
    const auto cache = cacheFileFor (file);
    if (cache.existsAsFile())   // REFERENCE CACHE: mesma faixa = sem reprocessar
    {
        result = ReferenceResult::fromVar (JSON::parse (cache.loadFileAsString()));
        if (result.ok) { fromCache = true; progress = 1.0f; done = true; return; }
    }
    result = ReferenceAnalyzer::analyzeFile (file, [this] (float p) { progress = p; }, [this] { return threadShouldExit(); });
    if (result.ok)
    {
        cache.getParentDirectory().createDirectory();
        cache.replaceWithText (JSON::toString (result.toVar(), true));
    }
    done = true;
}

// ====================================================================== PAGE
RefLabPage::RefLabPage (MetalMachineAudioProcessor& p) : processor (p)
{
    importButton.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> ("Escolha a musica de referencia", File::getSpecialLocation (File::userMusicDirectory),
                                                 "*.wav;*.aif;*.aiff;*.flac;*.mp3");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                              [this] (const FileChooser& fc) { if (fc.getResult().existsAsFile()) startAnalysis (fc.getResult()); });
    };
    keyButton.setTooltip ("Aplica KEY e SCALE detectadas ao plugin (sequencer, 808, Song)");
    keyButton.onClick = [this] { processor.applyReferenceKey(); status = "KEY aplicada."; repaint(); };
    grooveButton.setTooltip ("Cria um groove NOVO com o Beat DNA da referencia (nunca copia o ritmo da musica)");
    grooveButton.onClick = [this] { processor.generateGrooveFromReference(); status = "Groove original gerado: veja a aba DRUMS."; repaint(); };
    songButton.setTooltip ("Cria uma ideia musical NOVA com o DNA da referencia (aba SONG)");
    songButton.onClick = [this] { processor.generateSongFromReference(); status = "Musica original gerada: veja a aba SONG."; repaint(); };
    presetTarget.addItemList ({ "BASS", "LEAD", "PAD" }, 1);
    presetTarget.setSelectedId (1, dontSendNotification);
    presetButton.setTooltip ("Reconstroi um preset APROXIMADO a partir do Sound DNA e salva na categoria REFERENCE");
    presetButton.onClick = [this]
    {
        const auto& r = processor.getReference();
        if (! r.ok) return;
        const auto name = "Ref " + File (r.fileName).getFileNameWithoutExtension().substring (0, 24) + " " + presetTarget.getText();
        status = processor.createPresetFromReference (presetTarget.getSelectedId() - 1, name)
                   ? "Preset salvo: " + name + " (categoria REFERENCE no navegador)" : "Nao foi possivel salvar o preset.";
        repaint();
    };
    for (auto* c : std::initializer_list<Component*> { &mainPanel, &analysisPanel, &dnaPanel, &importButton, &keyButton, &grooveButton,
                                                       &songButton, &presetButton, &presetTarget })
        addAndMakeVisible (c);
    startTimerHz (15);
}

RefLabPage::~RefLabPage() { stopTimer(); job.reset(); }

bool RefLabPage::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files) if (File (f).hasFileExtension ("wav;aif;aiff;flac;mp3")) return true;
    return false;
}

void RefLabPage::filesDropped (const StringArray& files, int, int)
{
    dragOver = false;
    for (auto& f : files) if (File (f).hasFileExtension ("wav;aif;aiff;flac;mp3")) { startAnalysis (File (f)); return; }
}

void RefLabPage::startAnalysis (const File& f)
{
    job.reset();   // cancela analise anterior
    job = std::make_unique<ReferenceJob> (f);
    status = "ANALISANDO " + f.getFileName() + " ...";
    job->startThread (Thread::Priority::low);   // baixa prioridade: o audio nunca e afetado
    repaint();
}

void RefLabPage::timerCallback()
{
    const bool has = processor.getReference().ok;
    for (auto* b : { &keyButton, &grooveButton, &songButton, &presetButton }) b->setEnabled (has);
    if (job != nullptr)
    {
        if (job->done)
        {
            if (job->result.ok)
            {
                processor.setReference (job->result);
                status = (job->fromCache ? "CACHE: " : "ANALISE CONCLUIDA: ") + job->result.fileName;
            }
            else
                status = "REFERENCE ANALYSIS FAILED - " + job->result.error;
            job.reset();
        }
        repaint();
    }
}

void RefLabPage::drawBars (Graphics& g, Rectangle<int> area, const StringArray& names, const std::vector<float>& vals, bool percent)
{
    const auto& t = themeOf (*this);
    const int rows = names.size();
    const float rowH = (float) area.getHeight() / (float) jmax (1, rows);
    for (int i = 0; i < rows; ++i)
    {
        auto row = Rectangle<float> ((float) area.getX(), area.getY() + i * rowH, (float) area.getWidth(), rowH).reduced (0.0f, 2.0f);
        hw::micro (g, names[i], row.removeFromLeft (96.0f).toNearestInt(), Justification::centredLeft, t.textDim, 8.5f);
        auto val = row.removeFromRight (40.0f);
        const float v = jlimit (0.0f, 1.0f, vals[(size_t) i]);
        g.setColour (t.text.withAlpha (0.85f));
        g.setFont (FontOptions (10.0f, Font::bold));
        g.drawText (percent ? String (roundToInt (v * 100)) + "%" : String (v, 2), val.toNearestInt(), Justification::centredRight);
        auto track = row.reduced (4.0f, rowH * 0.28f);
        g.setColour (Colours::black.withAlpha (0.7f)); g.fillRect (track);
        g.setColour (t.accent.withAlpha (0.35f + 0.65f * v)); g.fillRect (track.withWidth (track.getWidth() * v));
    }
}

void RefLabPage::paintOverChildren (Graphics& g)
{
    const auto& t = themeOf (*this);
    const auto& r = processor.getReference();

    // zona de arraste
    hw::inset (g, dropArea.toFloat(), t, 3.0f);
    if (dragOver) { g.setColour (t.accent.withAlpha (0.2f)); g.fillRoundedRectangle (dropArea.toFloat(), 3.0f); }
    auto da = dropArea.reduced (10, 6);
    g.setColour (r.ok ? t.text : t.textDim);
    g.setFont (FontOptions (12.0f, Font::bold));
    g.drawText (r.ok ? r.fileName : "DROP REFERENCE TRACK", da.removeFromTop (18), Justification::centredLeft, true);
    if (job != nullptr)
    {
        auto bar = da.removeFromTop (8).toFloat();
        g.setColour (Colours::black); g.fillRect (bar);
        g.setColour (t.accent); g.fillRect (bar.withWidth (bar.getWidth() * job->progress.load()));
    }
    hw::micro (g, status, da, Justification::topLeft, dragOver ? t.accentSoft : t.textDim, 8.5f);

    // leituras: BPM / KEY com confianca + dinamica / estereo
    auto rd = readArea;
    auto readout = [&] (Rectangle<int> a, const String& label, const String& value, float conf, const String& sub)
    {
        hw::inset (g, a.toFloat(), t, 3.0f);
        auto in = a.reduced (8, 4);
        hw::micro (g, label, in.removeFromTop (12), Justification::centredLeft, t.textDim, 8.5f);
        g.setColour (r.ok ? t.accentSoft : t.textDim.withAlpha (0.5f));
        g.setFont (FontOptions (20.0f, Font::bold));
        g.drawText (r.ok ? value : "--", in.removeFromTop (24), Justification::centredLeft);
        if (conf >= 0.0f && r.ok)
        {
            auto cb = in.removeFromTop (6).toFloat();
            g.setColour (Colours::black); g.fillRect (cb);
            g.setColour (t.accent.withAlpha (0.4f + 0.6f * conf)); g.fillRect (cb.withWidth (cb.getWidth() * conf));
        }
        hw::micro (g, r.ok ? sub : String(), in, Justification::centredLeft, t.textDim, 8.5f);
    };
    const int w = (rd.getWidth() - 18) / 4;
    readout (rd.removeFromLeft (w), "Tempo", String (r.bpm, 1) + " BPM", r.bpmConfidence, "Confidence " + String (roundToInt (r.bpmConfidence * 100)) + "%");
    rd.removeFromLeft (6);
    readout (rd.removeFromLeft (w), "Key", String (kNoteLetters[r.keyRoot % 12]) + (r.keyMinor ? " MINOR" : " MAJOR"), r.keyConfidence,
             "Conf " + String (roundToInt (r.keyConfidence * 100)) + "%  alt " + kNoteLetters[r.altRoot % 12] + (r.altMinor ? " minor" : " major"));
    rd.removeFromLeft (6);
    readout (rd.removeFromLeft (w), "Dynamics", String (r.rmsDb, 1) + " dB RMS", -1.0f,
             "Peak " + String (r.peakDb, 1) + " dB  Crest " + String (r.crestDb, 1) + " dB");
    rd.removeFromLeft (6);
    readout (rd, "Stereo", "Width " + String (roundToInt (r.stereoWidth * 100)) + "%", -1.0f,
             "Correlation " + String (r.stereoCorrelation, 2) + "  " + String (r.durationSec, 0) + " s");

    // linha do tempo de energia + eventos
    hw::inset (g, timelineArea.toFloat(), t, 3.0f);
    if (r.ok && ! r.energy.empty())
    {
        auto tl = timelineArea.reduced (6, 14).toFloat();
        Path p; p.startNewSubPath (tl.getX(), tl.getBottom());
        for (size_t i = 0; i < r.energy.size(); ++i)
            p.lineTo (tl.getX() + tl.getWidth() * (float) i / (float) jmax ((size_t) 1, r.energy.size() - 1), tl.getBottom() - tl.getHeight() * r.energy[i]);
        p.lineTo (tl.getRight(), tl.getBottom()); p.closeSubPath();
        g.setGradientFill (ColourGradient (t.accent.withAlpha (0.45f), 0, tl.getY(), t.accent.withAlpha (0.05f), 0, tl.getBottom(), false));
        g.fillPath (p);
        for (auto& [sec, kind] : r.events)
        {
            const float x = tl.getX() + tl.getWidth() * (float) (sec / jmax (1.0, r.durationSec));
            g.setColour (kind == "IMPACT" ? t.accent : t.text.withAlpha (0.5f));
            g.drawVerticalLine ((int) x, tl.getY() - 10.0f, tl.getBottom());
            hw::micro (g, kind, { (int) x + 2, timelineArea.getY() + 2, 60, 10 }, Justification::topLeft, kind == "IMPACT" ? t.accentSoft : t.textDim, 7.5f);
        }
        for (int s = 0; s <= (int) r.durationSec; s += 30)
            hw::micro (g, String (s / 60) + ":" + String (s % 60).paddedLeft ('0', 2),
                       { (int) (tl.getX() + tl.getWidth() * s / jmax (1.0, r.durationSec)), timelineArea.getBottom() - 12, 40, 10 },
                       Justification::topLeft, t.textDim, 7.5f);
    }
    else hw::micro (g, "ENERGY / EVENTS", timelineArea, Justification::centred, t.textDim, 9.0f);

    // espectro por faixa + perfil de notas
    hw::micro (g, "Spectrum", spectrumArea.withHeight (12), Justification::topLeft, t.textDim, 8.5f);
    drawBars (g, spectrumArea.withTrimmedTop (14), { "SUB <60", "BASS", "LOW-MID", "MID", "HIGH-MID", "HIGH" },
              { r.bands[0] * 2.5f, r.bands[1] * 1.5f, r.bands[2] * 3.0f, r.bands[3] * 3.0f, r.bands[4] * 4.0f, r.bands[5] * 6.0f }, true);
    hw::micro (g, "Pitch profile (harmonic)", chromaArea.withHeight (12), Justification::topLeft, t.textDim, 8.5f);
    {
        auto ca = chromaArea.withTrimmedTop (16).toFloat();
        const float cw = ca.getWidth() / 12.0f;
        for (int i = 0; i < 12; ++i)
        {
            auto col = Rectangle<float> (ca.getX() + i * cw, ca.getY(), cw, ca.getHeight()).reduced (2.0f, 0.0f);
            auto lab = col.removeFromBottom (12.0f);
            const float v = r.ok ? r.chroma[(size_t) i] : 0.0f;
            g.setColour (Colours::black.withAlpha (0.6f)); g.fillRect (col);
            const bool tonic = r.ok && i == r.keyRoot;
            g.setColour ((tonic ? t.accentSoft : t.accent).withAlpha (0.3f + 0.7f * v));
            g.fillRect (col.withTrimmedTop (col.getHeight() * (1.0f - v)));
            hw::micro (g, kNoteLetters[i], lab.toNearestInt(), Justification::centred, tonic ? t.accentSoft : t.textDim, 7.5f);
        }
    }

    // DNA
    hw::micro (g, "Beat DNA", beatArea.withHeight (12), Justification::topLeft, t.accentSoft, 9.0f);
    drawBars (g, beatArea.withTrimmedTop (14), { "KICK DENSITY", "HAT DENSITY", "HALF-TIME", "SYNCOPATION", "TRAP", "ELECTRONIC" },
              { r.kickDensity, r.hatDensity, r.halfTime, r.syncopation, r.trap, r.electronic }, true);
    hw::micro (g, "Sound DNA", soundArea.withHeight (12), Justification::topLeft, t.accentSoft, 9.0f);
    drawBars (g, soundArea.withTrimmedTop (14), { "SUB", "BRIGHT", "DISTORTION", "STEREO", "DENSITY", "DARKNESS", "AGGRESSION", "IMPACT" },
              { r.sub, r.bright, r.distortion, r.stereo, r.density, r.darkness, r.aggression, r.impact }, true);
}

void RefLabPage::resized()
{
    constexpr int gap = 6;
    auto r = getLocalBounds();
    auto top = r.removeFromTop (128);
    r.removeFromTop (gap);
    auto mid = r.removeFromTop (r.getHeight() / 2 + 10);
    r.removeFromTop (gap);
    mainPanel.setBounds (top); analysisPanel.setBounds (mid); dnaPanel.setBounds (r);

    auto c = mainPanel.getContentArea().translated (top.getX(), top.getY()).reduced (4, 2);
    dropArea = c.removeFromLeft (280);
    importButton.setBounds (dropArea.removeFromRight (70).reduced (4, 24));
    c.removeFromLeft (gap);
    auto actions = c.removeFromRight (400);
    c.removeFromRight (gap);
    readArea = c;
    auto a1 = actions.removeFromTop (actions.getHeight() / 2).reduced (0, 3);
    keyButton.setBounds (a1.removeFromLeft (100)); a1.removeFromLeft (gap);
    grooveButton.setBounds (a1);
    auto a2 = actions.reduced (0, 3);
    songButton.setBounds (a2.removeFromLeft (180)); a2.removeFromLeft (gap);
    presetTarget.setBounds (a2.removeFromLeft (70)); a2.removeFromLeft (gap);
    presetButton.setBounds (a2);

    auto m = analysisPanel.getContentArea().translated (mid.getX(), mid.getY()).reduced (4, 2);
    timelineArea = m.removeFromTop (m.getHeight() / 2);
    m.removeFromTop (gap);
    spectrumArea = m.removeFromLeft (m.getWidth() / 2 - 10);
    m.removeFromLeft (20);
    chromaArea = m;

    auto d = dnaPanel.getContentArea().translated (r.getX(), r.getY()).reduced (4, 2);
    beatArea = d.removeFromLeft (d.getWidth() / 2 - 10);
    d.removeFromLeft (20);
    soundArea = d;
}
} // namespace mm
