#pragma once
#include <juce_dsp/juce_dsp.h>
#include "Parameters/Parameters.h"

namespace mm
{
// =============================================================================
//  FxEngine - cadeia de efeitos real (v0.6):
//  DISTORTION (2x oversampling) -> EQ -> CHORUS -> DELAY (sync) -> REVERB -> COMPRESSOR
//  ... master gain (no Processor) ... -> LIMITER + CLIPPER (teto -0.3 dBFS garantido)
//  Cada modulo liga/desliga com crossfade (sem clicks). Tudo pre-alocado em
//  prepare(): nenhuma alocacao no audio thread.
// =============================================================================
class FxEngine
{
public:
    struct Params
    {
        using P = std::atomic<float>*;
        P distOn{}, distType{}, distDrive{}, distTone{}, distMix{};
        P eqOn{}, eqLow{}, eqMid{}, eqHigh{};
        P chOn{}, chRate{}, chDepth{}, chMix{};
        P dlOn{}, dlDiv{}, dlFb{}, dlMix{}, dlPing{};
        P rvOn{}, rvSize{}, rvDamp{}, rvWidth{}, rvMix{};
        P cpOn{}, cpThresh{}, cpRatio{}, cpAttack{}, cpRelease{}, cpMakeup{};
        P limOn{};
        // v0.9
        P satOn{}, satType{}, satDrive{}, satMix{};
        P crOn{}, crBits{}, crDown{}, crMix{};
        P phOn{}, phRate{}, phDepth{}, phFb{}, phMix{};
        P flOn{}, flRate{}, flDepth{}, flFb{}, flMix{};
        P glOn{}, glMode{}, glDiv{}, glMix{};

        void attach (juce::AudioProcessorValueTreeState& s)
        {
            auto g = [&s] (const char* id) { auto* p = s.getRawParameterValue (id); jassert (p != nullptr); return p; };
            distOn = g (ParamIDs::fxDistOn); distType = g (ParamIDs::fxDistType); distDrive = g (ParamIDs::fxDistDrive);
            distTone = g (ParamIDs::fxDistTone); distMix = g (ParamIDs::fxDistMix);
            eqOn = g (ParamIDs::fxEqOn); eqLow = g (ParamIDs::fxEqLow); eqMid = g (ParamIDs::fxEqMid); eqHigh = g (ParamIDs::fxEqHigh);
            chOn = g (ParamIDs::fxChOn); chRate = g (ParamIDs::fxChRate); chDepth = g (ParamIDs::fxChDepth); chMix = g (ParamIDs::fxChMix);
            dlOn = g (ParamIDs::fxDlOn); dlDiv = g (ParamIDs::fxDlDiv); dlFb = g (ParamIDs::fxDlFb); dlMix = g (ParamIDs::fxDlMix);
            dlPing = g (ParamIDs::fxDlPing);
            rvOn = g (ParamIDs::fxRvOn); rvSize = g (ParamIDs::fxRvSize); rvDamp = g (ParamIDs::fxRvDamp);
            rvWidth = g (ParamIDs::fxRvWidth); rvMix = g (ParamIDs::fxRvMix);
            cpOn = g (ParamIDs::fxCpOn); cpThresh = g (ParamIDs::fxCpThresh); cpRatio = g (ParamIDs::fxCpRatio);
            cpAttack = g (ParamIDs::fxCpAttack); cpRelease = g (ParamIDs::fxCpRelease); cpMakeup = g (ParamIDs::fxCpMakeup);
            limOn = g (ParamIDs::fxLimOn);
            satOn = g ("fx_sat_on"); satType = g ("fx_sat_type"); satDrive = g ("fx_sat_drive"); satMix = g ("fx_sat_mix");
            crOn = g ("fx_cr_on"); crBits = g ("fx_cr_bits"); crDown = g ("fx_cr_down"); crMix = g ("fx_cr_mix");
            phOn = g ("fx_ph_on"); phRate = g ("fx_ph_rate"); phDepth = g ("fx_ph_depth"); phFb = g ("fx_ph_fb"); phMix = g ("fx_ph_mix");
            flOn = g ("fx_fl_on"); flRate = g ("fx_fl_rate"); flDepth = g ("fx_fl_depth"); flFb = g ("fx_fl_fb"); flMix = g ("fx_fl_mix");
            glOn = g ("fx_gl_on"); glMode = g ("fx_gl_mode"); glDiv = g ("fx_gl_div"); glMix = g ("fx_gl_mix");
        }
    };

