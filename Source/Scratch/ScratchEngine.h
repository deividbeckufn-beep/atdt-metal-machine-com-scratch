#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "Parameters/Parameters.h"
#include "ScratchPattern.h"
#include "ScratchSources.h"
#include "ScratchFX.h"

namespace mm
{
// =============================================================================
//  SCRATCH MACHINE ENGINE - toca-discos virtual
//  * o audio e LIDO na posicao do prato: o pitch sai da VELOCIDADE do prato (como no vinil real)
//  * MOMENTUM = inercia do prato; ACCEL/DECEL deformam a mao; RANDOMNESS = nenhum golpe identico
//  * disparo: TRIGGER (teclas C4 = padrao, C#4..G#4 = QUICK) | SYNC (grade do host + swing) | FREE
//  * SCRATCH PAD: o mouse move o prato em tempo real (atomico, sem travas)
//  * fontes: 10 sinteses originais (SYNTH SCRATCH) ou o WAV do usuario (SAMPLE SCRATCH)
//  Sem alocacao no audio thread; troca de sample protegida por SpinLock (try-lock no audio).
// =============================================================================
class ScratchEngine
{
public:
    static constexpr int numMoves = ScratchPattern::Count;
    static const juce::StringArray& moveNames()   { return ScratchPattern::names(); }
    static const juce::StringArray& sourceNames() { return ScratchSources::names(); }
    static double motion (int move, double u, float& gate) { return ScratchPattern::motion (move, u, 0.0, 0.5, 1, gate); }

    // QUICK SCRATCH (C#4..G#4 e botoes grandes)
    static constexpr int quick[8] { ScratchPattern::Baby, ScratchPattern::Chirp, ScratchPattern::Flare, ScratchPattern::Transformer,
                                    ScratchPattern::Crab, ScratchPattern::Reverse, ScratchPattern::Stab, ScratchPattern::Brake };

    struct Params
    {
        using P = std::atomic<float>*;
        P on{}, mode{}, move{}, rate{}, depth{}, start{}, fader{}, level{}, source{};
        P engine{}, speed{}, direction{}, curve{}, attack{}, release{}, gate{}, resonance{}, tone{}, noise{}, vinyl{}, pitch{}, width{},
          momentum{}, accel{}, decel{}, randomness{}, swing{}, repeat{}, variation{}, hold{}, loop{}, end{}, center{}, fadeIn{}, fadeOut{},
          intensity{}, fxFilter{}, fxDrive{}, fxSat{}, fxCrush{}, fxDelay{}, fxReverb{}, fxWidth{}, fxVinyl{}, fxMix{}, master{};
        void attach (juce::AudioProcessorValueTreeState& s)
        {
            auto g = [&s] (const char* id) { auto* p = s.getRawParameterValue (id); jassert (p != nullptr); return p; };
            on = g ("scr_on"); mode = g ("scr_mode"); move = g ("scr_move"); rate = g ("scr_rate"); depth = g ("scr_depth");
            start = g ("scr_start"); fader = g ("scr_fader"); level = g ("scr_level"); source = g ("scr_source");
            engine = g ("scr_engine"); speed = g ("scr_speed"); direction = g ("scr_direction"); curve = g ("scr_curve");
            attack = g ("scr_attack"); release = g ("scr_release"); gate = g ("scr_gate"); resonance = g ("scr_resonance");
            tone = g ("scr_tone"); noise = g ("scr_noise"); vinyl = g ("scr_vinyl"); pitch = g ("scr_pitch"); width = g ("scr_width");
            momentum = g ("scr_momentum"); accel = g ("scr_accel"); decel = g ("scr_decel"); randomness = g ("scr_random");
            swing = g ("scr_swing"); repeat = g ("scr_repeat"); variation = g ("scr_variation"); hold = g ("scr_hold"); loop = g ("scr_loop");
            end = g ("scr_end"); center = g ("scr_center"); fadeIn = g ("scr_fadein"); fadeOut = g ("scr_fadeout"); intensity = g ("scr_intensity");
            fxFilter = g ("scr_fx_filter"); fxDrive = g ("scr_fx_drive"); fxSat = g ("scr_fx_sat"); fxCrush = g ("scr_fx_crush");
            fxDelay = g ("scr_fx_delay"); fxReverb = g ("scr_fx_reverb"); fxWidth = g ("scr_fx_width"); fxVinyl = g ("scr_fx_vinyl");
            fxMix = g ("scr_fx_mix"); master = g ("scr_master");
        }
    };

