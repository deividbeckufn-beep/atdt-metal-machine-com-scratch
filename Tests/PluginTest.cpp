#include <map>
// =============================================================================
//  PluginTest - host de teste em linha de comando.
//  Carrega o ATDT SYNTH MACHINE.vst3, toca todos os presets de fabrica e verifica:
//   - o plugin gera audio (nao silencio)   - nenhum NaN/Inf
//   - pico sem clip absurdo               - salvar/restaurar estado funciona
//  Uso: PluginTest "caminho/ATDT SYNTH MACHINE.vst3"
// =============================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <cmath>

using namespace juce;

struct QcExtra { double sideLowRatio = 0.0; double zcrHz = 0.0; };
static QcExtra lastQc;   // medidas extras do ultimo render (controle de qualidade da biblioteca)

static void render (AudioPluginInstance& p, int notes[], int numNotes, double seconds, float& peak, float& rms, bool& finite)
{
    const double sr = 48000.0;
    const int block = 512;
    AudioBuffer<float> buffer (2, block);
    const int totalBlocks = (int) (seconds * sr / block);
    double sumSq = 0.0;
    long count = 0;
    peak = 0.0f; finite = true;
    // passa-baixas de 1 polo em 120 Hz: energia MID e SIDE so do grave
    const double a = 1.0 - std::exp (-MathConstants<double>::twoPi * 120.0 / sr);
    double lpL = 0.0, lpR = 0.0, midE = 0.0, sideE = 0.0, prev = 0.0;
    long zc = 0, zcSamples = 0;

    for (int b = 0; b < totalBlocks; ++b)
    {
        MidiBuffer midi;
        if (b == 0)
            for (int i = 0; i < numNotes; ++i) midi.addEvent (MidiMessage::noteOn (1, notes[i], (uint8) 110), 0);
        if (b == totalBlocks * 3 / 4)
            for (int i = 0; i < numNotes; ++i) midi.addEvent (MidiMessage::noteOff (1, notes[i]), 0);

        buffer.clear();
        p.processBlock (buffer, midi);

        for (int c = 0; c < 2; ++c)
        {
            const float* d = buffer.getReadPointer (c);
            for (int i = 0; i < block; ++i)
            {
                if (! std::isfinite (d[i])) finite = false;
                peak = jmax (peak, std::abs (d[i]));
                sumSq += (double) d[i] * d[i];
                ++count;
            }
        }
        const float* L = buffer.getReadPointer (0);
        const float* R = buffer.getReadPointer (1);
        for (int i = 0; i < block; ++i)
        {
            lpL += (L[i] - lpL) * a; lpR += (R[i] - lpR) * a;
            midE += 0.25 * (lpL + lpR) * (lpL + lpR);
            sideE += 0.25 * (lpL - lpR) * (lpL - lpR);
            const double m = 0.5 * (L[i] + R[i]);
            if (b < totalBlocks * 3 / 4) { if ((m >= 0.0) != (prev >= 0.0)) ++zc; ++zcSamples; }
            prev = m;
        }
    }
    rms = (float) std::sqrt (sumSq / (double) jmax (1L, count));
    lastQc.sideLowRatio = sideE / (midE + 1.0e-12);
    lastQc.zcrHz = zcSamples > 0 ? 0.5 * (double) zc * sr / (double) zcSamples : 0.0;
}

// Simula a posicao/andamento do REAPER tocando
struct TestPlayHead : public AudioPlayHead
{
    double bpm = 120.0, ppq = 0.0;
    bool playing = true;
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        p.setIsPlaying (playing);
        return p;
    }
};

static AudioProcessorParameter* findParam (AudioPluginInstance& p, const String& name)
{
    for (auto* prm : p.getParameters())
        if (prm->getName (64) == name)
            return prm;
    return nullptr;
}

// Sequencer em TRANSPORT + MIDI OUT: cada nota deve cair exatamente na grade de 1/16 do host
static bool testSync (AudioPluginInstance& plugin, double bpm, float feelNorm, double feelMult, const char* label)
{
    const double sr = 48000.0;
    const int block = 512;
    plugin.setCurrentProgram (0);   // INIT (padrao de 16 semicolcheias)
    auto set = [&] (const char* n, float v) { if (auto* p = findParam (plugin, n)) p->setValue (v); else std::printf ("  parametro nao encontrado: %s\n", n); };
    set ("Seq Mode", 0.5f);      // Sequence
    set ("Seq Trigger", 1.0f);   // Transport
    set ("Seq Rate", 0.4f);      // 1/16
    set ("Time Feel", feelNorm);
    set ("MIDI Out", 1.0f);

    TestPlayHead ph;
    ph.bpm = bpm;
    ph.playing = false;
    plugin.setPlayHead (&ph);

    AudioBuffer<float> buffer (2, block);
    MidiBuffer midi;
    for (int i = 0; i < 20; ++i) { buffer.clear(); midi.clear(); plugin.processBlock (buffer, midi); }

    ph.playing = true;
    ph.ppq = 0.0;
    std::vector<int64> onsets;
    const int blocks = (int) (4.0 * sr / block);
    for (int b = 0; b < blocks; ++b)
    {
        buffer.clear(); midi.clear();
        plugin.processBlock (buffer, midi);
        for (const auto meta : midi)
            if (meta.getMessage().isNoteOn())
                onsets.push_back ((int64) b * block + meta.samplePosition);
        ph.ppq += block * bpm / 60.0 / sr;
    }
    ph.playing = false;
    for (int i = 0; i < 10; ++i) { buffer.clear(); midi.clear(); plugin.processBlock (buffer, midi); }
    plugin.setPlayHead (nullptr);
    set ("MIDI Out", 0.0f);
    set ("Seq Mode", 0.0f);

    const double expected = 60.0 / bpm * 0.25 / feelMult * sr;
    const int expectedCount = (int) (blocks * (double) block / expected);
    double maxErr = 0.0;
    for (size_t i = 0; i < onsets.size(); ++i)
        maxErr = jmax (maxErr, std::abs ((double) onsets[i] - (double) i * expected));
    const bool ok = ! onsets.empty() && std::abs ((int) onsets.size() - expectedCount) <= 1 && maxErr <= 1.0;
    std::printf ("%-4s Sync %s: %d notas (esperado ~%d), passo %.1f amostras, erro max %.2f amostra(s)\n",
                 ok ? "OK" : "FAIL", label, (int) onsets.size(), expectedCount, expected, maxErr);
    return ok;
}


// ---- v0.4: brilho (centroide espectral) de uma nota sustentada ----
// Prova que WT POSITION e FM AMOUNT mudam o SOM, nao so a tela.
static double renderCentroid (AudioPluginInstance& plugin, int note, bool& finite)
{
    const int block = 512;
    AudioBuffer<float> buf (2, block);
    std::vector<float> tail;
    MidiBuffer none;
    for (int i = 0; i < 30; ++i) { buf.clear(); plugin.processBlock (buf, none); }   // parametros assentam
    finite = true;
    for (int b = 0; b < 94; ++b)   // ~1 s a 48 kHz
    {
        MidiBuffer midi;
        if (b == 0) midi.addEvent (MidiMessage::noteOn (1, note, (uint8) 100), 0);
        buf.clear();
        plugin.processBlock (buf, midi);
        for (int i = 0; i < block; ++i)
        {
            if (! std::isfinite (buf.getSample (0, i))) finite = false;
            tail.push_back (buf.getSample (0, i));
        }
    }
    MidiBuffer off; off.addEvent (MidiMessage::noteOff (1, note), 0);
    buf.clear(); plugin.processBlock (buf, off);
    for (int i = 0; i < 200; ++i) { buf.clear(); plugin.processBlock (buf, none); }

    const int n = 2048;
    const size_t start = tail.size() - (size_t) n;
    double num = 0.0, den = 0.0;
    for (int k = 1; k < n / 2; ++k)
    {
        double re = 0.0, im = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (MathConstants<double>::twoPi * i / (n - 1));
            const double x = tail[start + (size_t) i] * w;
            const double a = MathConstants<double>::twoPi * k * i / n;
            re += x * std::cos (a); im -= x * std::sin (a);
        }
        const double mag = std::sqrt (re * re + im * im);
        num += mag * (k * 48000.0 / n);
        den += mag;
    }
    return den > 1.0e-9 ? num / den : 0.0;
}

static int testTimbre (AudioPluginInstance& plugin)
{
    int failures = 0;
    auto set = [&] (const char* nm, float v) { if (auto* p = findParam (plugin, nm)) p->setValue (v); else { std::printf ("  parametro nao encontrado: %s\n", nm); ++failures; } };
    plugin.setCurrentProgram (0);
    set ("OSC2 Level", 0.0f); set ("OSC3 Level", 0.0f); set ("Sub Level", 0.0f);
    set ("Filter Cutoff", 1.0f); set ("Filter Env Amount", 0.5f); set ("Filter Drive", 0.0f);
    set ("Mod 1 Source", 0.0f);   // sem MW -> cutoff

    auto check = [&] (const char* label, double a, double b)
    {
        const bool ok = b > a * 1.4;
        if (! ok) ++failures;
        std::printf ("%-4s %-34s centroide %7.0f Hz -> %7.0f Hz\n", ok ? "OK" : "FAIL", label, a, b);
    };
    bool f1, f2;

    set ("OSC1 Wave", 6.0f / 7.0f);          // Wavetable
    set ("OSC1 Wavetable", 0.0f);            // Basic Morph
    set ("OSC1 WT Position", 0.0f);
    const double wt0 = renderCentroid (plugin, 45, f1);
    set ("OSC1 WT Position", 1.0f);
    const double wt1 = renderCentroid (plugin, 45, f2);
    check ("Wavetable: WT POS 0% -> 100%", wt0, wt1);
    if (! (f1 && f2)) { ++failures; std::printf ("FAIL Wavetable gerou NaN/Inf\n"); }

    set ("OSC1 Wave", 1.0f);                 // FM
    set ("OSC1 FM Amount", 0.0f);
    const double fm0 = renderCentroid (plugin, 45, f1);
    set ("OSC1 FM Amount", 1.0f);
    const double fm1 = renderCentroid (plugin, 45, f2);
    check ("FM: AMOUNT 0% -> 100%", fm0, fm1);
    if (! (f1 && f2)) { ++failures; std::printf ("FAIL FM gerou NaN/Inf\n"); }

    // nota muito aguda com a tabela mais brilhante: deve continuar estavel (mipmaps anti-aliasing)
    set ("OSC1 Wave", 6.0f / 7.0f); set ("OSC1 Wavetable", 1.0f); set ("OSC1 WT Position", 1.0f);
    const double hi = renderCentroid (plugin, 100, f1);
    const bool hiOk = f1 && hi > 0.0;
    if (! hiOk) ++failures;
    std::printf ("%-4s Wavetable nota aguda (E7) estavel: centroide %.0f Hz\n", hiOk ? "OK" : "FAIL", hi);
    return failures;
}


