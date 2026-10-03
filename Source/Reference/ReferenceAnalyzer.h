#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

namespace mm
{
// =============================================================================
//  REFERENCE ANALYZER - FULL MIX ANALYSIS (local, offline, fora do audio thread)
//  Mede caracteristicas da faixa; NUNCA copia notas/ritmos: o resultado alimenta
//  geradores que criam material ORIGINAL. Cada estimativa vem com CONFIANCA.
//  Metodos:
//   BPM ......... autocorrelacao do envelope de ataques (spectral flux), 70-200 BPM
//   KEY ......... cromagrama x perfis de Krumhansl (24 tonalidades)
//   ESPECTRO .... energia por faixa (sub/bass/low-mid/mid/high-mid/high), centroide, rolloff
//   DINAMICA .... RMS, pico, crest factor
//   ESTEREO ..... correlacao L/R e largura (side/mid)
//   EVENTOS ..... impactos (saltos de energia) e secoes altas/baixas
//   BEAT DNA .... densidade de kick/hat, half-time, sincope, trap
//   SOUND DNA ... sub, brilho, distorcao, estereo, densidade, escuridao, agressividade
// =============================================================================
struct ReferenceResult
{
    bool ok = false;
    juce::String error, fileName;
    double durationSec = 0.0, sampleRate = 0.0;
    double bpm = 0.0;  float bpmConfidence = 0.0f;
    int keyRoot = 0;   bool keyMinor = true; float keyConfidence = 0.0f;
    int altRoot = 0;   bool altMinor = false;   // segunda hipotese (ex.: tom relativo)
    std::array<float, 12> chroma {}, bassChroma {};   // perfil de notas (harmonico) normalizado - exibido no Reference Lab
    std::array<float, 6> bands {};            // fracao de energia: sub, bass, low-mid, mid, high-mid, high
    float centroidHz = 0, rolloffHz = 0, rmsDb = -100, peakDb = -100, crestDb = 0;
    float stereoCorrelation = 1, stereoWidth = 0;
    std::vector<float> energy;                // RMS por 0,5 s (0..1), para a linha do tempo
    std::vector<std::pair<double, juce::String>> events;   // (segundos, "IMPACT"/"HIGH"/"LOW")
    // BEAT DNA (0..1)
    float kickDensity = 0, hatDensity = 0, halfTime = 0, syncopation = 0, trap = 0, electronic = 0;
    // SOUND DNA (0..1)
    float sub = 0, bright = 0, distortion = 0, stereo = 0, density = 0, darkness = 0, aggression = 0, impact = 0;

    juce::var toVar() const;
    static ReferenceResult fromVar (const juce::var&);
};

class ReferenceAnalyzer
{
public:
    // aumente quando o algoritmo mudar: invalida o cache de analises antigas
    static constexpr int algorithmVersion = 2;

    // Pode demorar alguns segundos: chame de uma thread de fundo. 'progress' 0..1 opcional.
    static ReferenceResult analyzeFile (const juce::File& file, std::function<void (float)> progress = {},
                                        std::function<bool()> shouldExit = {})
    {
        ReferenceResult res;
        res.fileName = file.getFileName();
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
        if (reader == nullptr)
        {
            res.error = "Formato nao suportado ou arquivo corrompido. Use WAV, AIFF, FLAC ou MP3.";
            return res;
        }
        const double sr = reader->sampleRate;
        const auto maxSamples = (juce::int64) (sr * 360.0);    // ate 6 minutos
        const int n = (int) juce::jmin (reader->lengthInSamples, maxSamples);
        if (n < (int) (sr * 3.0))
        {
            res.error = "Audio curto demais (minimo 3 segundos).";
            return res;
        }
        juce::AudioBuffer<float> buf ((int) juce::jmin (2u, reader->numChannels), n);
        reader->read (&buf, 0, n, 0, true, buf.getNumChannels() > 1);
        return analyzeBuffer (buf, sr, res, progress, shouldExit);
    }