    explicit FxEngine (const Params& p) : params (p) {}

    void prepare (double sr, int maxBlock)
    {
        sampleRate = sr;
        blockSize = juce::jmax (32, maxBlock);
        const juce::dsp::ProcessSpec spec { sr, (juce::uint32) blockSize, 2 };

        temp.setSize (2, blockSize, false, true, false);

        oversampler.initProcessing ((size_t) blockSize);
        oversampler.reset();
        for (auto& f : toneState) f = 0.0f;
        dcIn = {}; dcOut = {};

        for (auto& f : eq) { f.prepare (spec); f.reset(); }
        lastEq = { -99.0f, -99.0f, -99.0f };

        chorus.prepare (spec); chorus.reset();
        phaser.prepare (spec); phaser.reset();
        flLen = (int) (sr * 0.03) + 4;
        for (auto& d : flBuf) d.assign ((size_t) flLen, 0.0f);
        flPos = 0; flPhase = 0.0;
        crHold = {}; crCounter = 0;
        satDcIn = {}; satDcOut = {};
        glLen = (int) (sr * 4.0) + 4;
        for (auto& d : glBuf) d.assign ((size_t) glLen, 0.0f);
        glWrite = 0; glSample = 0; glTapePos = 0.0;
        chorus.setCentreDelay (7.0f); chorus.setFeedback (0.0f);

        delayLen = (int) (sr * 4.0) + 4;
        for (auto& d : delayBuf) d.assign ((size_t) delayLen, 0.0f);
        writePos = 0;
        delaySmoothed.reset (sr, 0.08);
        delaySmoothed.setCurrentAndTargetValue ((float) (sr * 0.25));
        for (auto& d : dampState) d = 0.0f;

        reverb.prepare (spec); reverb.reset();
        compressor.prepare (spec); compressor.reset();
        limiter.prepare (spec); limiter.reset();
        limiter.setThreshold (-0.3f); limiter.setRelease (60.0f);
        makeup.reset (sr, 0.05);

        for (auto& m : mixes) m.reset (sr, 0.02);
        for (auto& m : mixes) m.setCurrentAndTargetValue (0.0f);
    }

    // DISTORTION .. COMPRESSOR (antes do master gain)
    void processMain (juce::AudioBuffer<float>& buffer, int numSamples, double bpm, bool playing = false, double ppq = 0.0)
    {
        hostPlaying = playing; hostPpq = ppq; hostBpm = juce::jlimit (20.0, 999.0, bpm);
        for (int pos = 0; pos < numSamples; pos += blockSize)
        {
            const int n = juce::jmin (blockSize, numSamples - pos);
            runModule (Dist,   params.distOn, buffer, pos, n, [this] (juce::AudioBuffer<float>& b, int nn) { distortion (b, nn); },
                       juce::jlimit (0.0f, 1.0f, params.distMix->load()));
            runModule (Sat,    params.satOn,  buffer, pos, n, [this] (juce::AudioBuffer<float>& b, int nn) { saturation (b, nn); },
                       juce::jlimit (0.0f, 1.0f, params.satMix->load()));
            runModule (Crush,  params.crOn,   buffer, pos, n, [this] (juce::AudioBuffer<float>& b, int nn) { bitcrush (b, nn); },
                       juce::jlimit (0.0f, 1.0f, params.crMix->load()));
            runModule (Eq,     params.eqOn,   buffer, pos, n, [this] (juce::AudioBuffer<float>& b, int nn) { equalizer (b, nn); });
            runModule (Chorus, params.chOn,   buffer, pos, n, [this] (juce::AudioBuffer<float>& b, int nn) { chorusFx (b, nn); });
            runModule (Phaser, params.phOn,   buffer, pos, n, [this] (juce::AudioBuffer<float>& b, int nn) { phaserFx (b, nn); });
            runModule (Flanger,params.flOn,   buffer, pos, n, [this] (juce::AudioBuffer<float>& b, int nn) { flangerFx (b, nn); },
                       juce::jlimit (0.0f, 1.0f, params.flMix->load()));
            runModule (Glitch, params.glOn,   buffer, pos, n, [this, pos] (juce::AudioBuffer<float>& b, int nn) { glitchFx (b, nn, pos); },
                       juce::jlimit (0.0f, 1.0f, params.glMix->load()));
            runModule (Delay,  params.dlOn,   buffer, pos, n, [this, bpm] (juce::AudioBuffer<float>& b, int nn) { delayFx (b, nn, bpm); });
            runModule (Reverb, params.rvOn,   buffer, pos, n, [this] (juce::AudioBuffer<float>& b, int nn) { reverbFx (b, nn); });
            runModule (Comp,   params.cpOn,   buffer, pos, n, [this] (juce::AudioBuffer<float>& b, int nn) { compressorFx (b, nn); });
        }
    }

