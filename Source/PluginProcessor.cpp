#include "PluginProcessor.h"
#include "DSP/WavetableBank.h"
#include "PluginEditor.h"

MetalMachineAudioProcessor::MetalMachineAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "METALMACHINE_STATE", mm::createParameterLayout())
{
    synthParams.attach (apvts);
    fxParams.attach (apvts);
    mm::WavetableBank::get();   // monta as wavetables AQUI (fora do audio thread)
    synth.initialise (synthParams);
    midiLearn.setParameters (getParameters());
    masterGainParam = apvts.getRawParameterValue (mm::ParamIDs::masterGain);
    bassMonoParam   = apvts.getRawParameterValue (mm::ParamIDs::bassMono);
    seqParams.attach (apvts);
    midiOutParam     = apvts.getRawParameterValue (mm::ParamIDs::midiOut);
    tempoSourceParam = apvts.getRawParameterValue (mm::ParamIDs::tempoSource);
    internalBpmParam = apvts.getRawParameterValue (mm::ParamIDs::internalBpm);
    timeFeelParam    = apvts.getRawParameterValue (mm::ParamIDs::timeFeel);
    humVelParam      = apvts.getRawParameterValue (mm::ParamIDs::humanizeVel);
    humTimeParam     = apvts.getRawParameterValue (mm::ParamIDs::humanizeTime);
    drumParams.attach (apvts);
    scratchParams.attach (apvts);
    scratch.prepare (48000.0);   // vozes embutidas ja existem antes do audio iniciar (a interface mostra o sample)
    mm::setInitDrums (drumPattern);
    pattern.setInit();
    presetManager   = std::make_unique<mm::PresetManager> (*this, apvts);
    presetManager->setPattern (&pattern);
    presetManager->setDrumPattern (&drumPattern);
}

void MetalMachineAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.prepare (sampleRate, samplesPerBlock);
    fx.prepare (sampleRate, samplesPerBlock);
    sequencer.prepare (sampleRate);
    drums.prepare (sampleRate);
    scratch.prepare (sampleRate);
    seqMidi.ensureSize (8192);
    keyboardState.reset();
    loadMeasurer.reset (sampleRate, samplesPerBlock);
    for (auto& f : bassMonoFilters) f.prepare (sampleRate);
    lastBassMonoFreq = -1.0f;
    masterGain.reset (sampleRate, 0.02);
    masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (masterGainParam->load(), -59.9f));
}

bool MetalMachineAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void MetalMachineAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    juce::AudioProcessLoadMeasurer::ScopedTimer cpuTimer (loadMeasurer, numSamples);

    buffer.clear();
    tempo.update (getPlayHead(), tempoSourceParam->load() > 0.5f, internalBpmParam->load());

    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);
    midiActivity.scan (midi);
    midiLearn.process (midi);
    drums.handleMidi (midi);   // DRUM KEYS: notas de bateria nao vao para o synth
    scratch.handleMidi (midi); // SCRATCH KEYS: C4-G4 disparam os golpes

    // sequencer/arp: transforma o MIDI do teclado nas notas que o synth toca
    seqMidi.clear();
    sequencer.process (midi, seqMidi, numSamples, tempo.getBpm(), tempo.isPlaying(), tempo.getPpq());

    synth.render (buffer, seqMidi, numSamples, tempo,
                  mm::feelMultiplier (juce::jlimit (0, 2, (int) timeFeelParam->load())));

    // MIDI OUT: envia as notas geradas para o REAPER (gravar no piano roll)
    if (midiOutParam->load() > 0.5f) midi.swapWith (seqMidi);
    else                             midi.clear();
    scratch.process (buffer, numSamples, tempo.getBpm(), tempo.isPlaying(), tempo.getPpq());   // scratch passa pelos efeitos
    fx.processMain (buffer, numSamples, tempo.getBpm(), tempo.isPlaying(), tempo.getPpq());   // cadeia FX (glitch usa o compasso)
    applyBassMono (buffer, numSamples);   // DEPOIS dos efeitos: distorcao/compressor/chorus nao recriam estereo no sub
    drums.process (buffer, numSamples, tempo.getBpm(), tempo.isPlaying(), tempo.getPpq(),   // bateria (fora do FX do synth)
                   mm::feelMultiplier (juce::jlimit (0, 2, (int) timeFeelParam->load())),
                   humVelParam->load(), humTimeParam->load());

    masterGain.setTargetValue (juce::Decibels::decibelsToGain (masterGainParam->load(), -59.9f));
    masterGain.applyGain (buffer, numSamples);
    fx.processLimiter (buffer, numSamples);                 // teto -0.3 dBFS
    {   // osciloscopio: 1 ponto a cada 4 amostras do canal esquerdo
        int w = scopeWrite.load (std::memory_order_relaxed);
        const float* d = buffer.getReadPointer (0);
        for (int i = 0; i < numSamples; i += 4)
        {
            scope[(size_t) w].store (d[i], std::memory_order_relaxed);
            w = (w + 1) % scopeSize;
        }
        scopeWrite.store (w, std::memory_order_relaxed);
    }
    peakLeft.store (buffer.getMagnitude (0, 0, numSamples));
    peakRight.store (buffer.getNumChannels() > 1 ? buffer.getMagnitude (1, 0, numSamples) : peakLeft.load());

    int voices = 0;
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (synth.getVoice (i)->isVoiceActive())
            ++voices;
    activeVoices.store (voices);
}