// ---- v0.5: swing e transpose medidos no MIDI gerado (transport, 120 BPM, 1/16) ----
static void runTransport (AudioPluginInstance& plugin, float swingNorm, float transposeNorm,
                          std::vector<int64>& onsets, std::vector<int>& notes)
{
    const double sr = 48000.0, bpm = 120.0;
    const int block = 512;
    plugin.setCurrentProgram (0);
    auto set = [&] (const char* n, float v) { if (auto* p = findParam (plugin, n)) p->setValue (v); else std::printf ("  parametro nao encontrado: %s\n", n); };
    set ("Seq Mode", 0.5f); set ("Seq Trigger", 1.0f); set ("Seq Rate", 0.4f); set ("Time Feel", 0.5f);
    set ("MIDI Out", 1.0f); set ("Seq Swing", swingNorm); set ("Seq Transpose", transposeNorm);

    TestPlayHead ph; ph.bpm = bpm; ph.playing = false;
    plugin.setPlayHead (&ph);
    AudioBuffer<float> buffer (2, block);
    MidiBuffer midi;
    for (int i = 0; i < 20; ++i) { buffer.clear(); midi.clear(); plugin.processBlock (buffer, midi); }
    ph.playing = true; ph.ppq = 0.0;
    for (int b = 0; b < (int) (2.0 * sr / block); ++b)
    {
        buffer.clear(); midi.clear();
        plugin.processBlock (buffer, midi);
        for (const auto meta : midi)
            if (meta.getMessage().isNoteOn())
            {
                onsets.push_back ((int64) b * block + meta.samplePosition);
                notes.push_back (meta.getMessage().getNoteNumber());
            }
        ph.ppq += block * bpm / 60.0 / sr;
    }
    ph.playing = false;
    for (int i = 0; i < 10; ++i) { buffer.clear(); midi.clear(); plugin.processBlock (buffer, midi); }
    plugin.setPlayHead (nullptr);
    set ("MIDI Out", 0.0f); set ("Seq Mode", 0.0f); set ("Seq Swing", 0.0f); set ("Seq Transpose", 0.5f);
}

static int testSwingTranspose (AudioPluginInstance& plugin)
{
    int failures = 0;
    const double step = 60.0 / 120.0 * 0.25 * 48000.0;   // 6000 amostras por 1/16

    std::vector<int64> on0, onS; std::vector<int> n0, nT; std::vector<int64> dummy;
    runTransport (plugin, 0.0f, 0.5f, on0, n0);   // referencia: sem swing, transpose 0
    runTransport (plugin, 1.0f, 0.5f, onS, nT);   // swing 100%

    double maxErr = 0.0;
    for (size_t i = 0; i < onS.size(); ++i)
    {
        const double expected = (double) i * step + ((i & 1) ? step / 3.0 : 0.0);
        maxErr = jmax (maxErr, std::abs ((double) onS[i] - expected));
    }
    const bool swingOk = onS.size() == 16 && maxErr <= 1.0;   // 2 s a 120 BPM em 1/16 = 16 notas
    if (! swingOk) ++failures;
    std::printf ("%-4s Swing 100%%: contratempos +%.0f amostras (1/3 de passo), %d notas, erro max %.2f\n",
                 swingOk ? "OK" : "FAIL", step / 3.0, (int) onS.size(), maxErr);

    nT.clear(); dummy.clear();
    runTransport (plugin, 0.0f, 0.75f, dummy, nT);   // transpose +12 st (normalizado 36/48)
    bool trOk = ! n0.empty() && n0.size() == nT.size();
    for (size_t i = 0; trOk && i < n0.size(); ++i)
        trOk = nT[i] == n0[i] + 12;
    if (! trOk) ++failures;
    std::printf ("%-4s Transpose +12: nota %d -> %d em todas as %d notas\n", trOk ? "OK" : "FAIL",
                 n0.empty() ? -1 : n0[0], nT.empty() ? -1 : nT[0], (int) nT.size());
    return failures;
}


// ---- v0.6: efeitos medidos no audio ----
// Toca um acorde por holdSec; mede o pico total e o RMS de 0.6 a 1.2 s DEPOIS do note-off (cauda).
static void renderTail (AudioPluginInstance& plugin, double holdSec, float& peak, double& tailRms)
{
    const int block = 512; const double sr = 48000.0;
    AudioBuffer<float> buf (2, block);
    MidiBuffer none;
    for (int i = 0; i < 30; ++i) { buf.clear(); plugin.processBlock (buf, none); }
    const int notes[] { 52, 55, 59 };
    const int holdBlocks = (int) (holdSec * sr / block), tailStart = holdBlocks + (int) (0.6 * sr / block),
              tailEnd = holdBlocks + (int) (1.2 * sr / block);
    peak = 0.0f; double sum = 0.0; long cnt = 0;
    for (int b = 0; b < tailEnd; ++b)
    {
        MidiBuffer m;
        if (b == 0)          for (int n : notes) m.addEvent (MidiMessage::noteOn (1, n, (uint8) 110), 0);
        if (b == holdBlocks) for (int n : notes) m.addEvent (MidiMessage::noteOff (1, n), 0);
        buf.clear(); plugin.processBlock (buf, m);
        peak = jmax (peak, buf.getMagnitude (0, block), buf.getMagnitude (1, 0, block));
        if (b >= tailStart)
            for (int i = 0; i < block; ++i) { const float v = buf.getSample (0, i); sum += (double) v * v; ++cnt; }
    }
    for (int i = 0; i < 700; ++i) { buf.clear(); plugin.processBlock (buf, none); }   // limpa caudas longas
    tailRms = std::sqrt (sum / (double) jmax (1L, cnt));
}

static int testFx (AudioPluginInstance& plugin)
{
    int failures = 0;
    auto set = [&] (const char* nm, float v) { if (auto* p = findParam (plugin, nm)) p->setValue (v); else { std::printf ("  parametro nao encontrado: %s\n", nm); ++failures; } };
    auto result = [&] (bool ok, const String& text) { if (! ok) ++failures; std::printf ("%-4s %s\n", ok ? "OK" : "FAIL", text.toRawUTF8()); };
    float pk; double dry, wet;

    plugin.setCurrentProgram (0);
    renderTail (plugin, 0.5, pk, dry);
    set ("FX Reverb On", 1.0f); set ("FX Reverb Size", 0.9f); set ("FX Reverb Mix", 0.6f);
    renderTail (plugin, 0.5, pk, wet);
    result (wet > dry * 20.0 && wet > 1.0e-3, "Reverb: cauda apos a nota  RMS " + String (dry, 6) + " -> " + String (wet, 4));
    set ("FX Reverb On", 0.0f);

    plugin.setCurrentProgram (0);
    set ("FX Delay On", 1.0f); set ("FX Delay Feedback", 0.55f); set ("FX Delay Mix", 0.7f);
    renderTail (plugin, 0.3, pk, wet);
    result (wet > dry * 20.0 && wet > 1.0e-3, "Delay: repeticoes apos a nota  RMS " + String (dry, 6) + " -> " + String (wet, 4));
    set ("FX Delay On", 0.0f);

    plugin.setCurrentProgram (0);
    set ("OSC1 Wave", 0.0f); set ("OSC2 Level", 0.0f); set ("OSC3 Level", 0.0f); set ("Sub Level", 0.0f);
    set ("Filter Cutoff", 1.0f); set ("Filter Env Amount", 0.5f); set ("Filter Drive", 0.0f); set ("Mod 1 Source", 0.0f);
    bool f1, f2;
    const double c0 = renderCentroid (plugin, 45, f1);
    set ("FX Dist On", 1.0f); set ("FX Dist Type", 1.0f / 3.0f); set ("FX Dist Drive", 1.0f); set ("FX Dist Tone", 1.0f);
    const double c1 = renderCentroid (plugin, 45, f2);
    result (f1 && f2 && c1 > c0 * 3.0, "Distortion (seno + Hard): centroide " + String (c0, 0) + " Hz -> " + String (c1, 0) + " Hz");
    set ("FX Dist On", 0.0f);

    plugin.setCurrentProgram (0);
    set ("Master Gain", 1.0f);   // +18 dB
    set ("FX Limiter On", 0.0f);
    renderTail (plugin, 0.5, pk, wet);
    const float peakOff = pk;
    set ("FX Limiter On", 1.0f);
    renderTail (plugin, 0.5, pk, wet);
    result (peakOff > 1.0f && pk <= 0.9661f, "Limiter com master +18 dB: pico " + String (peakOff, 2) + " -> " + String (pk, 4) + " (teto -0.3 dBFS = 0.9661)");
    set ("Master Gain", 54.0f / 78.0f);   // volta para -6 dB (nao depende da troca de programa)
    set ("FX Limiter On", 1.0f);
    plugin.setCurrentProgram (0);
    return failures;
}


#include "Drums/DrumEngine.h"   // teste unitario do motor de bateria