    // LIMITER (depois do master gain): teto em -0.3 dBFS
    void processLimiter (juce::AudioBuffer<float>& buffer, int numSamples)
    {
        for (int pos = 0; pos < numSamples; pos += blockSize)
        {
            const int n = juce::jmin (blockSize, numSamples - pos);
            runModule (Limit, params.limOn, buffer, pos, n, [this] (juce::AudioBuffer<float>& b, int nn)
            {
                juce::dsp::AudioBlock<float> blk (b.getArrayOfWritePointers(), 2, (size_t) nn);
                limiter.process (juce::dsp::ProcessContextReplacing<float> (blk));
                // CLIPPER de seguranca: o limiter nao tem lookahead, entao transientes rapidos
                // passariam do teto. Aqui o teto de -0.3 dBFS e garantido amostra a amostra.
                // o dsp::Limiter normaliza a saida para 0 dBFS: primeiro traz para o teto (-0.3 dB),
                // e o clip fica so como protecao (nao distorce o sinal normalmente)
                for (int c = 0; c < 2; ++c)
                {
                    juce::FloatVectorOperations::multiply (b.getWritePointer (c), ceiling, nn);
                    juce::FloatVectorOperations::clip (b.getWritePointer (c), b.getReadPointer (c), -ceiling, ceiling, nn);
                }
            });
        }
    }

    static const juce::StringArray& distTypeNames() { static const juce::StringArray n { "Soft", "Hard", "Fuzz", "Rectify" }; return n; }
    static const juce::StringArray& delayDivNames() { static const juce::StringArray n { "1/4", "1/8", "1/8D", "1/16", "1/4T", "1/8T", "1/2" }; return n; }

private:
    enum Module { Dist = 0, Eq, Chorus, Delay, Reverb, Comp, Limit, Sat, Crush, Phaser, Flanger, Glitch, NumModules };

    // Crossfade liga/desliga: processa uma copia e mistura com o sinal seco
    // wetScale < 1: mistura paralela (ex.: MIX da distorcao)
    template <typename Fn>
    void runModule (Module m, std::atomic<float>* onParam, juce::AudioBuffer<float>& buffer, int pos, int n, Fn&& fn,
                    float wetScale = 1.0f)
    {
        auto& mix = mixes[(size_t) m];
        mix.setTargetValue (onParam->load() > 0.5f ? 1.0f : 0.0f);
        if (! mix.isSmoothing() && mix.getCurrentValue() < 0.0001f)
            return;                                       // desligado: custo zero de CPU

        const int chans = juce::jmin (2, buffer.getNumChannels());
        for (int c = 0; c < 2; ++c)
            temp.copyFrom (c, 0, buffer, juce::jmin (c, chans - 1), pos, n);
        fn (temp, n);

        if (! mix.isSmoothing() && mix.getCurrentValue() > 0.9999f && wetScale > 0.999f)
        {
            for (int c = 0; c < chans; ++c)
                buffer.copyFrom (c, pos, temp, c, 0, n);
            return;
        }
        for (int i = 0; i < n; ++i)
        {
            const float a = mix.getNextValue() * wetScale;
            for (int c = 0; c < chans; ++c)
            {
                float* d = buffer.getWritePointer (c, pos);
                d[i] = d[i] * (1.0f - a) + temp.getSample (c, i) * a;
            }
        }
    }