void MetalMachineAudioProcessor::setParamReal (const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        p->endChangeGesture();
    }
}

void MetalMachineAudioProcessor::loadDrumPattern (int index)
{
    const auto& lib = mm::getDrumLibrary();
    if (! juce::isPositiveAndBelow (index, (int) lib.size()))
        return;
    setParamReal (mm::ParamIDs::drumLength, (float) mm::applyDrumPattern (drumPattern, lib[(size_t) index]));
}

void MetalMachineAudioProcessor::clearDrums()
{
    drumPattern.clear();
}

// ============================================================ BEAT ENGINE
static float rawParam (juce::AudioProcessorValueTreeState& s, const char* id) { return s.getRawParameterValue (id)->load(); }

static mm::BeatSettings readBeatSettings (juce::AudioProcessorValueTreeState& s, uint32_t locks)
{
    mm::BeatSettings b;
    b.style = (int) rawParam (s, "beat_style");       b.section = (int) rawParam (s, "beat_section");
    b.variation = (int) rawParam (s, "beat_variation");
    b.energy = rawParam (s, "beat_energy");           b.density = rawParam (s, "beat_density");
    b.aggression = rawParam (s, "beat_aggression");   b.electronic = rawParam (s, "beat_electronic");
    b.trap = rawParam (s, "beat_trap");               b.glitch = rawParam (s, "beat_glitch");
    b.halfTime = (int) rawParam (s, "beat_halftime"); b.doubleKick = (int) rawParam (s, "beat_doublekick");
    b.bars = (int) rawParam (s, "beat_bars") + 1;
    b.modernity = rawParam (s, "beat_modernity");  b.syncopation = rawParam (s, "beat_syncopation");
    b.space = rawParam (s, "beat_space");          b.complexity = rawParam (s, "beat_complexity");
    b.root = (int) rawParam (s, mm::ParamIDs::keyRoot); b.scale = (int) rawParam (s, mm::ParamIDs::keyScale);
    b.seed = (int) s.state.getProperty ("beatSeed", 1);
    b.locks = locks;
    return b;
}

uint32_t MetalMachineAudioProcessor::getBeatLocks() const { return (uint32_t) (int) apvts.state.getProperty ("beatLocks", 0); }
void MetalMachineAudioProcessor::setBeatLocks (uint32_t m) { apvts.state.setProperty ("beatLocks", (int) m, nullptr); }

void MetalMachineAudioProcessor::generateBeat (bool newSeed)
{
    if (newSeed)
        apvts.state.setProperty ("beatSeed", (int) apvts.state.getProperty ("beatSeed", 1) + 1, nullptr);
    const int len = mm::BeatGenerator::generate (drumPattern, readBeatSettings (apvts, getBeatLocks()));
    setParamReal (mm::ParamIDs::drumLength, (float) len);
    setParamReal (mm::ParamIDs::drumOn, 1.0f);
}