    static ReferenceResult analyzeBuffer (const juce::AudioBuffer<float>& buf, double sr, ReferenceResult res = {},
                                          std::function<void (float)> progress = {}, std::function<bool()> shouldExit = {})
    {
        const int n = buf.getNumSamples();
        const float* L = buf.getReadPointer (0);
        const float* R = buf.getReadPointer (buf.getNumChannels() > 1 ? 1 : 0);
        res.sampleRate = sr;
        res.durationSec = n / sr;

        // ---------------- dinamica e estereo
        double sumSq = 0, peak = 0, sLL = 0, sRR = 0, sLR = 0, mid = 0, side = 0;
        for (int i = 0; i < n; ++i)
        {
            const double m = 0.5 * (L[i] + R[i]), sd = 0.5 * (L[i] - R[i]);
            sumSq += m * m; peak = juce::jmax (peak, (double) std::abs (L[i]), (double) std::abs (R[i]));
            sLL += (double) L[i] * L[i]; sRR += (double) R[i] * R[i]; sLR += (double) L[i] * R[i];
            mid += m * m; side += sd * sd;
        }
        const double rms = std::sqrt (sumSq / n);
        res.rmsDb = (float) juce::Decibels::gainToDecibels (rms, -100.0);
        res.peakDb = (float) juce::Decibels::gainToDecibels (peak, -100.0);
        res.crestDb = res.peakDb - res.rmsDb;
        res.stereoCorrelation = (float) (sLR / std::sqrt (juce::jmax (1e-12, sLL * sRR)));
        res.stereoWidth = (float) juce::jlimit (0.0, 1.0, std::sqrt (side / juce::jmax (1e-12, mid)));
        if (rms < 1e-5) { res.error = "Audio silencioso."; return res; }

        // ---------------- analise espectral por quadros (FFT 2048, hop 512)
        constexpr int order = 11, fftSize = 1 << order, hop = 512;
        juce::dsp::FFT fft (order);
        std::vector<float> window (fftSize), frame (fftSize * 2), prevMag (fftSize / 2 + 1, 0.0f);
        for (int i = 0; i < fftSize; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / (fftSize - 1));
        const int frames = juce::jmax (1, (n - fftSize) / hop);
        std::vector<float> flux ((size_t) frames), lowFlux ((size_t) frames), highFlux ((size_t) frames), midFlux ((size_t) frames), frameRms ((size_t) frames);
        std::array<double, 12> chroma {}, bassChroma {};   // bassChroma (< 250 Hz) decide entre tons relativos
        std::array<double, 6> bandE {};
        double centroidSum = 0, rolloffSum = 0, weightSum = 0;
        const double binHz = sr / fftSize;
        auto band = [&] (double hz) { return hz < 60 ? 0 : hz < 250 ? 1 : hz < 500 ? 2 : hz < 2000 ? 3 : hz < 6000 ? 4 : 5; };
        // HPSS (separacao harmonico/percussiva): mediana no TEMPO de cada bin (9 quadros ~ 0,1 s).
        // Ataques curtos (kick, caixa, hats) somem; acordes e baixo sustentados ficam -> tonalidade sem contaminacao da bateria.
        constexpr int medLen = 9, bins = fftSize / 2 + 1;
        std::vector<float> history ((size_t) (medLen * bins), 0.0f);
        std::array<float, medLen> medBuf {};
        std::vector<float> harm ((size_t) bins, 0.0f);
        // ataques de kick: o grave logo apos o golpe (cauda tonal do kick) nao entra no cromagrama do baixo
        int lastKickFrame = -1000;
        const int kickHold = (int) (0.14 * sr / hop);
        double lowFluxAvg = 0.0;
        const int kLo = (int) (50.0 / binHz), kHi = (int) (5000.0 / binHz);

        for (int f = 0; f < frames; ++f)
        {
            if (shouldExit && shouldExit()) { res.error = "Cancelado."; return res; }
            if (progress && f % 256 == 0) progress (0.85f * (float) f / (float) frames);
            const int start = f * hop;
            double fr = 0;
            for (int i = 0; i < fftSize; ++i)
            {
                const float x = 0.5f * (L[start + i] + R[start + i]);
                frame[(size_t) i] = x * window[(size_t) i];
                fr += (double) x * x;
            }
            std::fill (frame.begin() + fftSize, frame.end(), 0.0f);
            frameRms[(size_t) f] = (float) std::sqrt (fr / fftSize);
            fft.performFrequencyOnlyForwardTransform (frame.data());
            double fl = 0, lo = 0, hi = 0, md = 0, tot = 0, cen = 0;
            for (int k = 1; k <= fftSize / 2; ++k)
            {
                const float mag = frame[(size_t) k];
                const float d = mag - prevMag[(size_t) k];
                const double hz = k * binHz;
                if (d > 0) { fl += d; if (hz < 150) lo += d; else if (hz > 6000) hi += d; else if (hz > 150 && hz < 2000) md += d; }
                prevMag[(size_t) k] = mag;
                const double e = (double) mag * mag;
                bandE[(size_t) band (hz)] += e;
                tot += e; cen += e * hz;
                history[(size_t) ((f % medLen) * bins + k)] = mag;
            }
            // cromagrama so da parte HARMONICA (mediana no tempo), por PICOS com interpolacao parabolica:
            // abaixo de ~200 Hz um semitom e menor que a resolucao da FFT; atribuir bins vizinhos a notas
            // vaza energia para notas erradas. O pico interpolado da a frequencia exata.
            if (f >= medLen)
            {
                for (int k = juce::jmax (1, kLo - 1); k <= juce::jmin (bins - 1, kHi + 1); ++k)
                {
                    for (int j = 0; j < medLen; ++j) medBuf[(size_t) j] = history[(size_t) (j * bins + k)];
                    std::nth_element (medBuf.begin(), medBuf.begin() + medLen / 2, medBuf.end());
                    harm[(size_t) k] = medBuf[(size_t) (medLen / 2)];
                }
                for (int k = juce::jmax (2, kLo); k <= juce::jmin (bins - 2, kHi); ++k)
                {
                    const float a0 = harm[(size_t) k - 1], a1 = harm[(size_t) k], a2 = harm[(size_t) k + 1];
                    if (a1 <= 1e-6f || a1 <= a0 || a1 < a2) continue;          // so maximos locais
                    const double la = std::log (a0 + 1e-9), lb = std::log (a1 + 1e-9), lc = std::log (a2 + 1e-9);
                    const double den = la - 2 * lb + lc;
                    const double delta = std::abs (den) > 1e-12 ? juce::jlimit (-0.5, 0.5, 0.5 * (la - lc) / den) : 0.0;
                    const double hz = (k + delta) * binHz;
                    const double e = (double) a1 * a1;
                    const double midi = 69.0 + 12.0 * std::log2 (hz / 440.0);
                    const auto pc = (size_t) (((int) std::lround (midi) % 12 + 12) % 12);
                    if (hz >= 100) chroma[pc] += e;                         // acima da fundamental do kick
                    if (hz < 250 && (f - medLen / 2) - lastKickFrame > kickHold) bassChroma[pc] += e;   // so baixo SUSTENTADO (quadro central da mediana)
                }
            }
            lowFluxAvg = f == 0 ? lo : 0.98 * lowFluxAvg + 0.02 * lo;
            if (lo > 2.5 * lowFluxAvg && lo > 1e-6) lastKickFrame = f;
            flux[(size_t) f] = (float) fl; lowFlux[(size_t) f] = (float) lo; highFlux[(size_t) f] = (float) hi; midFlux[(size_t) f] = (float) md;
            if (tot > 1e-9)
            {
                centroidSum += cen; weightSum += tot;
                double acc = 0; int k85 = 1;
                for (int k = 1; k <= fftSize / 2; ++k) { acc += (double) frame[(size_t) k] * frame[(size_t) k]; if (acc >= 0.85 * tot) { k85 = k; break; } }
                rolloffSum += k85 * binHz * tot;
            }
        }
        res.centroidHz = (float) (centroidSum / juce::jmax (1e-9, weightSum));
        res.rolloffHz = (float) (rolloffSum / juce::jmax (1e-9, weightSum));
        double bt = 0; for (auto e : bandE) bt += e;
        for (size_t i = 0; i < 6; ++i) res.bands[i] = (float) (bandE[i] / juce::jmax (1e-12, bt));

        // ---------------- BPM: autocorrelacao do envelope de ataques
        const double frameRate = sr / hop;
        auto normalize = [] (std::vector<float>& v)
        {
            double mean = 0; for (auto x : v) mean += x; mean /= juce::jmax ((size_t) 1, v.size());
            for (auto& x : v) x = juce::jmax (0.0f, x - (float) mean);
        };
        std::vector<float> onset = flux; normalize (onset);
        const int minLag = (int) (frameRate * 60.0 / 200.0), maxLag = (int) (frameRate * 60.0 / 70.0);
        std::vector<double> ac ((size_t) maxLag + 2, 0.0);
        for (int lag = minLag; lag <= maxLag + 1; ++lag)
        {
            double s = 0;
            for (int i = 0; i + lag < frames; ++i) s += (double) onset[(size_t) i] * onset[(size_t) (i + lag)];
            ac[(size_t) lag] = s;
        }
        // prefere a faixa 90-180 BPM (metal/trap moderno) com peso suave
        int best = minLag; double bestScore = -1, sumAc = 0; int cnt = 0;
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            const double bpm = 60.0 * frameRate / lag;
            const double w = (bpm >= 90 && bpm <= 180) ? 1.0 : 0.85;
            const double sc = ac[(size_t) lag] * w;
            sumAc += ac[(size_t) lag]; ++cnt;
            if (sc > bestScore) { bestScore = sc; best = lag; }
        }
        double lagF = best;
        if (best > minLag && best < maxLag)   // interpolacao parabolica
        {
            const double a = ac[(size_t) best - 1], b = ac[(size_t) best], c = ac[(size_t) best + 1];
            const double den = a - 2 * b + c;
            if (std::abs (den) > 1e-12) lagF = best + 0.5 * (a - c) / den;
        }
        res.bpm = std::round (600.0 * frameRate / lagF) / 10.0;
        const double meanAc = sumAc / juce::jmax (1, cnt);
        res.bpmConfidence = (float) juce::jlimit (0.0, 1.0, (ac[(size_t) best] / juce::jmax (1e-12, meanAc) - 1.0) / 3.0);