    // ---------------------------------------------------------------- DISTORTION
    void distortion (juce::AudioBuffer<float>& b, int n)
    {
        const int type = juce::jlimit (0, 3, (int) params.distType->load());
        const float drive = params.distDrive->load();
        const float pre = 1.0f + drive * drive * 60.0f;
        const float comp = 1.0f / std::sqrt (pre);                  // compensa o ganho de entrada
        const float toneHz = 800.0f * std::pow (22.0f, params.distTone->load());   // 800 Hz .. ~17 kHz
        const float toneCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * toneHz / (float) sampleRate);

        juce::dsp::AudioBlock<float> blk (b.getArrayOfWritePointers(), 2, (size_t) n);
        auto up = oversampler.processSamplesUp (blk);
        for (size_t c = 0; c < up.getNumChannels(); ++c)
        {
            float* d = up.getChannelPointer (c);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
            {
                const float x = d[i] * pre;
                float y;
                switch (type)
                {
                    case 0:  y = std::tanh (x); break;                                        // Soft
                    case 1:  y = juce::jlimit (-1.0f, 1.0f, x); break;                        // Hard
                    case 2:  y = std::tanh (x + 0.3f) - std::tanh (0.3f);                      // Fuzz (assimetrico)
                             y = juce::jlimit (-1.0f, 1.0f, y * 1.4f); break;
                    default: y = std::tanh (std::abs (x) * 1.5f); break;   // Rectify (o DC gerado e removido abaixo)
                }
                d[i] = y;
            }
        }
        oversampler.processSamplesDown (blk);