static int drumIsolation()   // motor isolado: nivel de cada canal e de todos juntos
{
    int failuresExtra = 0;
    std::atomic<float> on {1}, mode {1}, length {16}, swing {0}, volume {-3.0f}, keys {0};
    std::atomic<float> lvl[mm::DrumRow::Count], pan[mm::DrumRow::Count], pit[mm::DrumRow::Count], dec[mm::DrumRow::Count];
    mm::DrumEngine::Params P;
    P.on=&on; P.mode=&mode; P.length=&length; P.swing=&swing; P.volume=&volume; P.keys=&keys;
    for (int r=0;r<mm::DrumRow::Count;++r){ lvl[r]=0.0f; pan[r]=0.0f; pit[r]=0.0f; dec[r]=1.0f; P.level[r]=&lvl[r]; P.pan[r]=&pan[r]; P.pitch[r]=&pit[r]; P.decay[r]=&dec[r]; }
    mm::DrumPattern pat; mm::setInitDrums (pat);
    float results[5] {}; int idx = 0;
    for (int keep : { 0, 1, 3, 7, -1 })
    {
        for (int r=0;r<mm::DrumRow::Count;++r) lvl[r] = (keep < 0 || r == keep) ? 0.8f : 0.0f;
        mm::DrumEngine e (P, pat); e.prepare (48000.0);
        AudioBuffer<float> b (2, 512); float pk = 0;
        for (int blk=0; blk<94; ++blk){ b.clear(); e.process (b, 512, 120.0, false, 0.0, 1.0, 0.0f, 0.0f); pk = jmax (pk, b.getMagnitude (0, 512)); }
        results[idx++] = pk;
    }
    {   // 808: golpe unico em E (KEY) -> ~41.2 Hz (E1), sem NaN
        std::atomic<float> rootE { 4.0f }; P.root = &rootE;
        std::atomic<float> type808 { 0.0f }, drive808 { 0.2f }, glide808 { 0.08f };
        P.type808 = &type808; P.drive808 = &drive808; P.glide808 = &glide808;
        for (int r=0;r<mm::DrumRow::Count;++r) lvl[r] = r == mm::DrumRow::Bass808 ? 0.8f : 0.0f;
        mm::DrumPattern pat808; pat808.clear(); pat808.cells[mm::DrumRow::Bass808][0].store (110);
        mm::DrumEngine e (P, pat808); e.prepare (48000.0);
        AudioBuffer<float> b (2, 512); std::vector<float> out; bool fin = true;
        for (int blk = 0; blk < 60; ++blk) { b.clear(); e.process (b, 512, 120.0, false, 0.0, 1.0, 0.0f, 0.0f);
            for (int i = 0; i < 512; ++i) { out.push_back (b.getSample (0, i)); fin = fin && std::isfinite (b.getSample (0, i)); } }
        int zc = 0; for (size_t i = 4800; i < 24000; ++i) zc += (out[i] >= 0.0f) != (out[i - 1] >= 0.0f);   // 0.1 s .. 0.5 s
        const double hz = zc / 2.0 / (19200.0 / 48000.0);
        float pk = 0; for (auto x : out) pk = jmax (pk, std::abs (x));
        const bool ok808 = fin && pk > 0.05f && std::abs (hz - 41.2) < 3.0;
        if (! ok808) ++failuresExtra;
        std::printf ("%-4s 808 em E: %.1f Hz (esperado 41.2 = E1), pico %.2f\n", ok808 ? "OK" : "FAIL", hz, pk);
        // os 8 tipos de 808: todos soam, sem NaN, sem clip
        String line; bool allOk = true;
        static const char* tn[] { "Clean", "Sub", "Distorted", "Digital", "Metal", "Reese", "Short", "Long" };
        for (int ty = 0; ty < 8; ++ty)
        {
            type808 = (float) ty;
            mm::DrumEngine e2 (P, pat808); e2.prepare (48000.0);
            float pk2 = 0.0f; bool f2 = true;
            for (int blk = 0; blk < 40; ++blk) { b.clear(); e2.process (b, 512, 120.0, false, 0.0, 1.0, 0.0f, 0.0f);
                pk2 = jmax (pk2, b.getMagnitude (0, 512)); for (int i = 0; i < 512; ++i) f2 = f2 && std::isfinite (b.getSample (0, i)); }
            line << tn[ty] << " " << String (pk2, 2) << "  ";
            allOk = allOk && f2 && pk2 > 0.03f && pk2 < 1.0f;
        }
        if (! allOk) ++failuresExtra;
        std::printf ("%-4s 808 tipos: %s\n", allOk ? "OK" : "FAIL", line.trim().toRawUTF8());
    }
    const bool ok = results[0] > 0.05f && results[1] > 0.05f && results[2] > 0.02f && results[3] > 0.05f
                    && results[4] > 0.1f && results[4] < 0.9f;
    std::printf ("%-4s DrumEngine isolado (-3 dB): kick %.2f snare %.2f hat %.2f crash %.2f | todos juntos %.2f\n",
                 ok ? "OK" : "FAIL", results[0], results[1], results[2], results[3], results[4]);
    return (ok ? 0 : 1) + failuresExtra;
}

// ---- v0.7: bateria sintetizada ----
static float renderPeak (AudioPluginInstance& plugin, int note, int blocks, TestPlayHead* ph = nullptr)
{
    const int block = 512;
    AudioBuffer<float> buf (2, block);
    float peak = 0.0f;
    for (int b = 0; b < blocks; ++b)
    {
        MidiBuffer m;
        if (b == 0 && note >= 0) m.addEvent (MidiMessage::noteOn (1, note, (uint8) 110), 0);
        if (b == 4 && note >= 0) m.addEvent (MidiMessage::noteOff (1, note), 0);
        buf.clear(); plugin.processBlock (buf, m);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < block; ++i)
                if (std::isfinite (buf.getSample (c, i))) peak = jmax (peak, std::abs (buf.getSample (c, i)));
                else return -1.0f;   // NaN/Inf
        if (ph != nullptr) ph->ppq += block * ph->bpm / 60.0 / 48000.0;
    }
    return peak;
}

static int testDrums (AudioPluginInstance& plugin)
{
    int failures = 0;
    auto set = [&] (const char* nm, float v) { if (auto* p = findParam (plugin, nm)) p->setValue (v); else { std::printf ("  parametro nao encontrado: %s\n", nm); ++failures; } };
    auto result = [&] (bool ok, const String& text) { if (! ok) ++failures; std::printf ("%-4s %s\n", ok ? "OK" : "FAIL", text.toRawUTF8()); };
    AudioBuffer<float> b (2, 512); MidiBuffer none;
    auto settle = [&] { for (int i = 0; i < 40; ++i) { b.clear(); plugin.processBlock (b, none); } };

    // 1) sem notas: desligada = silencio; ALWAYS = toca sozinha (padrao Basic Rock)
    plugin.setCurrentProgram (0); settle();
    const float off = renderPeak (plugin, -1, 94);
    set ("Drums On", 1.0f); set ("Drums Play", 1.0f); settle();
    const float always = renderPeak (plugin, -1, 94);
    result (off < 1.0e-5f && always > 0.05f, "Bateria sem notas: desligada pico " + String (off, 5) + " | ALWAYS pico " + String (always, 3));
    // 2) TRANSPORT: parada = silencio; com play do host = toca
    set ("Drums Play", 0.0f);
    for (int i = 0; i < 500; ++i) { b.clear(); plugin.processBlock (b, none); }   // caudas dos pratos terminam
    const float stopped = renderPeak (plugin, -1, 94);
    TestPlayHead ph; ph.bpm = 140.0; ph.playing = true; ph.ppq = 0.0;
    plugin.setPlayHead (&ph);
    const float playing = renderPeak (plugin, -1, 94, &ph);
    plugin.setPlayHead (nullptr);
    // silencio = abaixo de -60 dBFS (cauda de prato inaudivel)
    result (stopped < 1.0e-3f && playing > 0.05f, "TRANSPORT: parado pico " + String (stopped, 5) + " | play do REAPER pico " + String (playing, 3));

    // 3) kit: o teclado toca cada instrumento; nota fora do mapa nao soa (synth mudo)
    int kit = -1;
    for (int i = 0; i < plugin.getNumPrograms(); ++i)
        if (plugin.getProgramName (i).contains ("Metal Kit - Basic Rock")) kit = i;
    result (kit >= 0, "Preset de kit encontrado na lista de programas");
    if (kit >= 0)
    {
        plugin.setCurrentProgram (kit); settle();
        const int notes[] { 36, 38, 39, 42, 46, 37, 45, 49, 51, 52 };
        const char* names[] { "kick", "snare", "clap", "hat", "open hat", "perc", "tom", "crash", "ride", "fx" };
        String line; bool all = true;
        for (int i = 0; i < 10; ++i)
        {
            const float pk = renderPeak (plugin, notes[i], 60);
            settle();
            line << names[i] << " " << String (pk, 2) << "  ";
            all = all && pk > 0.02f && pk < 4.0f;
        }
        result (all, "10 instrumentos pelo teclado: " + line.trim());
        for (int i = 0; i < 500; ++i) { b.clear(); plugin.processBlock (b, none); }
        const float c4 = renderPeak (plugin, 60, 60);
        result (c4 < 1.0e-3f, "Nota fora do mapa (C4) no kit: pico " + String (c4, 5) + " (synth mudo, < -60 dBFS)");
    }
    plugin.setCurrentProgram (0);
    return failures;
}


#include "Drums/BeatGenerator.h"   // testes do BEAT ENGINE
#include "Drums/BeatMidi.h"