        // ---------------- KEY: Krumhansl-Kessler
        static const double maj[12] { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
        static const double mnr[12] { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
        auto corr = [&] (const double* prof, int root)
        {
            double mx = 0, my = 0;
            for (int i = 0; i < 12; ++i) { mx += chroma[(size_t) ((i + root) % 12)]; my += prof[i]; }
            mx /= 12; my /= 12;
            double sxy = 0, sxx = 0, syy = 0;
            for (int i = 0; i < 12; ++i)
            {
                const double x = chroma[(size_t) ((i + root) % 12)] - mx, y = prof[i] - my;
                sxy += x * y; sxx += x * x; syy += y * y;
            }
            return sxy / std::sqrt (juce::jmax (1e-12, sxx * syy));
        };
        {
            double cm = 1e-12, bm = 1e-12;
            for (int i = 0; i < 12; ++i) { cm = juce::jmax (cm, chroma[(size_t) i]); bm = juce::jmax (bm, bassChroma[(size_t) i]); }
            for (int i = 0; i < 12; ++i) { res.chroma[(size_t) i] = (float) (chroma[(size_t) i] / cm); res.bassChroma[(size_t) i] = (float) (bassChroma[(size_t) i] / bm); }
        }
        // pontuacao = correlacao do perfil + peso da tonica no GRAVE (baixo/guitarra pedalam na tonica)
        double bassTot = 0; for (auto x : bassChroma) bassTot += x;
        double k1 = -9, k2 = -9;
        for (int r = 0; r < 12; ++r)
            for (int m = 0; m < 2; ++m)
            {
                const double c = corr (m ? mnr : maj, r) + 0.6 * bassChroma[(size_t) r] / juce::jmax (1e-12, bassTot);
                if (c > k1) { k2 = k1; res.altRoot = res.keyRoot; res.altMinor = res.keyMinor; k1 = c; res.keyRoot = r; res.keyMinor = m == 1; }
                else if (c > k2) { k2 = c; res.altRoot = r; res.altMinor = m == 1; }
            }
        res.keyConfidence = (float) juce::jlimit (0.0, 1.0, (k1 - k2) * 5.0 + 0.35);

        if (progress) progress (0.9f);

        // ---------------- linha do tempo de energia (0,5 s) e eventos
        const int perSeg = juce::jmax (1, (int) (frameRate * 0.5));
        float emax = 1e-9f;
        for (int f = 0; f < frames; f += perSeg)
        {
            double s = 0; int c = 0;
            for (int i = f; i < juce::jmin (frames, f + perSeg); ++i) { s += frameRms[(size_t) i]; ++c; }
            res.energy.push_back ((float) (s / juce::jmax (1, c)));
            emax = juce::jmax (emax, res.energy.back());
        }
        for (auto& e : res.energy) e /= emax;
        int impacts = 0;
        for (size_t i = 2; i < res.energy.size(); ++i)
        {
            const float before = 0.5f * (res.energy[i - 1] + res.energy[i - 2]);
            if (res.energy[i] > 0.55f && res.energy[i] > before * 1.8f) { res.events.push_back ({ i * 0.5, "IMPACT" }); ++impacts; }
        }
        // secoes: media movel de 4 s acima/abaixo da media geral
        double meanE = 0; for (auto e : res.energy) meanE += e; meanE /= juce::jmax ((size_t) 1, res.energy.size());
        int state = 0;
        for (size_t i = 0; i + 8 <= res.energy.size(); i += 4)
        {
            double s = 0; for (size_t j = i; j < i + 8; ++j) s += res.energy[j]; s /= 8;
            const int st = s > meanE * 1.15 ? 1 : (s < meanE * 0.75 ? -1 : 0);
            if (st != state && st != 0) res.events.push_back ({ i * 0.5, st > 0 ? "HIGH" : "LOW" });
            state = st;
        }

        // ---------------- BEAT DNA
        const double beats = res.durationSec * res.bpm / 60.0;
        // limiar ADAPTATIVO (percentil 95, nao o maximo): um impacto isolado nao pode "cegar" a deteccao
        auto p95 = [] (std::vector<float> v) { if (v.empty()) return 0.0f; std::nth_element (v.begin(), v.begin() + (long) (v.size() * 95 / 100), v.end()); return v[v.size() * 95 / 100]; };
        auto onsetsPerBeat = [&] (std::vector<float> v)
        {
            normalize (v);
            const double thr = 0.35 * p95 (v);
            int c = 0;
            for (size_t i = 1; i + 1 < v.size(); ++i)
                if (v[i] > thr && v[i] >= v[i - 1] && v[i] > v[i + 1]) ++c;
            return c / juce::jmax (1.0, beats);
        };
        const double kpb = onsetsPerBeat (lowFlux), hpb = onsetsPerBeat (highFlux);
        res.kickDensity = (float) juce::jlimit (0.0, 1.0, kpb / 3.0);
        res.hatDensity  = (float) juce::jlimit (0.0, 1.0, hpb / 4.0);
        {   // half-time: caixa (medios) com periodo de 2 tempos mais forte que 1 tempo
            std::vector<float> m = midFlux; normalize (m);
            auto acAt = [&] (double beatsLag)
            {
                const int lag = (int) std::round (beatsLag * 60.0 / res.bpm * frameRate);
                double s = 0;
                for (int i = 0; i + lag < frames; ++i)
                    s += (double) m[(size_t) i] * m[(size_t) (i + lag)];
                return s;
            };
            const double a2 = acAt (2.0), a4 = acAt (4.0), a1 = acAt (1.0);
            res.halfTime = (float) juce::jlimit (0.0, 1.0, (a4 / juce::jmax (1e-12, a2) - 0.9) * 1.5 + (a2 > a1 ? 0.0 : -0.2));
        }
        {   // sincope: ataques graves fora da grade de colcheias
            std::vector<float> v = lowFlux; normalize (v);
            const double mx = p95 (v) / 0.35 * 0.3;   // mesmo limiar adaptativo
            const double eighth = 30.0 / res.bpm * frameRate;
            int on = 0, off = 0;
            for (size_t i = 1; i + 1 < v.size(); ++i)
                if (v[i] > 0.3 * mx && v[i] >= v[i - 1] && v[i] > v[i + 1])
                {
                    const double ph = std::fmod ((double) i, eighth) / eighth;
                    (ph < 0.2 || ph > 0.8) ? ++on : ++off;
                }
            res.syncopation = (float) juce::jlimit (0.0, 1.0, (double) off / juce::jmax (1, on + off) * 1.6);
        }
        res.trap = juce::jlimit (0.0f, 1.0f, (float) (hpb / 4.0) * 0.6f + res.bands[0] * 3.0f * 0.4f + res.halfTime * 0.3f - 0.15f);
        res.electronic = juce::jlimit (0.0f, 1.0f, res.bands[0] * 2.5f + (1.0f - juce::jlimit (0.0f, 1.0f, res.crestDb / 20.0f)) * 0.3f);

        // ---------------- SOUND DNA
        res.sub = juce::jlimit (0.0f, 1.0f, res.bands[0] * 4.0f);
        res.bright = juce::jlimit (0.0f, 1.0f, (res.centroidHz - 800.0f) / 3200.0f);
        res.distortion = juce::jlimit (0.0f, 1.0f, (res.bands[4] + res.bands[5]) * 3.0f * 0.5f + (1.0f - juce::jlimit (0.0f, 1.0f, (res.crestDb - 6.0f) / 14.0f)) * 0.5f);
        res.stereo = juce::jlimit (0.0f, 1.0f, res.stereoWidth * 1.6f);
        double fm = 0; for (auto x : flux) fm += x; fm /= frames;
        double fmx = 0; for (auto x : flux) fmx = juce::jmax (fmx, (double) x);
        res.density = (float) juce::jlimit (0.0, 1.0, fm / juce::jmax (1e-9, fmx) * 4.0);
        res.darkness = 1.0f - res.bright;
        res.aggression = juce::jlimit (0.0f, 1.0f, 0.5f * res.distortion + 0.3f * juce::jlimit (0.0f, 1.0f, (res.rmsDb + 24.0f) / 16.0f) + 0.2f * res.kickDensity);
        res.impact = juce::jlimit (0.0f, 1.0f, impacts / juce::jmax (1.0f, (float) res.durationSec / 30.0f) / 4.0f);

        if (progress) progress (1.0f);
        res.ok = true;
        return res;
    }
};

// ---------------- cache (JSON) ----------------
inline juce::var ReferenceResult::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("file", fileName); o->setProperty ("dur", durationSec); o->setProperty ("bpm", bpm); o->setProperty ("bpmC", bpmConfidence);
    o->setProperty ("key", keyRoot); o->setProperty ("minor", keyMinor); o->setProperty ("keyC", keyConfidence);
    juce::Array<juce::var> b, e, ev;
    for (auto x : bands) b.add (x);
    for (auto x : energy) e.add (x);
    for (auto& [t, s] : events) ev.add (juce::Array<juce::var> { t, s });
    o->setProperty ("bands", b);
    o->setProperty ("energy", e);
    o->setProperty ("events", ev);
    const float vals[] { centroidHz, rolloffHz, rmsDb, peakDb, crestDb, stereoCorrelation, stereoWidth, kickDensity, hatDensity, halfTime,
                         syncopation, trap, electronic, sub, bright, distortion, stereo, density, darkness, aggression, impact };
    juce::Array<juce::var> v;
    for (auto x : vals) v.add (x);
    o->setProperty ("v", v);
    return juce::var (o);
}

inline ReferenceResult ReferenceResult::fromVar (const juce::var& j)
{
    ReferenceResult r;
    if (! j.isObject()) return r;
    r.fileName = j["file"].toString(); r.durationSec = j["dur"]; r.bpm = j["bpm"]; r.bpmConfidence = (float) (double) j["bpmC"];
    r.keyRoot = j["key"]; r.keyMinor = j["minor"]; r.keyConfidence = (float) (double) j["keyC"];
    if (auto* b = j["bands"].getArray()) for (int i = 0; i < juce::jmin (6, b->size()); ++i) r.bands[(size_t) i] = (float) (double) (*b)[i];
    if (auto* e = j["energy"].getArray()) for (auto& x : *e) r.energy.push_back ((float) (double) x);
    if (auto* ev = j["events"].getArray()) for (auto& p : *ev) if (auto* a = p.getArray(); a != nullptr && a->size() == 2) r.events.push_back ({ (double) (*a)[0], (*a)[1].toString() });
    if (auto* v = j["v"].getArray(); v != nullptr && v->size() == 21)
    {
        float* dst[] { &r.centroidHz, &r.rolloffHz, &r.rmsDb, &r.peakDb, &r.crestDb, &r.stereoCorrelation, &r.stereoWidth, &r.kickDensity,
                       &r.hatDensity, &r.halfTime, &r.syncopation, &r.trap, &r.electronic, &r.sub, &r.bright, &r.distortion, &r.stereo,
                       &r.density, &r.darkness, &r.aggression, &r.impact };
        for (int i = 0; i < 21; ++i) *dst[i] = (float) (double) (*v)[i];
    }
    r.ok = r.bpm > 0;
    return r;
}
} // namespace mm