    explicit ScratchEngine (const Params& p) : params (p) {}

    // message thread
    void prepare (double sr, int maxBlock = 4096)
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        sampleRate = sr;
        for (int i = 0; i < ScratchSources::count; ++i)
            if (i != ScratchSources::userIndex) sources[(size_t) i] = ScratchSources::make (i, sr);
        tmpL.assign ((size_t) juce::jmax (512, maxBlock), 0.0f); tmpR = tmpL;
        haas.assign ((size_t) (sr * 0.02) + 4, 0.0f); haasPos = 0;
        fx.prepare (sr);
        hitActive = false; triggerPending = -1; heldNote = -1;
    }

    bool loadUserSample (const juce::File& f, juce::String& error)
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
        if (r == nullptr) { error = "Formato nao suportado (use WAV, AIFF, FLAC ou MP3)."; return false; }
        const int n = (int) juce::jmin (r->lengthInSamples, (juce::int64) (r->sampleRate * 30.0));
        if (n < 1000) { error = "Sample curto demais."; return false; }
        juce::AudioBuffer<float> tmp ((int) juce::jmin (2u, r->numChannels), n);
        r->read (&tmp, 0, n, 0, true, tmp.getNumChannels() > 1);
        const double ratio = r->sampleRate / sampleRate;
        const int outN = (int) (n / ratio);
        juce::AudioBuffer<float> mono (1, outN);
        for (int i = 0; i < outN; ++i)
        {
            const double pos = i * ratio; const int i0 = juce::jmin (n - 1, (int) pos), i1 = juce::jmin (n - 1, i0 + 1);
            const float fr = (float) (pos - i0); float v = 0;
            for (int c = 0; c < tmp.getNumChannels(); ++c) v += tmp.getSample (c, i0) + (tmp.getSample (c, i1) - tmp.getSample (c, i0)) * fr;
            mono.setSample (0, i, v / (float) tmp.getNumChannels());
        }
        ScratchSources::normalize (mono);
        { const juce::SpinLock::ScopedLockType sl (lock); sources[(size_t) ScratchSources::userIndex] = std::move (mono); }
        userName = f.getFileName();
        return true;
    }
    juce::String getUserSampleName() const { return userName; }
    bool hasUserSample() const { return sources[(size_t) ScratchSources::userIndex].getNumSamples() > 0; }
    const juce::AudioBuffer<float>* getSourceForDisplay() const
    {
        const auto& b = sources[(size_t) currentSourceIndex()];
        return b.getNumSamples() > 0 ? &b : nullptr;
    }

    // ---- interface -> audio (atomicos)
    std::atomic<int> uiTrigger { -1 };          // dispara um padrao (botoes / QUICK)
    std::atomic<bool> uiRelease { false };      // RELEASE: para o golpe atual
    std::atomic<float> padTarget { 0.0f };      // SCRATCH PAD: posicao alvo (0..1 da regiao)
    std::atomic<bool> padActive { false };
    // ---- audio -> interface
    std::atomic<float> displayPos { 0.0f };     // posicao do prato (0..1 da fonte)
    std::atomic<float> platterSpeed { 0.0f };   // velocidade com sinal (o disco gira junto)

    // audio thread: TRIGGER -> C4 = padrao selecionado, C#4..G#4 = QUICK; HOLD repete enquanto a tecla esta pressionada
    void handleMidi (juce::MidiBuffer& midi)
    {
        if (params.on->load() < 0.5f || (int) params.mode->load() != 0) return;
        juce::MidiBuffer keep;
        for (const auto m : midi)
        {
            const auto msg = m.getMessage();
            const int note = msg.isNoteOnOrOff() ? msg.getNoteNumber() : -1;
            if (note >= 60 && note <= 68)
            {
                if (msg.isNoteOn())
                {
                    triggerPending = note == 60 ? (int) params.move->load() : quick[note - 61];
                    triggerOffset = m.samplePosition; triggerVel = msg.getFloatVelocity(); heldNote = note;
                }
                else if (note == heldNote) heldNote = -1;
                continue;
            }
            keep.addEvent (msg, m.samplePosition);
        }
        midi.swapWith (keep);
    }

    void process (juce::AudioBuffer<float>& out, int numSamples, double bpm, bool hostPlaying, double ppq)
    {
        if (params.on->load() < 0.5f) { hitActive = false; platterSpeed = 0.0f; return; }
        const juce::SpinLock::ScopedTryLockType tl (lock);
        if (! tl.isLocked()) return;
        const auto& src = sources[(size_t) currentSourceIndex()];
        const int len = src.getNumSamples();
        if (len < 2 || tmpL.empty()) return;
        const float* data = src.getReadPointer (0);
        bpm = juce::jlimit (20.0, 999.0, bpm);

        static const double rateBeats[] { 1.0, 0.5, 0.25, 2.0, 4.0, 0.125, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 1.0 / 12.0 };
        stepSamples = rateBeats[juce::jlimit (0, 9, (int) params.rate->load())] * 60.0 / bpm * sampleRate;
        if (const int ui = uiTrigger.exchange (-1); ui >= 0) startHit (ui, 0.9f, true);
        if (uiRelease.exchange (false)) { hitActive = false; repeatsLeft = 0; }
        const int mode = (int) params.mode->load();
        // regiao (START / END / CENTER)
        const double rs = juce::jlimit (0.0, 0.98, (double) params.start->load()) * (len - 1);
        const double re = juce::jlimit (rs + 64.0, (double) len - 1, (double) params.end->load() * (len - 1));
        const double rc = juce::jlimit (rs, re, rs + (re - rs) * params.center->load());
        const double pitchMul = std::exp2 (params.pitch->load() / 12.0f);
        const double range = juce::jmin (re - rs, (0.05 + 0.6 * params.depth->load()) * sampleRate * pitchMul);
        const int dir = (int) params.direction->load();
        const double follow = 1.0 - 0.9993 * params.momentum->load();            // inercia do prato
        const float level = params.level->load();
        const float faderSharp = 0.02f + 0.3f * params.fader->load();
        const float gateAmt = params.gate->load();
        const float noiseAmt = params.noise->load(), vinylAmt = params.vinyl->load(), width = params.width->load();
        const float atk = juce::jmax (0.0005f, params.attack->load() * 0.001f), rel = juce::jmax (0.002f, params.release->load() * 0.001f);
        const double fc = 300.0 * std::pow (55.0, (double) params.tone->load());   // TONE: passa-baixa ressonante
        const double g = std::tan (juce::MathConstants<double>::pi * juce::jmin (0.45, fc / sampleRate));
        const double k = 2.0 - 1.85 * params.resonance->load();
        const double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const int haasLen = (int) haas.size();
        const int haasDelay = juce::jlimit (1, haasLen - 2, (int) (width * 0.012 * sampleRate) + 1);
        const double fadeInS = params.fadeIn->load() * (re - rs), fadeOutS = params.fadeOut->load() * (re - rs);
        const bool pad = padActive.load (std::memory_order_relaxed);
        // SYNC: transporte comecou (ou voltou num loop) -> o primeiro tempo dispara de novo
        if (hostPlaying && ! wasPlaying) lastGridIndex = -1;
        wasPlaying = hostPlaying;

        for (int pos0 = 0; pos0 < numSamples; pos0 += (int) tmpL.size())
        {
            const int n = juce::jmin ((int) tmpL.size(), numSamples - pos0);
            for (int i = 0; i < n; ++i)
            {
                const int si = pos0 + i;
                if (mode == 0 && triggerPending >= 0 && si >= triggerOffset) { startHit (triggerPending, triggerVel, false); triggerPending = -1; }
                if (mode != 0 && (hostPlaying || mode == 2))
                {
                    const double gp = hostPlaying ? ppq * 60.0 / bpm * sampleRate + si : (double) clock;
                    const auto idx = (juce::int64) std::floor (gp / stepSamples);
                    if (idx < lastGridIndex) lastGridIndex = idx - 1;                    // salto para tras (loop do host)
                    const double swingDelay = (idx & 1) ? params.swing->load() * stepSamples / 3.0 : 0.0;   // SWING
                    if (idx != lastGridIndex && gp - (double) idx * stepSamples >= swingDelay) { lastGridIndex = idx; startHit ((int) params.move->load(), 0.9f, false); }
                }
                ++clock;

                double target = platterPos;
                float gate = 0.0f;
                if (pad)   // SCRATCH PAD: o mouse e a mao no disco
                {
                    target = rs + juce::jlimit (0.0f, 1.0f, padTarget.load (std::memory_order_relaxed)) * (re - rs);
                    gate = 1.0f;
                }
                else if (hitActive)
                {
                    const double u = hitTime / hitLen;
                    if (u >= 1.0)
                    {
                        const bool again = repeatsLeft > 0 || params.loop->load() > 0.5f || (params.hold->load() > 0.5f && heldNote >= 0);
                        if (repeatsLeft > 0) --repeatsLeft;
                        if (again) { hitTime = 0.0; ++hitSeed; } else hitActive = false;
                    }
                    else
                    {
                        const double uw = ScratchPattern::warp (u, params.accel->load(), params.decel->load());
                        double p = ScratchPattern::motion (hitPattern, uw, params.curve->load(), params.variation->load(), hitSeed, gate) * hitDepth;
                        target = dir == 1 ? re - p * range : dir == 2 ? rc + (p - 0.5) * range : rs + p * range;
                        if (gateAmt > 0.001f && std::fmod (hitTime / (stepSamples / 8.0), 1.0) > 0.5) gate *= 1.0f - gateAmt;   // GATE extra
                        hitTime += 1.0;
                    }
                }
                env = gate > 0.0f ? juce::jmin (1.0f, env + 1.0f / (atk * (float) sampleRate))
                                  : juce::jmax (0.0f, env - 1.0f / (rel * (float) sampleRate));
                // fisica do prato (MOMENTUM)
                const double prev = platterPos;
                platterPos += (juce::jlimit (0.0, (double) len - 2.0, target) - platterPos) * follow;
                const double speed = std::abs (platterPos - prev);
                speedSmooth += (float) ((platterPos - prev) - speedSmooth) * 0.01f;
                // leitura na posicao do prato
                const double rp = juce::jlimit (0.0, (double) len - 2.0, platterPos);
                const int i0 = (int) rp; const float fr = (float) (rp - i0);
                float x = data[i0] + (data[i0 + 1] - data[i0]) * fr;
                if (fadeInS > 1.0 && rp - rs < fadeInS) x *= (float) juce::jmax (0.0, (rp - rs) / fadeInS);
                if (fadeOutS > 1.0 && re - rp < fadeOutS) x *= (float) juce::jmax (0.0, (re - rp) / fadeOutS);
                const float speedGain = (float) juce::jlimit (0.0, 1.0, speed * 4.0);   // vinil parado nao toca
                x += (rng.nextFloat() * 2.0f - 1.0f) * noiseAmt * 0.25f * speedGain;   // atrito da agulha
                vinylNoise = 0.95f * vinylNoise + 0.05f * (rng.nextFloat() * 2.0f - 1.0f);
                x += vinylNoise * vinylAmt * 0.6f * juce::jmin (1.0f, speedGain * 2.0f);
                if (rng.nextFloat() < 0.0005f * vinylAmt) x += (rng.nextFloat() * 2.0f - 1.0f) * 0.4f * vinylAmt;
                const double v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;   // tom ressonante
                ic1 = 2.0 * v1 - ic1; ic2 = 2.0 * v2 - ic2;
                x = (float) v2;
                faderState += (gate - faderState) * faderSharp;                       // crossfader sem cliques
                x *= faderState * speedGain * env * hitVel * level;   // LEVEL antes dos efeitos: a protecao de saida fica de reserva
                haas[(size_t) haasPos] = x;                                            // WIDTH (Haas)
                const float xr = haas[(size_t) ((haasPos - haasDelay + haasLen) % haasLen)];
                if (++haasPos >= haasLen) haasPos = 0;
                tmpL[(size_t) i] = x;
                tmpR[(size_t) i] = x * (1.0f - width) + xr * width;
            }
            ScratchFX::Settings fs { params.fxFilter->load(), params.fxDrive->load(), params.fxSat->load(), params.fxCrush->load(),
                                     params.fxDelay->load(), params.fxReverb->load(), params.fxWidth->load(), params.fxVinyl->load(),
                                     params.fxMix->load(), params.master->load() };
            fx.process (tmpL.data(), tmpR.data(), n, fs, bpm, hitActive || pad || env > 0.001f);
            float* L = out.getWritePointer (0, pos0);
            float* R = out.getNumChannels() > 1 ? out.getWritePointer (1, pos0) : nullptr;
            for (int i = 0; i < n; ++i) { L[i] += tmpL[(size_t) i]; if (R) R[i] += tmpR[(size_t) i]; }
        }
        triggerOffset = 0;
        displayPos.store ((float) (platterPos / juce::jmax (1, len - 1)), std::memory_order_relaxed);
        platterSpeed.store (speedSmooth, std::memory_order_relaxed);
    }