static int beatTests()
{
    int failures = 0;
    auto result = [&] (bool ok, const String& text) { if (! ok) ++failures; std::printf ("%-4s %s\n", ok ? "OK" : "FAIL", text.toRawUTF8()); };
    using namespace mm;
    int cases = 0, badKick = 0, badSnare = 0, badScale = 0, machineGun = 0, emptyHats = 0;
    for (int st = 0; st < Beat::NumStyles; ++st)
        for (int sec = 0; sec < Beat::NumSections; ++sec)
        {
            DrumPattern p; BeatSettings b; b.style = st; b.section = sec; b.seed = 11 + st * 7 + sec; b.root = 6; b.scale = 0;
            const int len = BeatGenerator::generate (p, b);
            ++cases;
            const auto& prof = scaleDef (b.scale);
            for (int bar = 0; bar < len / 16; ++bar)
            {
                const int o = bar * 16;
                if (sec == Beat::PreDrop && bar == len / 16 - 1) continue;   // pre-drop: a bateria sai de proposito (contraste)
                if (p.cells[DrumRow::Kick][(size_t) o].load() == 0) { ++badKick; std::printf ("     (sem kick no 1: %s / %s, compasso %d)\n", Beat::styleNames()[st].toRawUTF8(), Beat::sectionNames()[sec].toRawUTF8(), bar + 1); }
                const bool half = p.cells[DrumRow::Snare][(size_t) (o + 8)].load() >= 100 && p.cells[DrumRow::Snare][(size_t) (o + 4)].load() < 100;
                const bool full = p.cells[DrumRow::Snare][(size_t) (o + 4)].load() >= 100 && p.cells[DrumRow::Snare][(size_t) (o + 12)].load() >= 100;
                if (! half && ! full) { ++badSnare; std::printf ("     (caixa fora: %s / %s, compasso %d)\n", Beat::styleNames()[st].toRawUTF8(), Beat::sectionNames()[sec].toRawUTF8(), bar + 1); }   // diagnostico so em falha
                int kicks = 0; for (int i = 0; i < 16; ++i) kicks += p.cells[DrumRow::Kick][(size_t) (o + i)].load() > 0;
                if (kicks >= 15) ++machineGun;        // double kick OFF nunca vira metralhadora
            }
            int hats = 0;
            for (int i = 0; i < len; ++i) hats += p.cells[DrumRow::ClosedHat][(size_t) i].load() > 0 || p.cells[DrumRow::Ride][(size_t) i].load() > 0
                                                  || p.cells[DrumRow::OpenHat][(size_t) i].load() > 0;
            if (hats == 0) ++emptyHats;
            for (int i = 0; i < len; ++i)
                if (p.cells[DrumRow::Bass808][(size_t) i].load() > 0)
                {
                    const int pc = ((p.bassNote[(size_t) i].load()) % 12 + 12) % 12;   // relativo a tonica
                    bool inScale = false;
                    for (int k = 0; k < prof.size; ++k) inScale = inScale || prof.steps[k] % 12 == pc;
                    if (! inScale) ++badScale;
                }
        }
    result (badKick == 0 && badSnare == 0, "Beat: " + String (cases) + " casos (" + String ((int) Beat::NumStyles) + " estilos x " + String ((int) Beat::NumSections) + " secoes) - kick no 1 e caixa no backbeat/half-time");
    result (badScale == 0, "Beat: 808 sempre dentro da escala (" + String (badScale) + " notas fora)");
    result (machineGun == 0, "Beat: double kick OFF nunca preenche o compasso inteiro");
    result (emptyHats == 0, "Beat: todo beat tem conducao (hats/ride)");

    {   // v0.11 SINCOPE: mais kicks no contratempo com SYNCOPATION alta
        auto offbeatRatio = [] (float syncop)
        {
            int off = 0, all = 0;
            for (int seed = 1; seed <= 40; ++seed)
            {
                DrumPattern p; BeatSettings b; b.style = Beat::Metalcore; b.syncopation = syncop; b.modernity = 0.0f; b.seed = seed; b.space = 0.0f; b.complexity = 0.0f;
                const int len = BeatGenerator::generate (p, b);
                for (int i = 0; i < len; ++i) if (p.cells[DrumRow::Kick][(size_t) i].load() >= 70) { ++all; off += (i % 2) == 1; }
            }
            return all > 0 ? (double) off / all : 0.0;
        };
        const double lo = offbeatRatio (0.0f), hi = offbeatRatio (1.0f);
        result (hi > lo + 0.1, "Beat 2.0: SYNCOPATION 0% -> 100%: kicks no contratempo " + String (lo * 100, 0) + "% -> " + String (hi * 100, 0) + "%");
    }
    {   // v0.11 SPACE: menos golpes, mas o 1 e o backbeat ficam
        auto events = [] (float sp, bool& anchorsOk)
        {
            int n = 0; anchorsOk = true;
            for (int seed = 1; seed <= 30; ++seed)
            {
                DrumPattern p; BeatSettings b; b.style = Beat::ModernTrap; b.space = sp; b.seed = seed; b.section = Beat::Chorus;
                const int len = BeatGenerator::generate (p, b);
                for (int r = 0; r < DrumRow::Count; ++r) for (int i = 0; i < len; ++i) n += p.cells[(size_t) r][(size_t) i].load() > 0;
                for (int bar = 0; bar < len / 16; ++bar)
                    anchorsOk = anchorsOk && p.cells[DrumRow::Kick][(size_t) (bar * 16)].load() > 0 && p.cells[DrumRow::Snare][(size_t) (bar * 16 + 8)].load() >= 100;
            }
            return n;
        };
        bool a0, a1; const int e0 = events (0.0f, a0), e1 = events (1.0f, a1);
        result (e1 < e0 * 0.8 && a0 && a1, "Beat 2.0: SPACE 0% -> 100%: " + String (e0) + " -> " + String (e1) + " golpes (1 e backbeat preservados)");
    }
    {   // v0.11 ROLLS INTELIGENTES: so antes da caixa ou no fim da frase
        int rolls = 0, misplaced = 0;
        for (int seed = 1; seed <= 40; ++seed)
        {
            DrumPattern p; BeatSettings b; b.style = Beat::ModernTrap; b.trap = 0.8f; b.seed = seed; b.modernity = 0.0f; b.section = Beat::Verse;
            const int len = BeatGenerator::generate (p, b);
            for (int i = 0; i < len; ++i)
                if (p.cells[DrumRow::ClosedHat][(size_t) i].load() > 0 && p.ratchet[DrumRow::ClosedHat][(size_t) i].load() > 1)
                {
                    ++rolls;
                    const bool beforeSnare = i + 1 < len && p.cells[DrumRow::Snare][(size_t) (i + 1)].load() >= 60;
                    const bool phraseEnd = i >= len - 2;
                    if (! beforeSnare && ! phraseEnd) ++misplaced;
                }
        }
        result (rolls > 20 && misplaced == 0, "Beat 2.0: " + String (rolls) + " rolls de hat, todos antes da caixa ou no fim da frase (" + String (misplaced) + " fora)");
    }
    {   // v0.11 PRE-DROP: hats aceleram e o ultimo 1/8 fica em silencio
        DrumPattern p; BeatSettings b; b.style = Beat::Trapcore; b.section = Beat::PreDrop; b.seed = 4;
        const int len = BeatGenerator::generate (p, b);
        bool silent = true; for (int r = 0; r < DrumRow::Count; ++r) for (int i = len - 2; i < len; ++i) silent = silent && p.cells[(size_t) r][(size_t) i].load() == 0;
        const int r1 = p.ratchet[DrumRow::ClosedHat][(size_t) (len - 16)].load(), r4 = p.ratchet[DrumRow::ClosedHat][(size_t) (len - 3)].load();
        result (silent && r4 > r1, "Beat 2.0: PRE-DROP hats 1/16 -> 1/64 (ratchet " + String (r1) + " -> " + String (r4) + ") e silencio no ultimo 1/8");
    }
    {   // v0.11 MAKE IT: cada transformacao faz o que promete
        DrumPattern p; BeatSettings b; b.style = Beat::Metalcore; b.seed = 12; b.complexity = 0.9f;
        const int len = BeatGenerator::generate (p, b);
        auto count = [&] (int row, int minVel = 1) { int n = 0; for (int i = 0; i < len; ++i) n += p.cells[(size_t) row][(size_t) i].load() >= minVel; return n; };
        auto ghosts = [&] { int n = 0; for (int r : { 0, 1 }) for (int i = 0; i < len; ++i) { const int v = p.cells[(size_t) r][(size_t) i].load(); n += v > 0 && v < 80; } return n; };
        const int k0 = count (DrumRow::Kick), b0 = count (DrumRow::Bass808);
        BeatGenerator::transform (p, len, 1, b);   // HEAVIER
        const bool heavier = count (DrumRow::Kick) >= k0 && p.cells[DrumRow::Crash][0].load() > 0;
        BeatGenerator::transform (p, len, 6, b);   // TIGHTER
        const bool tighter = ghosts() == 0;
        BeatGenerator::transform (p, len, 2, b);   // MORE TRAP
        int rolls = 0; for (int i = 0; i < len; ++i) rolls += p.ratchet[DrumRow::ClosedHat][(size_t) i].load() > 1;
        const bool moreTrap = rolls > 0 && count (DrumRow::Bass808) > b0 && count (DrumRow::Clap) > 0;
        BeatGenerator::transform (p, len, 4, b);   // DARKER
        bool noOct = true; for (int i = 0; i < len; ++i) noOct = noOct && (p.cells[DrumRow::Bass808][(size_t) i].load() == 0 || p.bassNote[(size_t) i].load() < 12);
        const bool darker = noOct && count (DrumRow::OpenHat) == 0;
        String snareBefore; for (int i = 0; i < len; ++i) snareBefore << (int) p.cells[DrumRow::Snare][(size_t) i].load();
        b.locks = 1u << DrumRow::Snare;
        BeatGenerator::transform (p, len, 7, b);   // GROOVIER com caixa travada
        String snareAfter; for (int i = 0; i < len; ++i) snareAfter << (int) p.cells[DrumRow::Snare][(size_t) i].load();
        result (heavier && tighter && moreTrap && darker && snareBefore == snareAfter,
                String ("Beat 2.0: MAKE IT - Heavier ") + (heavier ? "ok" : "X") + ", Tighter " + (tighter ? "ok" : "X") + ", More Trap "
                + (moreTrap ? "ok" : "X") + ", Darker " + (darker ? "ok" : "X") + ", trava respeitada " + (snareBefore == snareAfter ? "ok" : "X"));
    }
    {   // MAKE IT com LENGTH minimo (1..4 passos): nenhum indice negativo (bug corrigido na v0.11)
        bool ok = true;
        for (int len = 1; len <= 4; ++len)
            for (int kind = 0; kind < BeatGenerator::transformNames().size(); ++kind)
            {
                DrumPattern p; BeatSettings b;
                BeatGenerator::generate (p, b);
                BeatGenerator::transform (p, len, kind, b);
                ok = ok && p.cells[DrumRow::Kick][0].load() <= 127;             // executou sem travar
            }
        result (ok, "Beat 2.0: MAKE IT com LENGTH 1..4 (8 transformacoes x 4 comprimentos) executa sem travar");
    }
    {   // 808: slides aparecem no trap
        int sl = 0;
        for (int seed = 1; seed <= 20; ++seed)
        { DrumPattern p; BeatSettings b; b.style = Beat::ModernTrap; b.trap = 0.9f; b.seed = seed; const int len = BeatGenerator::generate (p, b);
          for (int i = 0; i < len; ++i) sl += p.slide808[(size_t) i].load(); }
        result (sl > 5, "Beat 2.0: 808 com slides no trap (" + String (sl) + " slides em 20 beats)");
    }
    {   // DOUBLE KICK HEAVY: compasso inteiro em 1/16 no metal
        DrumPattern p; BeatSettings b; b.style = Beat::ModernMetal; b.doubleKick = 3; b.bars = 1;
        BeatGenerator::generate (p, b);
        int k = 0; for (int i = 0; i < 16; ++i) k += p.cells[DrumRow::Kick][(size_t) i].load() > 0;
        result (k == 16, "Beat: DOUBLE KICK HEAVY = " + String (k) + "/16 semicolcheias");
    }
    {   // TRAP: rolls (ratchet) nos hats e 808 presente
        DrumPattern p; BeatSettings b; b.style = Beat::TrapMetal; b.trap = 0.8f; b.seed = 5;
        const int len = BeatGenerator::generate (p, b);
        int rolls = 0, b808 = 0;
        for (int i = 0; i < len; ++i) { rolls += p.ratchet[DrumRow::ClosedHat][(size_t) i].load() > 1; b808 += p.cells[DrumRow::Bass808][(size_t) i].load() > 0; }
        result (rolls >= 2 && b808 >= 2, "Beat: TRAP gera rolls de hat (" + String (rolls) + ") e 808 (" + String (b808) + " notas)");
    }
    {   // TRAVAS, determinismo e variacoes
        DrumPattern p; BeatSettings b; b.seed = 3;
        BeatGenerator::generate (p, b);
        String snare; for (int i = 0; i < 32; ++i) snare << (int) p.cells[DrumRow::Snare][(size_t) i].load();
        b.locks = 1u << DrumRow::Snare; b.style = Beat::DubstepMetal; b.seed = 99;
        BeatGenerator::generate (p, b);
        String after; for (int i = 0; i < 32; ++i) after << (int) p.cells[DrumRow::Snare][(size_t) i].load();
        result (snare == after, "Beat: canal travado (SNARE) nao muda ao gerar outro estilo");
        DrumPattern a1, a2, bb; BeatSettings s1; s1.seed = 42;
        BeatGenerator::generate (a1, s1); BeatGenerator::generate (a2, s1); s1.variation = 1; BeatGenerator::generate (bb, s1);
        String x1, x2, x3; for (int i = 0; i < 32; ++i) { x1 << (int) a1.cells[0][(size_t) i].load(); x2 << (int) a2.cells[0][(size_t) i].load(); x3 << (int) bb.cells[0][(size_t) i].load(); }
        String h1, h3; for (int i = 0; i < 32; ++i) { h1 << (int) a1.cells[DrumRow::ClosedHat][(size_t) i].load(); h3 << (int) bb.cells[DrumRow::ClosedHat][(size_t) i].load(); }
        result (x1 == x2 && (x1 != x3 || h1 != h3), "Beat: mesma seed = mesmo beat; variacao B = beat diferente (kick e/ou conducao)");
    }
    {   // MIDI: numero de notas = golpes x ratchet x repeticoes
        DrumPattern p; BeatSettings b; b.style = Beat::TrapMetal; b.trap = 0.9f; b.seed = 8;
        const int len = BeatGenerator::generate (p, b);
        int expected = 0;
        for (int r = 0; r < DrumRow::Count; ++r)
            for (int i = 0; i < len; ++i)
                if (p.cells[(size_t) r][(size_t) i].load() > 0) expected += p.ratchet[(size_t) r][(size_t) i].load();
        const auto single = BeatMidi::create (p, len, 150.0, 6, false, 2);
        const auto multi  = BeatMidi::create (p, len, 150.0, 6, true, 2);
        auto countOn = [] (const MidiFile& f) { int n = 0; for (int t = 0; t < f.getNumTracks(); ++t) for (auto* e : *f.getTrack (t)) n += e->message.isNoteOn(); return n; };
        const auto file = File::getSpecialLocation (File::tempDirectory).getChildFile ("atdt_test_beat.mid");
        const bool wrote = BeatMidi::write (multi, file);
        MidiFile reread; FileInputStream in (file); const bool readOk = in.openedOk() && reread.readFrom (in);
        result (countOn (single) == expected * 2 && countOn (multi) == expected * 2 && wrote && readOk && reread.getNumTracks() == multi.getNumTracks(),
                "MIDI: " + String (countOn (single)) + " notas (esperado " + String (expected * 2) + "), multitrilha com "
                + String (multi.getNumTracks() - 1) + " trilhas, arquivo gravado e relido");
        file.deleteFile();
    }
    return failures;
}