        for (int c = 0; c < 2; ++c)
        {
            float* d = b.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                toneState[(size_t) c] += (d[i] - toneState[(size_t) c]) * toneCoef;   // TONE (passa-baixas)
                // bloqueador de DC (~7 Hz): a retificacao cria offset; sem isso o silencio viraria -1 constante
                const float xin = toneState[(size_t) c];
                dcOut[(size_t) c] = xin - dcIn[(size_t) c] + 0.999f * dcOut[(size_t) c];
                dcIn[(size_t) c] = xin;
                d[i] = dcOut[(size_t) c] * juce::jmax (comp, 0.35f);
            }
        }
    }

    // ---------------------------------------------------------------- EQ
    void equalizer (juce::AudioBuffer<float>& b, int n)
    {
        const std::array<float, 3> g { params.eqLow->load(), params.eqMid->load(), params.eqHigh->load() };
        if (g != lastEq)
        {
            lastEq = g;
            using AC = juce::dsp::IIR::ArrayCoefficients<float>;
            const auto sr = sampleRate;
            const auto low  = AC::makeLowShelf  (sr, 120.0f,  0.71f, juce::Decibels::decibelsToGain (g[0]));
            const auto mid  = AC::makePeakFilter (sr, 1000.0f, 0.8f,  juce::Decibels::decibelsToGain (g[1]));
            const auto high = AC::makeHighShelf (sr, 8000.0f, 0.71f, juce::Decibels::decibelsToGain (g[2]));
            for (int c = 0; c < 2; ++c)
            {
                *eq[(size_t) (c * 3 + 0)].coefficients = low;
                *eq[(size_t) (c * 3 + 1)].coefficients = mid;
                *eq[(size_t) (c * 3 + 2)].coefficients = high;
            }
        }
        for (int c = 0; c < 2; ++c)
        {
            float* d = b.getWritePointer (c);
            for (int i = 0; i < n; ++i)
                d[i] = eq[(size_t) (c * 3 + 2)].processSample (eq[(size_t) (c * 3 + 1)].processSample (eq[(size_t) (c * 3)].processSample (d[i])));
        }
    }

    // ---------------------------------------------------------------- CHORUS
    void chorusFx (juce::AudioBuffer<float>& b, int n)
    {
        chorus.setRate (params.chRate->load());
        chorus.setDepth (params.chDepth->load());
        chorus.setMix (params.chMix->load());
        juce::dsp::AudioBlock<float> blk (b.getArrayOfWritePointers(), 2, (size_t) n);
        chorus.process (juce::dsp::ProcessContextReplacing<float> (blk));
    }

    // ---------------------------------------------------------------- DELAY (sincronizado ao BPM)
    void delayFx (juce::AudioBuffer<float>& b, int n, double bpm)
    {
        static const double divBeats[] { 1.0, 0.5, 0.75, 0.25, 2.0 / 3.0, 1.0 / 3.0, 2.0 };
        const double beats = divBeats[juce::jlimit (0, 6, (int) params.dlDiv->load())];
        const double samples = juce::jlimit (16.0, (double) delayLen - 8.0, beats * 60.0 / juce::jlimit (20.0, 999.0, bpm) * sampleRate);
        delaySmoothed.setTargetValue ((float) samples);
        const float fb = juce::jlimit (0.0f, 0.95f, params.dlFb->load());
        const float mix = params.dlMix->load();
        const bool ping = params.dlPing->load() > 0.5f;
        const float damp = 0.35f;   // agudos das repeticoes vao escurecendo

        float* l = b.getWritePointer (0);
        float* r = b.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            const float dt = delaySmoothed.getNextValue();
            float rp = (float) writePos - dt;
            if (rp < 0.0f) rp += (float) delayLen;
            const int i0 = (int) rp, i1 = (i0 + 1) % delayLen;
            const float f = rp - (float) i0;
            float yl = delayBuf[0][(size_t) i0] + (delayBuf[0][(size_t) i1] - delayBuf[0][(size_t) i0]) * f;
            float yr = delayBuf[1][(size_t) i0] + (delayBuf[1][(size_t) i1] - delayBuf[1][(size_t) i0]) * f;
            dampState[0] += (yl - dampState[0]) * (1.0f - damp); yl = dampState[0];
            dampState[1] += (yr - dampState[1]) * (1.0f - damp); yr = dampState[1];

            const float inL = l[i], inR = r[i];
            if (ping)   // PING-PONG: entrada mono no L, repeticoes cruzam os lados
            {
                delayBuf[0][(size_t) writePos] = 0.5f * (inL + inR) + yr * fb;
                delayBuf[1][(size_t) writePos] = yl * fb;
            }
            else
            {
                delayBuf[0][(size_t) writePos] = inL + yl * fb;
                delayBuf[1][(size_t) writePos] = inR + yr * fb;
            }
            l[i] = inL + yl * mix;
            r[i] = inR + yr * mix;
            if (++writePos >= delayLen) writePos = 0;
        }
    }

    // ---------------------------------------------------------------- REVERB
    void reverbFx (juce::AudioBuffer<float>& b, int n)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = params.rvSize->load();
        rp.damping = params.rvDamp->load();
        rp.width = params.rvWidth->load();
        rp.wetLevel = params.rvMix->load();
        rp.dryLevel = 1.0f - 0.5f * params.rvMix->load();
        reverb.setParameters (rp);
        juce::dsp::AudioBlock<float> blk (b.getArrayOfWritePointers(), 2, (size_t) n);
        reverb.process (juce::dsp::ProcessContextReplacing<float> (blk));
    }

    // ---------------------------------------------------------------- COMPRESSOR
    void compressorFx (juce::AudioBuffer<float>& b, int n)
    {
        compressor.setThreshold (params.cpThresh->load());
        compressor.setRatio (params.cpRatio->load());
        compressor.setAttack (params.cpAttack->load());
        compressor.setRelease (params.cpRelease->load());
        juce::dsp::AudioBlock<float> blk (b.getArrayOfWritePointers(), 2, (size_t) n);
        compressor.process (juce::dsp::ProcessContextReplacing<float> (blk));
        makeup.setTargetValue (juce::Decibels::decibelsToGain (params.cpMakeup->load()));
        makeup.applyGain (b, n);
    }

    // ---------------------------------------------------------------- SATURATION (harmonicos suaves)
    void saturation (juce::AudioBuffer<float>& b, int n)
    {
        const int type = juce::jlimit (0, 3, (int) params.satType->load());   // Warm, Tape, Tube, Digital
        const float drive = 1.0f + params.satDrive->load() * 9.0f;
        const float norm = 1.0f / std::tanh (drive);                             // mantem o nivel de pico
        for (int c = 0; c < 2; ++c)
        {
            float* d = b.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                const float x = d[i] * drive;
                float y;
                switch (type)
                {
                    case 0:  y = std::tanh (x) * norm; break;                                          // Warm
                    case 1:  y = (x / (1.0f + std::abs (x))) * (1.0f + 1.0f / drive); break;           // Tape (curva macia)
                    case 2:  y = (std::tanh (x + 0.2f * x * x) - 0.2f * std::tanh (0.2f)) * norm; break; // Tube (pares)
                    default: y = std::round (std::tanh (x) * 24.0f) / 24.0f; break;                      // Digital (degraus)
                }
                // bloqueador de DC: a curva Tube (assimetrica) gera offset
                satDcOut[(size_t) c] = y - satDcIn[(size_t) c] + 0.999f * satDcOut[(size_t) c];
                satDcIn[(size_t) c] = y;
                d[i] = satDcOut[(size_t) c];
            }
        }
    }

    // ---------------------------------------------------------------- BITCRUSH (bits + reducao de sample rate)
    void bitcrush (juce::AudioBuffer<float>& b, int n)
    {
        const float levels = std::exp2 (juce::jlimit (2.0f, 16.0f, params.crBits->load())) * 0.5f;
        const int down = juce::jlimit (1, 64, (int) params.crDown->load());
        for (int i = 0; i < n; ++i)
        {
            if (crCounter == 0)
                for (int c = 0; c < 2; ++c)
                    crHold[(size_t) c] = std::round (b.getSample (c, i) * levels) / levels;
            if (++crCounter >= down) crCounter = 0;
            for (int c = 0; c < 2; ++c)
                b.setSample (c, i, crHold[(size_t) c]);
        }
    }

    // ---------------------------------------------------------------- PHASER
    void phaserFx (juce::AudioBuffer<float>& b, int n)
    {
        phaser.setRate (params.phRate->load());
        phaser.setDepth (params.phDepth->load());
        phaser.setFeedback (params.phFb->load());
        phaser.setCentreFrequency (900.0f);
        phaser.setMix (params.phMix->load());
        juce::dsp::AudioBlock<float> blk (b.getArrayOfWritePointers(), 2, (size_t) n);
        phaser.process (juce::dsp::ProcessContextReplacing<float> (blk));
    }

    // ---------------------------------------------------------------- FLANGER (atraso curto modulado + realimentacao)
    void flangerFx (juce::AudioBuffer<float>& b, int n)
    {
        const double inc = params.flRate->load() / sampleRate;
        const float depth = params.flDepth->load(), fb = juce::jlimit (-0.95f, 0.95f, params.flFb->load());
        for (int i = 0; i < n; ++i)
        {
            flPhase += inc; if (flPhase >= 1.0) flPhase -= 1.0;
            for (int c = 0; c < 2; ++c)
            {
                // estereo: canal R com LFO defasado 90 graus
                const double ph = flPhase + (c == 1 ? 0.25 : 0.0);
                const float lfo = 0.5f + 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * ph);
                const float delay = (float) sampleRate * (0.0008f + depth * 0.0065f * lfo);
                float rp = (float) flPos - delay; if (rp < 0.0f) rp += (float) flLen;
                const int i0 = (int) rp, i1 = (i0 + 1) % flLen; const float f = rp - (float) i0;
                auto& buf = flBuf[(size_t) c];
                const float y = buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * f;
                const float x = b.getSample (c, i);
                buf[(size_t) flPos] = x + y * fb;
                b.setSample (c, i, 0.5f * (x + y));
            }
            if (++flPos >= flLen) flPos = 0;
        }
    }

    // ---------------------------------------------------------------- GLITCH (sincronizado ao BPM / compasso)
    //  Stutter: repete a 1a fatia de cada grupo de 4 | Gate: corta ritmicamente
    //  Reverse: toca cada fatia ao contrario        | Tape Stop: a fatia desacelera ate parar
    void glitchFx (juce::AudioBuffer<float>& b, int n, int blockOffset)
    {
        static const double divBeats[] { 2.0, 1.0, 0.5, 0.25, 0.125, 0.0625 };   // 1/2 .. 1/64
        const int mode = juce::jlimit (0, 3, (int) params.glMode->load());
        const double beats = divBeats[juce::jlimit (0, 5, (int) params.glDiv->load())];
        const double sliceLen = juce::jlimit (32.0, (double) glLen / 5.0, beats * 60.0 / hostBpm * sampleRate);
        const double spb = 60.0 / hostBpm * sampleRate;           // amostras por tempo
        const double fade = juce::jmin (96.0, sliceLen * 0.1);

        for (int i = 0; i < n; ++i)
        {
            // posicao musical: PPQ do host (tocando) ou contador interno
            const double pos = hostPlaying ? (hostPpq * spb + (double) (blockOffset + i)) : (double) glSample;
            const double inSlice = std::fmod (pos, sliceLen);
            const double sliceIdx = std::floor (pos / sliceLen);
            for (int c = 0; c < 2; ++c) glBuf[(size_t) c][(size_t) glWrite] = b.getSample (c, i);

            auto readBack = [&] (double samplesAgo, int c)
            {
                double rp = (double) glWrite - samplesAgo;
                while (rp < 0.0) rp += glLen;
                const int i0 = (int) rp % glLen, i1 = (i0 + 1) % glLen; const float f = (float) (rp - std::floor (rp));
                return glBuf[(size_t) c][(size_t) i0] + (glBuf[(size_t) c][(size_t) i1] - glBuf[(size_t) c][(size_t) i0]) * f;
            };
            float env = 1.0f;
            if (inSlice < fade) env = (float) (inSlice / fade);
            else if (inSlice > sliceLen - fade) env = (float) ((sliceLen - inSlice) / fade);

            for (int c = 0; c < 2; ++c)
            {
                float y = b.getSample (c, i);
                switch (mode)
                {
                    case 0:   // STUTTER: dentro de cada grupo de 4 fatias, repete a primeira
                    {
                        const double group = std::fmod (sliceIdx, 4.0);
                        if (group > 0.5) y = readBack (group * sliceLen, c) * env;
                        break;
                    }
                    case 1:   // GATE: fatias alternadas em silencio (com rampas curtas)
                        y *= (std::fmod (sliceIdx, 2.0) < 0.5) ? env : 0.0f;
                        break;
                    case 2:   // REVERSE: fatia anterior de tras para frente
                        y = readBack (2.0 * inSlice + 1.0, c) * env;
                        break;
                    default:  // TAPE STOP: velocidade cai de 1 a 0 ao longo da fatia
                    {
                        const double t = inSlice / sliceLen;
                        const double consumed = sliceLen * (t - 0.5 * t * t);   // integral da velocidade (1 - t)
                        y = readBack (inSlice - consumed + 1.0, c) * env;
                        break;
                    }
                }
                b.setSample (c, i, y);
            }
            if (++glWrite >= glLen) glWrite = 0;
            ++glSample;
        }
    }

    const Params& params;
    double sampleRate = 44100.0;
    int blockSize = 512;
    juce::AudioBuffer<float> temp;
    std::array<juce::SmoothedValue<float>, NumModules> mixes;

    juce::dsp::Oversampling<float> oversampler { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true };
    std::array<float, 2> toneState {};
    std::array<float, 2> dcIn {}, dcOut {};

    std::array<juce::dsp::IIR::Filter<float>, 6> eq;
    std::array<float, 3> lastEq {};

    juce::dsp::Chorus<float> chorus;
    juce::dsp::Phaser<float> phaser;
    std::array<std::vector<float>, 2> flBuf;
    int flLen = 1, flPos = 0;
    double flPhase = 0.0;
    std::array<float, 2> crHold {};
    std::array<float, 2> satDcIn {}, satDcOut {};
    int crCounter = 0;
    std::array<std::vector<float>, 2> glBuf;
    int glLen = 1, glWrite = 0;
    long long glSample = 0;
    double glTapePos = 0.0;
    bool hostPlaying = false;
    double hostPpq = 0.0, hostBpm = 120.0;

    std::array<std::vector<float>, 2> delayBuf;
    int delayLen = 1, writePos = 0;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> delaySmoothed;
    std::array<float, 2> dampState {};

    juce::dsp::Reverb reverb;
    juce::dsp::Compressor<float> compressor;
    juce::SmoothedValue<float> makeup;
    juce::dsp::Limiter<float> limiter;
    static constexpr float ceiling = 0.9660f;   // -0.3 dBFS
};
} // namespace mm