private:
    int currentSourceIndex() const
    {
        const int sel = juce::jlimit (0, ScratchSources::count - 1, (int) params.source->load());
        if (params.engine->load() > 0.5f && sources[(size_t) ScratchSources::userIndex].getNumSamples() > 0) return ScratchSources::userIndex;
        return sel == ScratchSources::userIndex ? 0 : sel;   // SAMPLE sem arquivo: usa a voz embutida
    }

    void startHit (int pattern, float vel, bool quickButton)
    {
        int p = juce::jlimit (0, numMoves - 1, pattern);
        const float rnd = params.randomness->load();
        if (p == ScratchPattern::Random) p = rng.nextInt ((int) ScratchPattern::Random);
        hitPattern = p; hitActive = true; hitTime = 0.0; ++hitSeed;
        const float intensity = quickButton ? params.intensity->load() : 0.7f;
        hitLen = juce::jmax (64.0, stepSamples / juce::jlimit (0.25, 4.0, (double) params.speed->load())
                                   * (1.0 + (rng.nextFloat() - 0.5) * rnd * 0.3));                  // RANDOMNESS no tempo
        hitDepth = juce::jlimit (0.2, 1.3, (0.75 + 0.5 * intensity) * (1.0 + (rng.nextFloat() - 0.5) * rnd * 0.6));   // e na profundidade
        hitVel = 0.4f + 0.6f * vel;
        repeatsLeft = juce::jmax (0, (int) params.repeat->load() - 1);
    }

    const Params& params;
    double sampleRate = 44100.0, stepSamples = 24000.0;
    juce::SpinLock lock;
    std::array<juce::AudioBuffer<float>, ScratchSources::count> sources;
    juce::String userName;
    std::vector<float> tmpL, tmpR, haas;
    int haasPos = 0;
    ScratchFX fx;
    juce::Random rng;
    bool hitActive = false;
    int hitPattern = 0, repeatsLeft = 0, triggerPending = -1, triggerOffset = 0, heldNote = -1;
    uint32_t hitSeed = 1;
    float triggerVel = 1.0f, hitVel = 1.0f, faderState = 0.0f, env = 0.0f, vinylNoise = 0.0f, speedSmooth = 0.0f;
    double hitTime = 0.0, hitLen = 24000.0, hitDepth = 1.0, platterPos = 0.0, ic1 = 0.0, ic2 = 0.0;
    juce::int64 clock = 0, lastGridIndex = -1;
    bool wasPlaying = false;
};
} // namespace mm