#include "Song/SongGenerator.h"   // testes do SONG GENERATOR

static int songTests()
{
    int failures = 0;
    auto result = [&] (bool ok, const String& text) { if (! ok) ++failures; std::printf ("%-4s %s\n", ok ? "OK" : "FAIL", text.toRawUTF8()); };
    using namespace mm;
    SongSettings st; st.root = 6; st.scale = 0; st.seed = 48291;
    const auto song = SongGenerator::generate (st);
    result (song.valid && song.sections.size() == 11 && song.totalBars == 72, "Song: estrutura padrao com " + String ((int) song.sections.size())
            + " secoes / " + String (song.totalBars) + " compassos (intro, verso, pre, refrao, ..., breakdown, bridge, final, outro)");
    // todas as notas melodicas na escala (F# menor)
    const auto& sc = scaleDef (st.scale);
    int out = 0, total = 0;
    for (int t : { Song::B808, Song::Bass, Song::Riff, Song::Melody, Song::Arp, Song::Pad })
        for (auto& n : song.tracks[(size_t) t])
        {
            ++total; const int pc = ((n.note - st.root) % 12 + 12) % 12; bool in = false;
            for (int k = 0; k < sc.size; ++k) in = in || sc.steps[k] == pc;
            out += in ? 0 : 1;
        }
    result (total > 500 && out == 0, "Song: " + String (total) + " notas melodicas, todas em F# menor (" + String (out) + " fora)");
    // 808 = tonica do acorde do compasso
    int bad808 = 0;
    for (auto& n : song.tracks[Song::B808])
    {
        const int bar = n.tick / SongGenerator::barTicks;
        const int chordPc = (st.root + sc.steps[(size_t) (song.chordDegree[(size_t) bar] % sc.size)]) % 12;
        bad808 += (n.note % 12) != chordPc;
    }
    result (! song.tracks[Song::B808].empty() && bad808 == 0, "Song: 808 sempre na tonica do acorde (" + String ((int) song.tracks[Song::B808].size()) + " notas)");
    // RHYTHMIC LOCK: no refrao, todo acento do motif tem kick
    const auto& ch = song.sections[3];
    int locked = 0, needed = 0;
    for (auto [stp, deg] : song.motif)
    {
        const int tk = ch.startBar * SongGenerator::barTicks + stp * SongGenerator::step;
        bool snare = false; for (auto& n : song.tracks[Song::Snare]) snare = snare || (n.tick == tk && n.note == 38 && n.vel >= 100);
        if (snare) continue;                       // acento na caixa do backbeat: a caixa ja marca
        ++needed;
        for (auto& n : song.tracks[Song::Kick]) if (n.tick == tk) { ++locked; break; }
    }
    result (needed > 0 && locked == needed, "Song: RHYTHMIC LOCK - " + String (locked) + "/" + String (needed) + " acentos do motif (fora da caixa) com kick no refrao");
    // motif reaparece: intervalos do motif presentes na melodia do refrao
    int chorusMel = 0;
    for (auto& n : song.tracks[Song::Melody]) chorusMel += n.tick >= ch.startBar * SongGenerator::barTicks && n.tick < (ch.startBar + ch.bars) * SongGenerator::barTicks;
    result (chorusMel == (int) song.motif.size() * ch.bars, "Song: motif de " + String ((int) song.motif.size()) + " notas em todos os " + String (ch.bars) + " compassos do refrao");
    // curva de energia + camadas: refrao mais denso que verso
    auto notesIn = [&] (const SongSection& sec) { int n = 0; for (auto& tr : song.tracks) for (auto& x : tr) n += x.tick >= sec.startBar * SongGenerator::barTicks && x.tick < (sec.startBar + sec.bars) * SongGenerator::barTicks; return n; };
    result (song.sections[3].energy > song.sections[1].energy && notesIn (song.sections[3]) > notesIn (song.sections[1]),
            "Song: refrao com mais energia e mais camadas que o verso (" + String (notesIn (song.sections[1])) + " -> " + String (notesIn (song.sections[3])) + " notas)");
    // determinismo + travas
    const auto again = SongGenerator::generate (st);
    SongSettings st2 = st; st2.seed = 777;
    const auto locked2 = SongGenerator::generate (st2, &song, 1u << Song::LMelody);
    auto same = [] (const std::vector<SongNote>& a, const std::vector<SongNote>& b)
    { if (a.size() != b.size()) return false; for (size_t i = 0; i < a.size(); ++i) if (a[i].tick != b[i].tick || a[i].note != b[i].note) return false; return true; };
    const bool c1 = same (song.tracks[Song::Melody], again.tracks[Song::Melody]);
    const bool c2 = same (song.tracks[Song::Melody], locked2.tracks[Song::Melody]);
    const bool c3 = ! same (song.tracks[Song::Kick], locked2.tracks[Song::Kick]);
    if (! (c1 && c2 && c3)) std::printf ("     (determinismo %d, trava %d, bateria mudou %d)\n", (int) c1, (int) c2, (int) c3);
    result (c1 && c2 && c3,
            "Song: mesma seed = mesma musica; MELODY travada mantida com seed nova (bateria muda)");
    // FX nas transicoes
    int impacts = 0, risers = 0; for (auto& n : song.tracks[Song::Fx]) { impacts += n.note == 50; risers += n.note == 48; }
    int strong = 0; for (auto& sec : song.sections) strong += sec.type == Song::Chorus || sec.type == Song::FinalChorus || sec.type == Song::Breakdown || sec.type == Song::Drop;
    result (impacts == strong && risers == strong, "Song: FX - " + String (impacts) + " impactos e " + String (risers) + " risers nas entradas de refrao/breakdown");
    // MIDI: 12 trilhas + meta, marcadores, arquivo relido
    const auto midi = SongGenerator::toMidi (song);
    const auto file = File::getSpecialLocation (File::tempDirectory).getChildFile ("atdt_song_test.mid");
    file.deleteFile(); { FileOutputStream o (file); midi.writeTo (o, 1); }
    MidiFile re; FileInputStream in (file); const bool ok = in.openedOk() && re.readFrom (in);
    int markers = 0; for (auto* e : *re.getTrack (0)) markers += e->message.isTextMetaEvent() && e->message.getMetaEventType() == 6;
    result (ok && re.getNumTracks() == 13 && markers == (int) song.sections.size(), "Song MIDI: " + String (re.getNumTracks() - 1) + " trilhas + "
            + String (markers) + " marcadores de secao, gravado e relido");
    file.deleteFile();
    return failures;
}

#include "Reference/ReferenceAnalyzer.h"   // testes do REFERENCE LAB

// faixa sintetica de gabarito: kick nos tempos, hats em 1/16, acordes de A menor (Am F C G), 20 s
static AudioBuffer<float> makeReferenceAudio (double sr, double bpm, int rootMidi, bool minor)
{
    const int n = (int) (sr * 20.0);
    AudioBuffer<float> b (2, n); b.clear();
    const double beat = 60.0 / bpm * sr;
    Random rng (7);
    const int prog[4][3] = { { 0, 3, 7 }, { -4, 0, 3 }, { 3, 7, 10 }, { -2, 2, 5 } };   // i VI III VII (relativo a tonica)
    for (int i = 0; i < n; ++i)
    {
        const double posBeat = i / beat;
        const double tb = std::fmod ((double) i, beat) / sr;                       // tempo desde o tempo
        float x = 0.9f * (float) std::sin (2.0 * MathConstants<double>::pi * (55.0 + 90.0 * std::exp (-tb * 30.0)) * tb) * (float) std::exp (-tb * 9.0);
        const double t16 = std::fmod ((double) i, beat / 4.0) / sr;
        x += 0.12f * (rng.nextFloat() * 2.0f - 1.0f) * (float) std::exp (-t16 * 120.0);
        const int chord = ((int) (posBeat / 4.0)) % 4;
        for (int k = 0; k < 3; ++k)
        {
            const int semis = minor ? prog[chord][k] : prog[chord][k];
            const double f = 440.0 * std::pow (2.0, (rootMidi + 12 + semis - 69) / 12.0);
            x += 0.07f * (float) std::sin (2.0 * MathConstants<double>::pi * f * i / sr);
        }
        // baixo pedal na tonica (como numa faixa real de metalcore)
        const double fb = 440.0 * std::pow (2.0, (rootMidi - 12 - 69) / 12.0);
        x += 0.25f * (float) std::sin (2.0 * MathConstants<double>::pi * fb * i / sr) * (float) std::exp (-std::fmod ((double) i, beat / 2.0) / sr * 4.0);
        b.setSample (0, i, x * 0.8f); b.setSample (1, i, x * 0.75f);
    }
    return b;
}