void MetalMachineAudioProcessor::makeBeatFill()
{
    mm::BeatGenerator::makeFill (drumPattern, (int) apvts.getRawParameterValue (mm::ParamIDs::drumLength)->load(),
                                 readBeatSettings (apvts, getBeatLocks()));
}

void MetalMachineAudioProcessor::randomizeBeat()
{
    auto b = readBeatSettings (apvts, getBeatLocks());
    const int mode = (int) rawParam (apvts, "beat_randmode");            // 0 subtle, 1 medium, 2 extreme
    const float amt = mode == 0 ? 0.1f : mode == 1 ? 0.25f : 0.5f;
    auto jitter = [&] (float v) { return juce::jlimit (0.0f, 1.0f, v + (random.nextFloat() * 2.0f - 1.0f) * amt); };
    b.seed = random.nextInt (1000000) + 1;
    b.energy = jitter (b.energy); b.density = jitter (b.density); b.aggression = jitter (b.aggression);
    if (mode >= 1) { b.electronic = jitter (b.electronic); b.trap = jitter (b.trap); b.glitch = jitter (b.glitch); }
    if (mode == 2) { b.doubleKick = random.nextInt (5); b.halfTime = random.nextInt (3); }
    apvts.state.setProperty ("beatSeed", b.seed, nullptr);
    const int len = mm::BeatGenerator::generate (drumPattern, b);   // estilo, secao, BPM e KEY preservados
    setParamReal (mm::ParamIDs::drumLength, (float) len);
    setParamReal (mm::ParamIDs::drumOn, 1.0f);
}

void MetalMachineAudioProcessor::applyBeatTransform (int kind)
{
    mm::BeatGenerator::transform (drumPattern, (int) apvts.getRawParameterValue (mm::ParamIDs::drumLength)->load(), kind,
                                  readBeatSettings (apvts, getBeatLocks()));
    setParamReal (mm::ParamIDs::drumOn, 1.0f);
}

// ============================================================ SONG GENERATOR
int MetalMachineAudioProcessor::getSongSeed() const { return (int) apvts.state.getProperty ("songSeed", 48291); }
uint32_t MetalMachineAudioProcessor::getSongLocks() const { return (uint32_t) (int) apvts.state.getProperty ("songLocks", 0); }
void MetalMachineAudioProcessor::setSongLocks (uint32_t m) { apvts.state.setProperty ("songLocks", (int) m, nullptr); }

void MetalMachineAudioProcessor::generateSong (bool newSeed)
{
    if (newSeed)
        apvts.state.setProperty ("songSeed", random.nextInt (99999) + 1, nullptr);
    mm::SongSettings st;
    st.bpm = tempo.getBpm();
    st.root = (int) rawParam (apvts, mm::ParamIDs::keyRoot);  st.scale = (int) rawParam (apvts, mm::ParamIDs::keyScale);
    st.style = (int) rawParam (apvts, "song_style");           st.mood = (int) rawParam (apvts, "song_mood");
    st.structure = (int) rawParam (apvts, "song_structure");
    st.heaviness = rawParam (apvts, "song_heaviness");         st.electronic = rawParam (apvts, "song_electronic");
    st.trap = rawParam (apvts, "song_trap");                   st.darkness = rawParam (apvts, "song_darkness");
    st.melodic = rawParam (apvts, "song_melodic");             st.modernity = rawParam (apvts, "song_modernity");
    st.texture = rawParam (apvts, "song_texture");             st.fxDensity = rawParam (apvts, "song_fx");
    st.vocalSpace = rawParam (apvts, "song_vocalspace") > 0.5f; st.quickIdea = rawParam (apvts, "song_quick") > 0.5f;
    st.seed = getSongSeed();
    song = mm::SongGenerator::generate (st, song.valid ? &song : nullptr, getSongLocks());
}