static int referenceTests()
{
    int failures = 0;
    auto result = [&] (bool ok, const String& text) { if (! ok) ++failures; std::printf ("%-4s %s\n", ok ? "OK" : "FAIL", text.toRawUTF8()); };
    static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const double sr = 44100.0;
    // arquivo real (WAV) -> analise -> cache
    const auto audio = makeReferenceAudio (sr, 140.0, 57, true);                // A2 = 57 -> tonica A
    const auto wav = File::getSpecialLocation (File::tempDirectory).getChildFile ("atdt_ref_test.wav");
    wav.deleteFile();
    {
        WavAudioFormat fmt;
        std::unique_ptr<AudioFormatWriter> w (fmt.createWriterFor (new FileOutputStream (wav), sr, 2, 24, {}, 0));
        if (w != nullptr) w->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
    }
    const auto r = mm::ReferenceAnalyzer::analyzeFile (wav);
    result (r.ok && std::abs (r.bpm - 140.0) <= 1.0, "REF: BPM " + String (r.bpm, 1) + " (gabarito 140), confianca " + String (roundToInt (r.bpmConfidence * 100)) + "%");
    result (r.ok && r.keyRoot == 9 && r.keyMinor, "REF: KEY " + String (names[r.keyRoot % 12]) + (r.keyMinor ? " MINOR" : " MAJOR")
            + " (gabarito A MINOR), confianca " + String (roundToInt (r.keyConfidence * 100)) + "%");
    result (r.ok && r.bands[0] + r.bands[1] > 0.3f && r.energy.size() >= 38, "REF: espectro (sub+bass " + String (roundToInt ((r.bands[0] + r.bands[1]) * 100))
            + "%), linha do tempo com " + String ((int) r.energy.size()) + " pontos");
    result (r.hatDensity > 0.5f && r.kickDensity > 0.2f, "REF: Beat DNA - hats em 1/16 detectados (" + String (roundToInt (r.hatDensity * 100))
            + "%), kick " + String (roundToInt (r.kickDensity * 100)) + "%");
    const auto back = mm::ReferenceResult::fromVar (JSON::parse (JSON::toString (r.toVar())));
    result (back.ok && std::abs (back.bpm - r.bpm) < 0.01 && back.keyRoot == r.keyRoot && back.energy.size() == r.energy.size(),
            "REF: cache JSON salva e restaura a analise");
    // tempo diferente + tom maior
    const auto r2 = mm::ReferenceAnalyzer::analyzeBuffer (makeReferenceAudio (sr, 160.0, 50, false), sr);   // D
    result (r2.ok && std::abs (r2.bpm - 160.0) <= 1.0 && r2.keyRoot == 2, "REF: segundo gabarito BPM " + String (r2.bpm, 1) + " (160), KEY "
            + String (names[r2.keyRoot % 12]) + (r2.keyMinor ? " MINOR" : " MAJOR") + " (tonica D)");
    // erros tratados sem travar
    const auto bad = mm::ReferenceAnalyzer::analyzeFile (File::getSpecialLocation (File::tempDirectory).getChildFile ("nao_existe.wav"));
    AudioBuffer<float> silent (2, (int) (sr * 5)); silent.clear();
    const auto sil = mm::ReferenceAnalyzer::analyzeBuffer (silent, sr);
    result (! bad.ok && bad.error.isNotEmpty() && ! sil.ok && sil.error.isNotEmpty(), "REF: arquivo invalido e audio silencioso dao erro claro (\""
            + bad.error.upToFirstOccurrenceOf (".", false, false) + "\")");
    wav.deleteFile();
    return failures;
}

#include "Scratch/ScratchEngine.h"   // testes do SCRATCH

// parametros do Scratch Machine para testes isolados (valores padrao = os do plugin)
struct ScratchTestParams
{
    std::map<std::string, std::unique_ptr<std::atomic<float>>> v;
    mm::ScratchEngine::Params P;
    std::atomic<float>& operator[] (const std::string& k) { return *v.at (k); }
    ScratchTestParams()
    {
        const std::pair<const char*, float> d[] {
            { "on", 1 }, { "mode", 0 }, { "move", 0 }, { "rate", 0 }, { "depth", .6f }, { "start", .1f }, { "fader", .8f }, { "level", .8f }, { "source", 0 },
            { "engine", 0 }, { "speed", 1 }, { "direction", 0 }, { "curve", .2f }, { "attack", 2 }, { "release", 30 }, { "gate", 0 }, { "resonance", .15f },
            { "tone", .85f }, { "noise", 0 }, { "vinyl", 0 }, { "pitch", 0 }, { "width", .3f }, { "momentum", .3f }, { "accel", 0 }, { "decel", 0 },
            { "randomness", 0 }, { "swing", 0 }, { "repeat", 1 }, { "variation", .5f }, { "hold", 0 }, { "loop", 0 }, { "end", 1 }, { "center", .5f },
            { "fadeIn", 0 }, { "fadeOut", 0 }, { "intensity", .7f }, { "fxFilter", 1 }, { "fxDrive", 0 }, { "fxSat", 0 }, { "fxCrush", 0 },
            { "fxDelay", 0 }, { "fxReverb", 0 }, { "fxWidth", .5f }, { "fxVinyl", 0 }, { "fxMix", 1 }, { "master", 0 } };
        for (auto& [k, x] : d) v[k] = std::make_unique<std::atomic<float>> (x);
        auto& p = P;
        std::atomic<float>** f[] { &p.on, &p.mode, &p.move, &p.rate, &p.depth, &p.start, &p.fader, &p.level, &p.source, &p.engine, &p.speed,
            &p.direction, &p.curve, &p.attack, &p.release, &p.gate, &p.resonance, &p.tone, &p.noise, &p.vinyl, &p.pitch, &p.width, &p.momentum,
            &p.accel, &p.decel, &p.randomness, &p.swing, &p.repeat, &p.variation, &p.hold, &p.loop, &p.end, &p.center, &p.fadeIn, &p.fadeOut,
            &p.intensity, &p.fxFilter, &p.fxDrive, &p.fxSat, &p.fxCrush, &p.fxDelay, &p.fxReverb, &p.fxWidth, &p.fxVinyl, &p.fxMix, &p.master };
        int i = 0; for (auto& [k, x] : d) { juce::ignoreUnused (x); *f[i++] = v[k].get(); }
    }
};

static int scratchTests()
{
    int failures = 0;
    auto result = [&] (bool ok, const String& text) { if (! ok) ++failures; std::printf ("%-4s %s\n", ok ? "OK" : "FAIL", text.toRawUTF8()); };
    ScratchTestParams T;
    const double sr = 48000.0;
    auto render = [&] (int note, int blocks, std::vector<float>& out, MidiBuffer* passed = nullptr, int holdUntilBlock = -1)
    {
        mm::ScratchEngine e (T.P); e.prepare (sr, 512);
        AudioBuffer<float> b (2, 512);
        for (int k = 0; k < blocks; ++k)
        {
            MidiBuffer m;
            if (k == 0 && note >= 0) m.addEvent (MidiMessage::noteOn (1, note, (uint8) 120), 0);
            if (k == holdUntilBlock && note >= 0) m.addEvent (MidiMessage::noteOff (1, note), 0);
            e.handleMidi (m);
            if (passed != nullptr && k == 0) *passed = m;
            b.clear(); e.process (b, 512, 120.0, false, 0.0);
            for (int i = 0; i < 512; ++i) out.push_back (b.getSample (0, i));
        }
    };
    auto silentFraction = [] (const std::vector<float>& v, size_t a, size_t z) { int q = 0; for (size_t i = a; i < z; ++i) q += std::abs (v[i]) < 0.01f; return (double) q / (double) (z - a); };
    auto energy = [] (const std::vector<float>& v, size_t a, size_t z) { double e = 0; for (size_t i = a; i < z; ++i) e += (double) v[i] * v[i]; return e; };

    std::vector<float> baby, trans;
    T["curve"] = 0.0f;                                      // mao senoidal: a virada e suave e o disco para de verdade
    render (60, 50, baby);                                  // C4 = padrao selecionado (BABY)
    T["curve"] = 0.2f;
    float pk = 0; bool fin = true; for (auto x : baby) { pk = jmax (pk, std::abs (x)); fin = fin && std::isfinite (x); }
    result (fin && pk > 0.1f && energy (baby, 11800, 12200) < energy (baby, 3000, 3400) * 0.1,
            "SCRATCH: BABY soa (pico " + String (pk, 2) + ") e silencia na virada do disco (disco parado = sem som)");
    render (64, 50, trans);                                 // D#4 = QUICK TRANSFORMER
    const double sb = silentFraction (baby, 1000, 11000), st = silentFraction (trans, 1000, 11000);
    result (st > 0.3 && sb < 0.2, "SCRATCH: TRANSFORMER picota (" + String (roundToInt (st * 100)) + "% silencio) x BABY continuo (" + String (roundToInt (sb * 100)) + "%)");
    std::vector<float> other; MidiBuffer passed;
    render (59, 20, other, &passed);
    float pkO = 0; for (auto x : other) pkO = jmax (pkO, std::abs (x));
    int passedNotes = 0; for (const auto m : passed) passedNotes += m.getMessage().isNoteOn();
    result (pkO < 1e-6f && passedNotes == 1, "SCRATCH: C4-G#4 disparam golpes; outras notas seguem para o synth");
    {   // os 19 padroes soam, sem NaN
        int ok = 0;
        for (int pat = 0; pat < mm::ScratchPattern::Count; ++pat)
        {
            mm::ScratchEngine e (T.P); e.prepare (sr, 512); e.uiTrigger = pat;
            AudioBuffer<float> b (2, 512); float p2 = 0; bool f2 = true;
            for (int k = 0; k < 50; ++k) { b.clear(); e.process (b, 512, 120.0, false, 0.0); p2 = jmax (p2, b.getMagnitude (0, 512));
                for (int i = 0; i < 512; ++i) f2 = f2 && std::isfinite (b.getSample (0, i)) && std::isfinite (b.getSample (1, i)); }
            ok += (f2 && p2 > 0.05f) ? 1 : 0;
        }
        result (ok == mm::ScratchPattern::Count, "SCRATCH: " + String (ok) + "/" + String ((int) mm::ScratchPattern::Count) + " padroes soam sem NaN");
    }
    {   // SCRATCH PAD: so soa enquanto o disco se move
        mm::ScratchEngine e (T.P); e.prepare (sr, 512);
        AudioBuffer<float> b (2, 512); e.padActive = true;
        float moving = 0, still = 0;
        for (int k = 0; k < 40; ++k) { e.padTarget = 0.3f + 0.3f * (float) std::sin (k * 0.4); b.clear(); e.process (b, 512, 120.0, false, 0.0); moving = jmax (moving, b.getMagnitude (0, 512)); }
        e.padTarget = 0.5f;
        for (int k = 0; k < 20; ++k) { b.clear(); e.process (b, 512, 120.0, false, 0.0); }
        for (int k = 0; k < 10; ++k) { b.clear(); e.process (b, 512, 120.0, false, 0.0); still = jmax (still, b.getMagnitude (0, 512)); }
        result (moving > 0.05f && still < moving * 0.05f, "SCRATCH PAD: mouse movendo = som (" + String (moving, 2) + "), mouse parado = silencio (" + String (still, 3) + ")");
    }
    {   // SYNC: um golpe por tempo
        T["mode"] = 1.0f;
        mm::ScratchEngine e (T.P); e.prepare (sr, 512);
        AudioBuffer<float> b (2, 512); int beats = 0; double ppq = 0; float bp = 0; int idx = 0;
        for (int k = 0; k < 188; ++k)
        {
            b.clear(); e.process (b, 512, 120.0, true, ppq); bp = jmax (bp, b.getMagnitude (0, 512));
            ppq += 512.0 * 2.0 / sr;
            if ((int) std::floor (ppq) != idx) { beats += bp > 0.05f; bp = 0; idx = (int) std::floor (ppq); }
        }
        T["mode"] = 0.0f;
        result (beats >= 3, "SCRATCH: SYNC dispara um golpe por tempo (" + String (beats) + " de 4)");
    }
    {   // RANDOMNESS: golpes iguais ficam diferentes
        T["randomness"] = 1.0f;
        std::vector<float> a1, a2; render (60, 30, a1); render (60, 30, a2);
        double d = 0; for (size_t i = 0; i < a1.size(); ++i) d += std::abs (a1[i] - a2[i]);
        T["randomness"] = 0.0f;
        std::vector<float> b1, b2; render (60, 30, b1); render (60, 30, b2);
        double d0 = 0; for (size_t i = 0; i < b1.size(); ++i) d0 += std::abs (b1[i] - b2[i]);
        result (d > 1.0 && d0 < d * 0.2, "SCRATCH: RANDOMNESS torna golpes repetidos diferentes (dif " + String (d, 1) + " x " + String (d0, 1) + " sem)");
    }
    {   // HOLD: repete enquanto a tecla esta pressionada
        T["hold"] = 1.0f;
        std::vector<float> h; render (60, 220, h, nullptr, 100);   // solta no bloco 100 (~1,07 s): o golpe atual termina o movimento
        T["hold"] = 0.0f;
        result (energy (h, 24000 * 2 + 2000, 24000 * 2 + 10000) > 0.5 && energy (h, 220 * 512 - 8000, 220 * 512) < 1e-4,
                "SCRATCH: HOLD repete enquanto segura a tecla e para ao soltar");
    }
    {   // FX completo ligado: estavel
        for (auto k : { "fxDrive", "fxSat", "fxCrush", "fxDelay", "fxReverb", "fxVinyl", "noise", "vinyl", "gate" }) T[k] = 0.8f;
        std::vector<float> fx; render (60, 60, fx);
        bool ok = true; float pk2 = 0; for (auto x : fx) { ok = ok && std::isfinite (x); pk2 = jmax (pk2, std::abs (x)); }
        for (auto k : { "fxDrive", "fxSat", "fxCrush", "fxDelay", "fxReverb", "fxVinyl", "noise", "vinyl", "gate" }) T[k] = 0.0f;
        result (ok && pk2 > 0.05f && pk2 < 1.5f, "SCRATCH FX: drive+sat+crush+delay+reverb+vinil ligados, estavel (pico " + String (pk2, 2) + ")");
    }
    {   // SAMPLE SCRATCH
        const auto wav = File::getSpecialLocation (File::tempDirectory).getChildFile ("atdt_scratch_user.wav");
        wav.deleteFile();
        AudioBuffer<float> a (1, 44100); for (int i = 0; i < 44100; ++i) a.setSample (0, i, 0.5f * (float) std::sin (i * 0.05));
        { WavAudioFormat f; std::unique_ptr<AudioFormatWriter> w (f.createWriterFor (new FileOutputStream (wav), 44100.0, 1, 16, {}, 0)); if (w) w->writeFromAudioSampleBuffer (a, 0, 44100); }
        mm::ScratchEngine e (T.P); e.prepare (sr, 512); String err;
        const bool ok = e.loadUserSample (wav, err);
        T["engine"] = 1.0f;
        AudioBuffer<float> b (2, 512); MidiBuffer m; m.addEvent (MidiMessage::noteOn (1, 60, (uint8) 120), 0); e.handleMidi (m);
        float upk = 0; for (int k = 0; k < 30; ++k) { b.clear(); e.process (b, 512, 120.0, false, 0.0); upk = jmax (upk, b.getMagnitude (0, 512)); }
        T["engine"] = 0.0f;
        result (ok && e.hasUserSample() && upk > 0.1f, "SAMPLE SCRATCH: WAV 44.1 kHz -> 48 kHz carregado e arranhado (pico " + String (upk, 2) + ")");
        wav.deleteFile();
    }
    return failures;
}

#include "Effects/FxEngine.h"   // teste unitario dos efeitos

static double centroidOf (const std::vector<float>& x, double sr)
{
    const int n = 4096; const size_t st = x.size() - (size_t) n; double num = 0, den = 0;
    for (int k = 1; k < n / 2; ++k)
    {
        double re = 0, im = 0;
        for (int i = 0; i < n; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (MathConstants<double>::twoPi * i / (n - 1));
            const double a = MathConstants<double>::twoPi * k * i / n;
            re += x[st + (size_t) i] * w * std::cos (a); im -= x[st + (size_t) i] * w * std::sin (a);
        }
        const double m = std::sqrt (re * re + im * im); num += m * k * sr / n; den += m;
    }
    return num / jmax (1e-12, den);
}

static int fxIsolation()   // FxEngine sozinho: saturacao em cada tipo com uma senoide pura
{
    std::array<std::atomic<float>, 64> st; for (auto& a : st) a = 0.0f;
    mm::FxEngine::Params P; int k = 0; auto nx = [&] { return &st[(size_t) k++]; };
    for (auto* f : { &P.distOn, &P.distType, &P.distDrive, &P.distTone, &P.distMix, &P.eqOn, &P.eqLow, &P.eqMid, &P.eqHigh,
                     &P.chOn, &P.chRate, &P.chDepth, &P.chMix, &P.dlOn, &P.dlDiv, &P.dlFb, &P.dlMix, &P.dlPing,
                     &P.rvOn, &P.rvSize, &P.rvDamp, &P.rvWidth, &P.rvMix, &P.cpOn, &P.cpThresh, &P.cpRatio, &P.cpAttack,
                     &P.cpRelease, &P.cpMakeup, &P.limOn, &P.satOn, &P.satType, &P.satDrive, &P.satMix, &P.crOn, &P.crBits,
                     &P.crDown, &P.crMix, &P.phOn, &P.phRate, &P.phDepth, &P.phFb, &P.phMix, &P.flOn, &P.flRate, &P.flDepth,
                     &P.flFb, &P.flMix, &P.glOn, &P.glMode, &P.glDiv, &P.glMix })
        *f = nx();
    int failures = 0;
    const double sr = 48000.0;
    auto run = [&] (int type) -> std::pair<double, double>
    {
        mm::FxEngine fx (P); fx.prepare (sr, 512);
        P.satOn->store (type < 0 ? 0.0f : 1.0f); P.satType->store ((float) jmax (0, type)); P.satDrive->store (1.0f); P.satMix->store (1.0f);
        AudioBuffer<float> b (2, 512); std::vector<float> out; double ph = 0; double dc = 0;
        for (int blk = 0; blk < 60; ++blk)
        {
            for (int i = 0; i < 512; ++i) { const float v = 0.3f * (float) std::sin (ph); ph += MathConstants<double>::twoPi * 110.0 / sr; b.setSample (0, i, v); b.setSample (1, i, v); }
            fx.processMain (b, 512, 120.0);
            for (int i = 0; i < 512; ++i) { out.push_back (b.getSample (0, i)); dc += b.getSample (0, i); }
        }
        return { centroidOf (out, sr), dc / (double) out.size() };
    };
    const auto dry = run (-1);
    const char* names[] { "Warm", "Tape", "Tube", "Digital" };
    for (int t = 0; t < 4; ++t)
    {
        const auto r = run (t);
        const bool ok = r.first > dry.first * 1.5 && std::abs (r.second) < 0.01;
        if (! ok) ++failures;
        std::printf ("%-4s Saturation %-7s isolada: centroide %.0f -> %.0f Hz, DC %.4f\n", ok ? "OK" : "FAIL", names[t], dry.first, r.first, r.second);
    }
    return failures;
}

// ---- v0.9: saturacao, bitcrush, phaser, flanger, glitch ----
static std::vector<float> renderSignal (AudioPluginInstance& plugin, int note)
{
    const int block = 512;
    AudioBuffer<float> buf (2, block); MidiBuffer none; std::vector<float> out;
    for (int i = 0; i < 30; ++i) { buf.clear(); plugin.processBlock (buf, none); }
    for (int b = 0; b < 94; ++b)
    {
        MidiBuffer m; if (b == 0) m.addEvent (MidiMessage::noteOn (1, note, (uint8) 100), 0);
        buf.clear(); plugin.processBlock (buf, m);
        for (int i = 0; i < block; ++i) out.push_back (buf.getSample (0, i));
    }
    MidiBuffer off; off.addEvent (MidiMessage::noteOff (1, note), 0);
    buf.clear(); plugin.processBlock (buf, off);
    for (int i = 0; i < 300; ++i) { buf.clear(); plugin.processBlock (buf, none); }
    return out;
}