void MetalMachineAudioProcessor::previewSongSection (int index)
{
    if (! song.valid || ! juce::isPositiveAndBelow (index, (int) song.sections.size())) return;
    const auto& sec = song.sections[(size_t) index];
    // BATERIA + 808: o mesmo padrao do MIDI
    mm::SongGenerator::sectionPattern (drumPattern, song, sec);
    setParamReal (mm::ParamIDs::drumLength, 32.0f);
    setParamReal (mm::ParamIDs::drumOn, 1.0f);
    setParamReal (mm::ParamIDs::drumMode, 0.0f);                       // toca com o play do REAPER
    // SYNTH RIFF da secao no step sequencer (graus da escala a partir do acorde da secao)
    const int chord = song.chordDegree[(size_t) sec.startBar];
    for (int i = 0; i < mm::SequencePattern::maxSteps; ++i)
    {
        auto& stp = pattern.steps[(size_t) i];
        const int k = i % 16;
        stp.degree.store (chord + sec.riffDegree[(size_t) k]);
        stp.on.store (sec.riffOn[(size_t) k] != 0);
        stp.velocity.store (k % 4 == 0 ? 0.95f : 0.75f);
        stp.gate.store (0.6f);
        stp.accent.store (k % 4 == 0);
        stp.slide.store (false);
    }
    pattern.name = "SONG " + mm::Song::secNames()[sec.type];
    pattern.touch();
    setParamReal (mm::ParamIDs::seqLength, 16.0f);
    setParamReal (mm::ParamIDs::seqMode, 1.0f);                         // Sequence
    setParamReal (mm::ParamIDs::seqTrigger, 2.0f);                      // Transport
    setParamReal (mm::ParamIDs::seqRate, 2.0f);                         // 1/16
    const bool heavy = sec.type == mm::Song::Breakdown || sec.type == mm::Song::Drop;
    setParamReal (mm::ParamIDs::seqOctave, heavy ? -1.0f : 0.0f);
}

// ============================================================ PRODUCER MODE: INSPIRE ME
juce::String MetalMachineAudioProcessor::inspireMe()
{
    // combinacoes coerentes (estilo do Song Generator, clima, tonalidade, BPM sugerido, categoria do synth)
    struct Idea { int songStyle, mood, root, scale; int bpm; const char* synthCat; float heavy, electr, trap, dark; };
    static const Idea ideas[] {
        { 0,  0, 6, 0, 150, "LEADS",      0.85f, 0.65f, 0.35f, 0.80f },   // Modern Metalcore, Dark, F# menor
        { 11, 2, 2, 0, 140, "PADS",       0.70f, 0.90f, 0.30f, 0.60f },   // Electronicore, Epic, D menor
        { 3,  3, 1, 0, 145, "TRAP",       0.85f, 0.70f, 0.85f, 0.80f },   // Trap Metalcore, Aggressive, C# menor
        { 15, 5, 4, 2, 130, "BASS",       0.90f, 0.60f, 0.20f, 0.85f },   // Industrial Modern Metal, Tense, E frigio
        { 12, 0, 7, 1, 160, "DUBSTEP",    0.95f, 0.75f, 0.50f, 0.95f },   // Darkcore, Dark, G menor harmonica
        { 5,  6, 9, 0, 120, "ATMOSPHERE", 0.70f, 0.55f, 0.20f, 0.70f },   // Cinematic Metalcore, Cinematic, A menor
        { 14, 3, 11, 0, 150, "TRAP",      0.75f, 0.70f, 0.90f, 0.70f },   // Modern Trap Rock, Aggressive, B menor
        { 13, 2, 3, 0, 155, "LEADS",      0.80f, 0.90f, 0.40f, 0.75f },   // Cyber Metal, Epic, D# menor
        { 7,  1, 0, 4, 135, "PLUCKS",     0.65f, 0.60f, 0.25f, 0.55f },   // Emotional Metalcore, Emotional, C dorico
    };
    const auto& idea = ideas[random.nextInt ((int) std::size (ideas))];
    auto& pm = getPresetManager();
    // 1) synth: um preset da categoria que combina (carregar preset zera os parametros -> vem primeiro)
    auto list = pm.filter (idea.synthCat, {});
    juce::String synthName = "INIT";
    if (! list.isEmpty())
    {
        const int idx = list[random.nextInt (list.size())];
        pm.load (idx);
        synthName = pm.getPresets().getReference (idx).name;
    }
    // 2) tonalidade + DNA da musica
    setParamReal (mm::ParamIDs::keyRoot, (float) idea.root);
    setParamReal (mm::ParamIDs::keyScale, (float) idea.scale);
    setParamReal ("song_style", (float) idea.songStyle);   setParamReal ("song_mood", (float) idea.mood);
    setParamReal ("song_heaviness", idea.heavy);           setParamReal ("song_electronic", idea.electr);
    setParamReal ("song_trap", idea.trap);                 setParamReal ("song_darkness", idea.dark);
    setParamReal ("song_modernity", 0.9f);
    // 3) beat no estilo correspondente + musica completa
    setParamReal ("beat_style", (float) mm::SongGenerator::beatStyleFor (idea.songStyle));
    setParamReal ("beat_section", 3.0f);                   // Chorus
    setParamReal ("beat_trap", idea.trap);                 setParamReal ("beat_electronic", idea.electr);
    setParamReal ("beat_aggression", idea.heavy);          setParamReal ("beat_modernity", 0.9f);
    generateBeat (true);
    generateSong (true);
    static const char* notes[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return mm::Song::styleNames()[idea.songStyle] + " | " + mm::Song::moodNames()[idea.mood] + " | KEY " + notes[idea.root] + " "
         + mm::scaleNames()[idea.scale] + " | BPM sugerido " + juce::String (idea.bpm) + " | SYNTH: " + synthName
         + " | BEAT: " + drumPattern.name + " | SONG seed " + juce::String (getSongSeed());
}

juce::File MetalMachineAudioProcessor::exportSongMidi (bool toTempFolder)
{
    if (! song.valid) generateSong (false);
    const auto midi = mm::SongGenerator::toMidi (song);
    const auto name = juce::File::createLegalFileName ("SONG " + mm::Song::styleNames()[song.settings.style] + " seed "
                                                       + juce::String (song.settings.seed) + " " + juce::String (juce::roundToInt (song.settings.bpm)) + "bpm");
    const auto folder = toTempFolder ? juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ATDT MIDI")
                                     : mm::PresetManager::getBaseFolder().getChildFile ("MIDI");
    const auto file = folder.getChildFile (name + ".mid");
    return mm::BeatMidi::write (midi, file) ? file : juce::File();
}

// ============================================================ REFERENCE LAB
void MetalMachineAudioProcessor::setReference (const mm::ReferenceResult& r)
{
    reference = r;
    if (r.ok) apvts.state.setProperty ("refAnalysis", juce::JSON::toString (r.toVar(), true), nullptr);   // volta com o projeto
}

bool MetalMachineAudioProcessor::loadScratchSample (const juce::File& f, juce::String& error)
{
    if (! scratch.loadUserSample (f, error)) return false;
    apvts.state.setProperty ("scratchFile", f.getFullPathName(), nullptr);
    setParamReal ("scr_source", 4.0f);
    return true;
}

void MetalMachineAudioProcessor::applyReferenceKey()
{
    if (! reference.ok) return;
    setParamReal (mm::ParamIDs::keyRoot, (float) reference.keyRoot);
    setParamReal (mm::ParamIDs::keyScale, reference.keyMinor ? 0.0f : 6.0f);   // Minor / Major
}

void MetalMachineAudioProcessor::generateGrooveFromReference()
{
    if (! reference.ok) return;
    const auto& r = reference;
    applyReferenceKey();
    // estilo pelo carater ritmico medido (nunca copia o ritmo: o gerador cria um padrao novo)
    const int style = r.trap > 0.55f ? (r.aggression > 0.6f ? mm::Beat::Trapcore : mm::Beat::ModernTrap)
                    : r.electronic > 0.6f ? mm::Beat::ModernElectronicore
                    : r.halfTime > 0.5f ? mm::Beat::IndustrialMetalcore : mm::Beat::Metalcore;
    setParamReal ("beat_style", (float) style);
    setParamReal ("beat_trap", r.trap);                 setParamReal ("beat_electronic", r.electronic);
    setParamReal ("beat_density", juce::jlimit (0.1f, 1.0f, 0.3f + 0.6f * r.hatDensity));
    setParamReal ("beat_aggression", r.aggression);     setParamReal ("beat_syncopation", r.syncopation);
    setParamReal ("beat_energy", juce::jlimit (0.2f, 1.0f, 0.4f + 0.6f * r.aggression));
    setParamReal ("beat_halftime", r.halfTime > 0.55f ? 2.0f : 0.0f);
    setParamReal ("beat_space", juce::jlimit (0.0f, 1.0f, 0.6f - 0.6f * r.density));
    generateBeat (true);
}

void MetalMachineAudioProcessor::generateSongFromReference()
{
    if (! reference.ok) return;
    const auto& r = reference;
    applyReferenceKey();
    setParamReal ("song_heaviness", r.aggression);      setParamReal ("song_electronic", r.electronic);
    setParamReal ("song_trap", r.trap);                 setParamReal ("song_darkness", r.darkness);
    setParamReal ("song_texture", juce::jlimit (0.0f, 1.0f, r.stereo * 0.7f + r.density * 0.3f));
    setParamReal ("song_fx", r.impact);
    setParamReal ("song_mood", r.keyMinor ? (r.darkness > 0.6f ? 0.0f : 1.0f) : 2.0f);   // Dark / Emotional / Epic
    generateSong (true);
}

bool MetalMachineAudioProcessor::createPresetFromReference (int target, const juce::String& name)
{
    if (! reference.ok) return false;
    const auto& r = reference;
    resetToInitPatch();
    auto set = [this] (const char* id, float v) { setParamReal (id, v); };
    // RECONSTRUCAO APROXIMADA a partir da mixagem completa (nao sao os parametros originais)
    if (target == 0)   // BASS
    {
        set ("osc1_wave", r.distortion > 0.5f ? 2.0f : 1.0f); set ("osc1_level", 0.7f);
        set ("osc2_wave", 3.0f); set ("osc2_level", 0.35f * r.distortion); set ("osc3_level", 0.0f);
        set ("sub_level", juce::jlimit (0.3f, 1.0f, 0.4f + r.sub)); set ("sub_octave", -1.0f);
        set ("filter_cutoff", 250.0f + 2200.0f * r.bright); set ("filter_resonance", 0.2f);
        set ("voice_mode", 1.0f); set ("bass_mono", 150.0f);
        set ("amp_attack", 0.002f); set ("amp_release", 0.12f);
        set ("fx_dist_on", r.distortion > 0.25f ? 1.0f : 0.0f); set ("fx_dist_drive", 0.2f + 0.6f * r.distortion);
        set ("fx_cp_on", 1.0f);
    }
    else if (target == 1)   // LEAD
    {
        set ("osc1_wave", 2.0f); set ("osc1_level", 0.6f); set ("osc2_wave", 3.0f); set ("osc2_level", 0.4f); set ("osc2_fine", 7.0f);
        set ("osc3_level", 0.0f); set ("sub_level", 0.0f);
        set ("unison_voices", 1.0f + std::round (r.stereo * 4.0f)); set ("unison_spread", r.stereo);
        set ("filter_cutoff", 1500.0f + 6000.0f * r.bright); set ("voice_mode", 2.0f); set ("voice_glide", 0.05f);
        set ("fx_dist_on", r.distortion > 0.3f ? 1.0f : 0.0f); set ("fx_dist_drive", 0.15f + 0.4f * r.distortion);
        set ("fx_dl_on", 1.0f); set ("fx_dl_mix", 0.18f); set ("fx_rv_on", 1.0f); set ("fx_rv_mix", 0.12f + 0.15f * r.stereo);
    }
    else   // PAD
    {
        set ("osc1_wave", 6.0f); set ("osc1_wt", r.darkness > 0.5f ? 3.0f : 0.0f); set ("osc1_wtpos", 0.3f); set ("osc1_level", 0.55f);
        set ("osc2_wave", 2.0f); set ("osc2_level", 0.45f); set ("osc2_fine", 9.0f); set ("osc3_level", 0.0f); set ("sub_level", 0.0f);
        set ("unison_voices", 4.0f + std::round (r.stereo * 4.0f)); set ("unison_detune", 0.25f); set ("unison_spread", 1.0f);
        set ("filter_cutoff", 700.0f + 3000.0f * r.bright);
        set ("amp_attack", 0.4f + 1.4f * (1.0f - r.density)); set ("amp_release", 2.0f);
        set ("fx_ch_on", 1.0f); set ("fx_rv_on", 1.0f); set ("fx_rv_mix", 0.3f + 0.15f * r.stereo); set ("fx_rv_size", 0.8f);
    }
    set ("master_gain", -10.0f);
    return getPresetManager().saveUserPreset (name, "REFERENCE");
}

juce::File MetalMachineAudioProcessor::exportBeatMidi (bool multiTrack, bool toTempFolder)
{
    const int len = (int) apvts.getRawParameterValue (mm::ParamIDs::drumLength)->load();
    const double bpm = tempo.getBpm();
    const auto midi = mm::BeatMidi::create (drumPattern, len, bpm, (int) rawParam (apvts, mm::ParamIDs::keyRoot), multiTrack,
                                            len <= 16 ? 4 : 2);   // exporta 4 compassos
    const auto name = juce::File::createLegalFileName (drumPattern.name + " " + juce::String (juce::roundToInt (bpm)) + "bpm"
                                                       + (multiTrack ? " MULTI" : ""));
    const auto folder = toTempFolder ? juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ATDT MIDI")
                                     : mm::PresetManager::getBaseFolder().getChildFile ("MIDI");
    const auto file = folder.getChildFile (name + ".mid");
    return mm::BeatMidi::write (midi, file) ? file : juce::File();
}

void MetalMachineAudioProcessor::loadLibraryPattern (int index)
{
    const auto& lib = mm::getPatternLibrary();
    if (! juce::isPositiveAndBelow (index, (int) lib.size()))
        return;
    const auto& def = lib[(size_t) index];
    const int len = mm::applyPattern (pattern, def);
    setParamReal (mm::ParamIDs::seqLength, (float) len);
    setParamReal (mm::ParamIDs::seqRate, (float) def.rate);
    setParamReal (mm::ParamIDs::timeFeel, (float) def.feel);
}

void MetalMachineAudioProcessor::randomizeSequence()
{
    mm::randomizePattern (pattern, (int) apvts.getRawParameterValue (mm::ParamIDs::seqLength)->load(), random);
}

void MetalMachineAudioProcessor::randomizeVelocities()
{
    for (int i = 0; i < mm::SequencePattern::maxSteps; ++i)
    {
        auto& s = pattern.steps[(size_t) i];
        const bool strong = (i % 4) == 0;
        s.velocity = strong ? 0.8f + 0.2f * random.nextFloat() : 0.45f + 0.4f * random.nextFloat();
    }
    pattern.touch();
}

void MetalMachineAudioProcessor::clearSequence()
{
    pattern.setInit();
}

int MetalMachineAudioProcessor::getNumPrograms()
{
    int n = 1;   // 0 = INIT
    for (auto& p : presetManager->getPresets())
        if (p.isFactory) ++n;
    return n;
}

int MetalMachineAudioProcessor::getCurrentProgram()
{
    const int i = presetManager->getCurrentIndex();
    return (i >= 0 && presetManager->getPresets()[i].isFactory) ? presetManager->getPresets()[i].factoryIndex + 1 : 0;
}

void MetalMachineAudioProcessor::setCurrentProgram (int index)
{
    if (index <= 0)
    {
        resetToInitPatch();
        presetManager->markInit();
        return;
    }
    const auto& list = presetManager->getPresets();
    for (int i = 0; i < list.size(); ++i)
        if (list[i].isFactory && list[i].factoryIndex == index - 1)
            presetManager->load (i);
}

const juce::String MetalMachineAudioProcessor::getProgramName (int index)
{
    if (index <= 0)
        return "INIT";
    for (auto& p : presetManager->getPresets())
        if (p.isFactory && p.factoryIndex == index - 1)
            return p.category + " - " + p.name;
    return {};
}

void MetalMachineAudioProcessor::resetToInitPatch()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->getDefaultValue());
            rp->endChangeGesture();
        }
}