static int testFx2 (AudioPluginInstance& plugin)
{
    int failures = 0;
    auto set = [&] (const char* nm, float v) { if (auto* p = findParam (plugin, nm)) p->setValue (v); else { std::printf ("  parametro nao encontrado: %s\n", nm); ++failures; } };
    auto result = [&] (bool ok, const String& text) { if (! ok) ++failures; std::printf ("%-4s %s\n", ok ? "OK" : "FAIL", text.toRawUTF8()); };
    auto energy = [] (const std::vector<float>& v) { double e = 0; for (auto x : v) e += (double) x * x; return e; };
    auto finite = [] (const std::vector<float>& v) { for (auto x : v) if (! std::isfinite (x)) return false; return true; };
    auto diffRatio = [&] (const std::vector<float>& a, const std::vector<float>& b)
    {
        double d = 0; for (size_t i = 0; i < jmin (a.size(), b.size()); ++i) d += (double) (a[i] - b[i]) * (a[i] - b[i]);
        return std::sqrt (d / jmax (1.0e-12, energy (a)));
    };
    auto base = [&]
    {
        plugin.setCurrentProgram (0);
        {   // a troca de programa e aplicada no proximo bloco: processa antes de mexer nos parametros
            AudioBuffer<float> b (2, 512); MidiBuffer none;
            for (int i = 0; i < 4; ++i) { b.clear(); plugin.processBlock (b, none); }
        }
        set ("OSC1 Wave", 0.0f); set ("OSC2 Level", 0.0f); set ("OSC3 Level", 0.0f); set ("Sub Level", 0.0f);
        set ("Filter Cutoff", 1.0f); set ("Filter Env Amount", 0.5f); set ("Filter Drive", 0.0f); set ("Mod 1 Source", 0.0f);
        set ("Amp Sustain", 0.3f); set ("Amp Decay", 0.35f);   // som que decai: glitch fica mensuravel
    };
    bool f1, f2;

    base(); const double c0 = renderCentroid (plugin, 45, f1);
    {   // integracao da saturacao no plugin (a qualidade do DSP e provada no teste isolado)
        base(); const auto d0 = renderSignal (plugin, 57);
        set ("FX Saturation On", 1.0f); set ("FX Saturation Type", 2.0f / 3.0f); set ("FX Saturation Drive", 1.0f);
        const auto d1 = renderSignal (plugin, 57);
        const double d = diffRatio (d0, d1);
        result (finite (d1) && d > 0.1, "Saturation (Tube) no plugin: o sinal muda " + String (d * 100.0, 0) + "% (sem NaN)");
    }

    base(); set ("FX Bitcrush On", 1.0f); set ("FX Bitcrush Bits", 1.0f / 14.0f); set ("FX Bitcrush Downsample", 7.0f / 63.0f);
    const double cCr = renderCentroid (plugin, 45, f2);
    result (f2 && cCr > c0 * 1.5, "Bitcrush (3 bits, /8): centroide " + String (c0, 0) + " -> " + String (cCr, 0) + " Hz");

    base(); const auto dry = renderSignal (plugin, 57);
    struct Case { const char* label; std::vector<std::pair<const char*, float>> ps; double minDiff; };
    const Case cases[] {
        { "Phaser",            { { "FX Phaser On", 1 }, { "FX Phaser Mix", 1.0f } }, 0.1 },
        { "Flanger",           { { "FX Flanger On", 1 } }, 0.1 },
        { "Glitch Stutter 1/16", { { "FX Glitch On", 1 }, { "FX Glitch Mode", 0.0f }, { "FX Glitch Division", 0.6f } }, 0.1 },
        { "Glitch Reverse 1/8",  { { "FX Glitch On", 1 }, { "FX Glitch Mode", 2.0f / 3.0f }, { "FX Glitch Division", 0.4f } }, 0.1 },
        { "Glitch Tape Stop 1/4",{ { "FX Glitch On", 1 }, { "FX Glitch Mode", 1.0f }, { "FX Glitch Division", 0.2f } }, 0.1 },
    };
    for (auto& c : cases)
    {
        base(); for (auto& [n, v] : c.ps) set (n, v);
        const auto wet = renderSignal (plugin, 57);
        const double d = diffRatio (dry, wet);
        result (finite (wet) && d > c.minDiff, String (c.label) + ": o sinal muda " + String (d * 100.0, 0) + "% (sem NaN)");
    }
    // gate precisa de som SUSTENTADO (com som que decai, a energia retida depende da posicao das fatias)
    base(); set ("Amp Sustain", 1.0f);
    const auto sustained = renderSignal (plugin, 57);
    set ("FX Glitch On", 1.0f); set ("FX Glitch Mode", 1.0f / 3.0f); set ("FX Glitch Division", 0.6f);
    const auto gated = renderSignal (plugin, 57);
    const double ratio = energy (gated) / jmax (1.0e-12, energy (sustained));
    result (finite (gated) && ratio > 0.3 && ratio < 0.7, "Glitch Gate 1/16: energia " + String (ratio * 100.0, 0) + "% do sinal seco (esperado ~50%)");
    plugin.setCurrentProgram (0);
    for (auto* nm : { "FX Saturation On", "FX Bitcrush On", "FX Phaser On", "FX Flanger On", "FX Glitch On" }) set (nm, 0.0f);
    return failures;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI juce;
    if (SystemStats::getEnvironmentVariable ("MM_UNIT", {}).isNotEmpty())   // so os testes unitarios (rapido)
        return drumIsolation() + fxIsolation() + beatTests() + songTests() + referenceTests() + scratchTests();
    if (argc < 2) { std::printf ("uso: PluginTest <plugin.vst3>\n"); return 2; }

    AudioPluginFormatManager fm;
    fm.addFormat (new VST3PluginFormat());

    OwnedArray<PluginDescription> types;
    VST3PluginFormat vst3;
    vst3.findAllTypesForFile (types, String (argv[1]));
    if (types.isEmpty()) { std::printf ("FALHA: plugin nao encontrado\n"); return 1; }

    String err;
    auto plugin = fm.createPluginInstance (*types[0], 48000.0, 512, err);
    if (plugin == nullptr) { std::printf ("FALHA ao carregar: %s\n", err.toRawUTF8()); return 1; }

    std::printf ("Plugin: %s | instrumento=%d | parametros=%d | programas=%d\n",
                 plugin->getName().toRawUTF8(), (int) types[0]->isInstrument,
                 plugin->getParameters().size(), plugin->getNumPrograms());

    plugin->setPlayConfigDetails (0, 2, 48000.0, 512);

    // MM_DUMP_IDS=arquivo : grava a lista oficial de IDs de parametro (usada pelo gerador da biblioteca)
    if (auto dumpPath = SystemStats::getEnvironmentVariable ("MM_DUMP_IDS", {}); dumpPath.isNotEmpty())
    {
        MemoryBlock st; plugin->getStateInformation (st);
        StringArray ids;
        if (auto head = AudioProcessor::getXmlFromBinary (st.getData(), (int) st.getSize()))
            if (auto* comp = head->getChildByName ("IComponent"))
            {
                MemoryBlock mb; mb.fromBase64Encoding (comp->getAllSubText());
                if (auto inner = AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize()))
                    for (auto* e : inner->getChildWithTagNameIterator ("PARAM"))
                        ids.add (e->getStringAttribute ("id") + "=" + e->getStringAttribute ("value"));
            }
        File (dumpPath).replaceWithText (ids.joinIntoString ("\n"));
        std::printf ("IDs gravados: %d\n", ids.size());
        return 0;
    }
    plugin->prepareToPlay (48000.0, 512);

    int failures = 0;
    int bassNote[] { 40 };          // E2
    int chord[] { 52, 55, 59 };      // Em

    for (int prog = 0; prog < plugin->getNumPrograms(); ++prog)
    {
        plugin->setCurrentProgram (prog);
        {   // espera ~100 ms: o master gain e suavizado (20 ms) para nao gerar clicks
            AudioBuffer<float> settle (2, 512);
            MidiBuffer none;
            for (int i = 0; i < 10; ++i) { settle.clear(); plugin->processBlock (settle, none); }
        }
        const auto name = plugin->getProgramName (prog);
        const bool isBass = name.startsWith ("BASS") || name.startsWith ("DUBSTEP") || name.startsWith ("TRAP")
                            || name.startsWith ("BREAKDOWN");
        const bool isScratch = name.startsWith ("SCRATCH");   // golpes em C4 (modulo) / nota unica (synth scratch)
        float peak, rms; bool finite;
        int c4[] { 60 };
        TestPlayHead playing; playing.bpm = 120.0; playing.playing = true;
        const bool needsPlay = name.contains ("(Play)");                       // preset que toca com o play do host
        if (needsPlay) plugin->setPlayHead (&playing);
        if (isScratch)   render (*plugin, c4, 1, 2.0, peak, rms, finite);
        else if (isBass) render (*plugin, bassNote, 1, 2.0, peak, rms, finite);
        else             render (*plugin, chord, 3, 2.0, peak, rms, finite);
        if (needsPlay) plugin->setPlayHead (nullptr);

        const bool ok = finite && rms > 0.001f && peak < 4.0f;
        if (! ok) ++failures;
        std::printf ("%-4s %-32s peak %6.3f (%+5.1f dBFS)  rms %6.4f %s\n", ok ? "OK" : "FAIL", name.toRawUTF8(),
                     peak, Decibels::gainToDecibels (peak, -100.0f), rms, finite ? "" : "NaN/Inf!");
        if (auto qcPath = SystemStats::getEnvironmentVariable ("MM_QC", {}); qcPath.isNotEmpty())   // relatorio da biblioteca
            File (qcPath).appendText (name + "|" + String (peak, 5) + "|" + String (rms, 6) + "|"
                                      + String (lastQc.sideLowRatio, 6) + "|" + String (lastQc.zcrHz, 1) + "\n");

        // silencio apos o release (voz nao fica presa)
        AudioBuffer<float> tail (2, 512);
        MidiBuffer none;
        float tailPeak = 0.0f;
        for (int i = 0; i < 940; ++i) { tail.clear(); plugin->processBlock (tail, none); }   // 10 s: separa nota presa de cauda longa
        for (int c = 0; c < 2; ++c) tailPeak = jmax (tailPeak, tail.getMagnitude (c, 0, 512));
        // LOOP / FREE tocam continuamente por definicao (como o ALWAYS da bateria): nao sao notas presas
        const bool continuous = name.contains ("SCRATCH - Loop ") || name.contains ("SCRATCH - Free ");
        if (continuous) std::printf ("     (continuo por definicao: %s)\n", name.fromLastOccurrenceOf ("- ", false, false).toRawUTF8());
        if (tailPeak > 0.001f && ! continuous) { ++failures; std::printf ("     FAIL: nota presa (tail %.4f)\n", tailPeak); }
    }

    // sincronia com o host (REAPER simulado)
    if (! testSync (*plugin, 120.0, 0.5f, 1.0, "120 BPM 1/16       ")) ++failures;
    if (! testSync (*plugin, 180.0, 0.5f, 1.0, "180 BPM 1/16       ")) ++failures;
    if (! testSync (*plugin, 150.0, 0.0f, 0.5, "150 BPM 1/16 HALF  ")) ++failures;
    if (! testSync (*plugin, 150.0, 1.0f, 2.0, "150 BPM 1/16 DOUBLE")) ++failures;


    // estado: preset com sequencia -> salvar -> trocar -> restaurar -> deve ficar identico
    int seqProg = 0;
    for (int i = 0; i < plugin->getNumPrograms(); ++i)
        if (plugin->getProgramName (i).contains ("Drop Slide")) seqProg = i;
    plugin->setCurrentProgram (seqProg);
    MemoryBlock state, state2;
    plugin->getStateInformation (state);
    plugin->setCurrentProgram (0);
    plugin->setStateInformation (state.getData(), (int) state.getSize());
    plugin->getStateInformation (state2);
    const bool stateOk = state.getSize() > 100 && state == state2;
    if (! stateOk) ++failures;
    std::printf ("%-4s Estado salvo/restaurado (%d bytes)\n", stateOk ? "OK" : "FAIL", (int) state.getSize());

    failures += testTimbre (*plugin);   // antes do releaseResources: o plugin precisa estar ativo
    failures += testSwingTranspose (*plugin);
    failures += testFx (*plugin);
    failures += testDrums (*plugin);
    failures += drumIsolation();
    failures += testFx2 (*plugin);
    failures += fxIsolation();
    failures += beatTests();
    failures += songTests();
    failures += referenceTests();
    failures += scratchTests();
    plugin->releaseResources();
    std::printf ("\nRESULTADO: %s (%d falha(s))\n", failures == 0 ? "TUDO OK" : "FALHOU", failures);
    return failures == 0 ? 0 : 1;
}