// Bass Mono: tudo abaixo da frequencia escolhida fica mono (canal "side" passa por
// um passa-altas de 24 dB). Mantem o grave centralizado mesmo com unison/spread.
void MetalMachineAudioProcessor::applyBassMono (juce::AudioBuffer<float>& buffer, int numSamples)
{
    const float freq = bassMonoParam->load();
    if (freq < 20.0f || buffer.getNumChannels() < 2)
    {
        if (lastBassMonoFreq >= 0.0f)
            for (auto& f : bassMonoFilters) f.reset();
        lastBassMonoFreq = -1.0f;
        return;
    }

    if (std::abs (freq - lastBassMonoFreq) > 0.01f)
    {
        for (auto& f : bassMonoFilters) f.setParameters (freq, 0.3f);   // Q ~0.707 por estagio
        lastBassMonoFreq = freq;
    }

    float* l = buffer.getWritePointer (0);
    float* r = buffer.getWritePointer (1);
    for (int i = 0; i < numSamples; ++i)
    {
        const float mid  = 0.5f * (l[i] + r[i]);
        float side = 0.5f * (l[i] - r[i]);
        side = bassMonoFilters[0].process (side, mm::StateVariableFilter::Type::HighPass);
        side = bassMonoFilters[1].process (side, mm::StateVariableFilter::Type::HighPass);
        l[i] = mid + side;
        r[i] = mid - side;
    }
}

juce::String MetalMachineAudioProcessor::getMacroName (int index) const
{
    static const char* defaults[] { "AGGRESSION", "MOVEMENT", "DARKNESS", "SPACE" };
    return apvts.state.getProperty ("macroName" + juce::String (index + 1),
                                    defaults[juce::jlimit (0, 3, index)]).toString();
}

void MetalMachineAudioProcessor::setMacroName (int index, const juce::String& name)
{
    apvts.state.setProperty ("macroName" + juce::String (index + 1), name.toUpperCase().substring (0, 16), nullptr);
}

float MetalMachineAudioProcessor::getUiScale() const
{
    return (float) apvts.state.getProperty ("uiScale", 1.0f);
}

void MetalMachineAudioProcessor::setUiScale (float scale)
{
    apvts.state.setProperty ("uiScale", scale, nullptr);
}

juce::String MetalMachineAudioProcessor::getThemeName() const
{
    return apvts.state.getProperty ("theme", "METAL").toString();
}

void MetalMachineAudioProcessor::setThemeName (const juce::String& name)
{
    apvts.state.setProperty ("theme", name, nullptr);
}

juce::AudioProcessorEditor* MetalMachineAudioProcessor::createEditor()
{
    return new MetalMachineAudioProcessorEditor (*this);
}

void MetalMachineAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("pluginVersion", JucePlugin_VersionString, nullptr);
    state.removeChild (state.getChildWithName ("MIDI_LEARN"), nullptr);
    state.appendChild (midiLearn.toValueTree(), nullptr);
    state.removeChild (state.getChildWithName ("SEQUENCE"), nullptr);
    state.appendChild (pattern.toValueTree(), nullptr);
    state.removeChild (state.getChildWithName ("DRUMS"), nullptr);
    state.appendChild (drumPattern.toValueTree(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void MetalMachineAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            midiLearn.fromValueTree (tree.getChildWithName ("MIDI_LEARN"));
            pattern.fromValueTree (tree.getChildWithName ("SEQUENCE"));
            if (tree.hasProperty ("scratchFile"))   // sample do scratch volta com o projeto
            {
                const juce::File f (tree.getProperty ("scratchFile").toString());
                juce::String err;   // carregamento direto (sem callback assincrono que poderia sobreviver ao plugin)
                if (f.existsAsFile()) scratch.loadUserSample (f, err);
            }
            if (tree.hasProperty ("refAnalysis"))
                reference = mm::ReferenceResult::fromVar (juce::JSON::parse (tree.getProperty ("refAnalysis").toString()));
            if (! drumPattern.fromValueTree (tree.getChildWithName ("DRUMS")))
                mm::setInitDrums (drumPattern);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MetalMachineAudioProcessor();
}
