#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DrumPattern.h"
#include "Parameters/Parameters.h"

namespace mm
{
// =============================================================================
//  DrumEngine - bateria 100% sintetizada (nenhum sample).
//  10 canais: KICK SNARE CLAP HAT OPEN-HAT PERC TOM CRASH RIDE FX
//  - toca o DrumPattern em 1/16 sincronizado ao REAPER (PPQ), com swing,
//    TIME FEEL (half/double) e humanize;
//  - DRUM KEYS: tocar pelo teclado MIDI (mapa GM: 36 kick, 38 snare, 42 hat...)
//  Sem alocacao no audio thread.
// =============================================================================
class DrumEngine
{
public:
    static constexpr int numRows = DrumRow::Count;

    struct Params
    {
        using P = std::atomic<float>*;
        P on{}, mode{}, length{}, swing{}, volume{}, keys{}, root{};
        P type808{}, drive808{}, glide808{};
        P level[numRows] {}, pan[numRows] {}, pitch[numRows] {}, decay[numRows] {};

        void attach (juce::AudioProcessorValueTreeState& s)
        {
            auto g = [&s] (const juce::String& id) { auto* p = s.getRawParameterValue (id); jassert (p != nullptr); return p; };
            on = g (ParamIDs::drumOn); mode = g (ParamIDs::drumMode); length = g (ParamIDs::drumLength);
            swing = g (ParamIDs::drumSwing); volume = g (ParamIDs::drumVolume); keys = g (ParamIDs::drumKeys);
            root = g (ParamIDs::keyRoot);
            type808 = g ("drum808_type"); drive808 = g ("drum808_drive"); glide808 = g ("drum808_glide");
            for (int r = 0; r < numRows; ++r)
            {
                level[r] = g (ParamIDs::drum (r, "level")); pan[r] = g (ParamIDs::drum (r, "pan"));
                pitch[r] = g (ParamIDs::drum (r, "pitch")); decay[r] = g (ParamIDs::drum (r, "decay"));
            }
        }
    };

    DrumEngine (const Params& p, const DrumPattern& pat) : params (p), pattern (pat) {}

    void prepare (double sr)
    {
        sampleRate = sr;
        for (auto& v : voices) v = Voice {};
        numTriggers = 0;
        numPending = 0;
        sampleClock = 0;
        lastStep = -1;
        internalBeat = 0.0;
        wasRunning = false;
        uiStep = -1;
        volumeSmoothed.reset (sr, 0.05);
        volumeSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.volume->load(), -59.9f));
    }

    // Tira do MIDI as notas de bateria (quando DRUM KEYS esta ligado) e agenda os golpes
    void handleMidi (juce::MidiBuffer& midi)
    {
        if (params.keys->load() < 0.5f || params.on->load() < 0.5f)
            return;
        juce::MidiBuffer keep;
        for (const auto meta : midi)
        {
            const auto msg = meta.getMessage();
            const int row = msg.isNoteOnOrOff() ? drumRowForNote (msg.getNoteNumber()) : -1;
            if (row < 0) { keep.addEvent (msg, meta.samplePosition); continue; }
            if (msg.isNoteOn())
                addTrigger (row, msg.getFloatVelocity(), meta.samplePosition);
        }
        midi.swapWith (keep);
    }

    // Soma a bateria em 'out'. Deve ser chamado uma vez por bloco.
    void process (juce::AudioBuffer<float>& out, int numSamples, double bpm, bool hostPlaying, double hostPpq,
                  double feelMult, float humVel, float humTime)
    {
        const bool on = params.on->load() > 0.5f;
        const bool always = (int) params.mode->load() == 1;
        const bool running = on && (hostPlaying || always);

        if (running)
            schedulePattern (numSamples, bpm, hostPlaying, hostPpq, feelMult, humVel, humTime);
        else
        {
            numPending = 0;
            uiStep = -1;
            lastStep = -1;
            internalBeat = 0.0;
        }
        wasRunning = running;

        // golpes agendados (ratchet/swing/humanize) que caem neste bloco
        for (int i = 0; i < numPending;)
        {
            const auto rel = pending[(size_t) i].absTime - sampleClock;
            if (rel < numSamples)
            {
                const auto& pd = pending[(size_t) i];
                addTrigger (pd.row, pd.vel, (int) juce::jmax ((juce::int64) 0, rel), pd.note, pd.pitch, pd.slide);
                pending[(size_t) i] = pending[(size_t) --numPending];
            }
            else ++i;
        }

        // ordena os golpes deste bloco (poucos: insercao)
        for (int i = 1; i < numTriggers; ++i)
            for (int j = i; j > 0 && triggers[(size_t) j].offset < triggers[(size_t) j - 1].offset; --j)
                std::swap (triggers[(size_t) j], triggers[(size_t) j - 1]);

        volumeSmoothed.setTargetValue (juce::Decibels::decibelsToGain (params.volume->load(), -59.9f));
        float* L = out.getWritePointer (0);
        float* R = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;

        // ganhos por canal (uma vez por bloco)
        std::array<float, numRows> gL {}, gR {};
        for (int r = 0; r < numRows; ++r)
        {
            const float angle = (params.pan[r]->load() + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            gL[(size_t) r] = params.level[r]->load() * std::cos (angle) * 1.4142f;
            gR[(size_t) r] = params.level[r]->load() * std::sin (angle) * 1.4142f;
        }

        int t = 0;
        bool anyActive = numTriggers > 0;
        for (auto& v : voices) anyActive = anyActive || v.active;
        if (! anyActive)
        {
            volumeSmoothed.skip (numSamples);
            sampleClock += numSamples;
            return;
        }

        for (int s = 0; s < numSamples; ++s)
        {
            while (t < numTriggers && triggers[(size_t) t].offset <= s)
            {
                const auto& tg = triggers[(size_t) t];
                start (tg.row, tg.vel, tg.note, tg.pitch, tg.slide);
                ++t;
            }
            float sl = 0.0f, sr = 0.0f;
            for (int r = 0; r < numRows; ++r)
            {
                auto& v = voices[(size_t) r];
                if (! v.active) continue;
                const float x = renderSample (r, v);
                sl += x * gL[(size_t) r] * (1.0f - v.panOffset);
                sr += x * gR[(size_t) r] * (1.0f + v.panOffset);
            }
            const float vol = volumeSmoothed.getNextValue() * 0.5f;
            if (R != nullptr) { L[s] += sl * vol; R[s] += sr * vol; }
            else              { L[s] += 0.5f * (sl + sr) * vol; }
        }
        numTriggers = 0;
        sampleClock += numSamples;
    }

    int getCurrentStep() const noexcept { return uiStep.load(); }

private:
    struct Voice
    {
        bool active = false;
        double t = 0.0;          // segundos desde o golpe
        float vel = 1.0f;
        double ph[6] {};         // fases (osciladores metalicos / tons)
        float hp1 = 0.0f, hp1x = 0.0f, hp2 = 0.0f, hp2x = 0.0f, lp = 0.0f, bp1 = 0.0f, bp2 = 0.0f;
        float choke = 1.0f;      // open hat abafado pelo hat fechado
        double freq = 0.0, targetFreq = 0.0;   // 808 (glide)
        float panOffset = 0.0f;
        bool choking = false;
        float pitchMul = 1.0f, decayMul = 1.0f;
    };
    struct Trigger { int row = 0; float vel = 1.0f; int offset = 0; int note = 0; float pitch = 0.0f; bool slide = false; };
    struct Pending { int row = 0; float vel = 1.0f; juce::int64 absTime = 0; int note = 0; float pitch = 0.0f; bool slide = false; };

    void addTrigger (int row, float vel, int offset, int note = 0, float pitch = 0.0f, bool slide = false)
    {
        if (numTriggers < (int) triggers.size())
            triggers[(size_t) numTriggers++] = { row, juce::jlimit (0.05f, 1.0f, vel), juce::jmax (0, offset), note, pitch, slide };
    }

    // golpe em tempo absoluto: se cair depois deste bloco, fica agendado
    void schedule (int row, float vel, juce::int64 offsetInBlock, int numSamples, int note, float pitch = 0.0f, bool slide = false)
    {
        if (offsetInBlock < numSamples) addTrigger (row, vel, (int) offsetInBlock, note, pitch, slide);
        else if (numPending < (int) pending.size())
            pending[(size_t) numPending++] = { row, juce::jlimit (0.05f, 1.0f, vel), sampleClock + offsetInBlock, note, pitch, slide };
    }

    void schedulePattern (int numSamples, double bpm, bool hostPlaying, double hostPpq, double feelMult, float humVel, float humTime)
    {
        bpm = juce::jlimit (20.0, 999.0, bpm);
        const double stepBeats = 0.25 / juce::jmax (0.25, feelMult);
        const double bps = bpm / 60.0 / sampleRate;
        const double stepSamples = stepBeats / bps;
        const int length = juce::jlimit (1, DrumPattern::maxSteps, (int) params.length->load());
        const double startBeat = hostPlaying ? hostPpq : internalBeat;
        const double endBeat = startBeat + numSamples * bps;
        const float swing = params.swing->load();

        // primeiro passo a disparar: o proximo limite de passo (ou o atual, se acabou de comecar)
        long long k = (long long) std::ceil (startBeat / stepBeats - 1.0e-9);
        if (! wasRunning && std::abs (startBeat - (double) k * stepBeats) > 1.0e-9 && hostPlaying)
            k = (long long) std::ceil (startBeat / stepBeats);

        for (; (double) k * stepBeats < endBeat; ++k)
        {
            if (k == lastStep)
                continue;
            lastStep = k;
            int offset = (int) std::llround (((double) k * stepBeats - startBeat) / bps);
            const int step = (int) (((k % length) + length) % length);
            uiStep = step;
            if ((k & 1) != 0) offset += (int) (swing * stepSamples / 3.0);                   // SWING
            if (humTime > 0.0f) offset += (int) (random.nextFloat() * humTime * 0.25 * stepSamples);   // HUMANIZE timing
            for (int r = 0; r < numRows; ++r)
            {
                const uint8_t v = pattern.cells[(size_t) r][(size_t) step].load();
                if (v == 0) continue;
                float vel = (float) v / 127.0f;
                if (humVel > 0.0f)
                    vel *= 1.0f + (random.nextFloat() * 2.0f - 1.0f) * humVel * 0.5f;
                // RATCHET: N golpes igualmente espacados dentro do passo (roll), com leve queda de intensidade
                const int rt = juce::jlimit (1, 4, (int) pattern.ratchet[(size_t) r][(size_t) step].load());
                const int note = r == DrumRow::Bass808 ? (int) pattern.bassNote[(size_t) step].load() : 0;
                const bool slide = r == DrumRow::Bass808 && pattern.slide808[(size_t) step].load() != 0;
                const int dir = r == DrumRow::ClosedHat ? (int) pattern.hatRollPitch[(size_t) step].load() : 0;
                for (int j = 0; j < rt; ++j)   // roll: cada sub-golpe sobe/desce 1,5 semitom
                    schedule (r, vel * (1.0f - 0.1f * (float) j), (juce::int64) offset + (juce::int64) (j * stepSamples / rt),
                              numSamples, note, (float) (dir * j) * 1.5f, slide);
            }
        }
        if (! hostPlaying)
            internalBeat = endBeat;
    }

    void start (int row, float vel, int note = 0, float pitchSemis = 0.0f, bool slideStep = false)
    {
        auto& v = voices[(size_t) row];
        const bool slide = row == DrumRow::Bass808 && v.active && slideStep;
        const double prevFreq = v.freq;
        const double prevPhase = v.ph[0];
        v = Voice {};
        if (row == DrumRow::Bass808)
        {
            // 808 afinado na KEY: tonica na oitava 1 (24 + root) + nota do passo + PITCH do canal
            const double midi = 24.0 + params.root->load() + note + params.pitch[row]->load();
            v.targetFreq = 440.0 * std::pow (2.0, (midi - 69.0) / 12.0);
            v.freq = slide ? prevFreq : v.targetFreq;          // SLIDE: parte da nota anterior
            v.ph[0] = slide ? prevPhase : 0.0;                 // sem reiniciar a fase: sem click
        }
        v.active = true;
        v.vel = 0.25f + 0.75f * vel * vel;
        v.pitchMul = std::exp2 ((params.pitch[row]->load() + pitchSemis) / 12.0f);
        // microvariacao de pan (hats/perc): movimento estereo discreto, nunca exagerado
        if (row == DrumRow::ClosedHat || row == DrumRow::OpenHat || row == DrumRow::Perc)
            v.panOffset = (random.nextFloat() - 0.5f) * 0.24f;
        v.decayMul = params.decay[row]->load();
        if (row == DrumRow::ClosedHat && voices[(size_t) DrumRow::OpenHat].active)
            voices[(size_t) DrumRow::OpenHat].choking = true;   // hat fechado abafa o aberto
    }

    float noise() noexcept
    {
        seed = seed * 1664525u + 1013904223u;
        return (float) (int32_t) seed * (1.0f / 2147483648.0f);
    }

    static float hp (float x, float& y, float& xPrev, float a) noexcept { y = a * (y + x - xPrev); xPrev = x; return y; }

    float metal (Voice& v, float mul) noexcept
    {
        static const double f[6] { 205.3, 304.4, 369.6, 522.7, 540.0, 800.0 };   // razoes inarmonicas estilo 808
        float sum = 0.0f;
        for (int i = 0; i < 6; ++i)
        {
            v.ph[i] += f[i] * mul * v.pitchMul / sampleRate;
            v.ph[i] -= std::floor (v.ph[i]);
            sum += v.ph[i] < 0.5 ? 1.0f : -1.0f;
        }
        return sum / 6.0f;
    }

    float renderSample (int row, Voice& v) noexcept
    {
        const float t = (float) v.t;
        const float d = v.decayMul;
        const float pm = v.pitchMul;
        constexpr float twoPi = juce::MathConstants<float>::twoPi;
        float out = 0.0f, env = 0.0f;

        switch (row)
        {
            case DrumRow::Kick:
            {
                const double f = 45.0 * pm + (170.0 * pm - 45.0 * pm) * std::exp (-t / 0.035);
                v.ph[0] += f / sampleRate; v.ph[0] -= std::floor (v.ph[0]);
                env = std::exp (-t / (0.35f * d));
                const float click = noise() * std::exp (-t / 0.003f) * 0.6f;
                out = std::tanh (1.6f * ((float) std::sin (twoPi * v.ph[0]) * env + click));
                break;
            }
            case DrumRow::Snare:
            {
                v.ph[0] += 185.0 * pm / sampleRate; v.ph[1] += 330.0 * pm / sampleRate;
                const float tone = ((float) std::sin (twoPi * v.ph[0]) + 0.6f * (float) std::sin (twoPi * v.ph[1])) * std::exp (-t / (0.06f * d));
                env = std::exp (-t / (0.18f * d));
                const float n = hp (noise(), v.hp1, v.hp1x, 0.82f) * env;
                out = 0.55f * tone + 0.9f * n;
                break;
            }
            case DrumRow::Clap:
            {
                float e = 0.0f;
                for (int b = 0; b < 3; ++b)                                     // 3 rajadas rapidas
                    if (t >= b * 0.011f) e = juce::jmax (e, std::exp (-(t - b * 0.011f) / 0.006f));
                if (t >= 0.03f) e = juce::jmax (e, 0.7f * std::exp (-(t - 0.03f) / (0.16f * d)));
                env = e;
                // passa-banda simples ~1.2 kHz
                const float x = noise();
                const float fc = 0.15f * pm, q = 0.35f;
                v.bp1 += fc * (x - v.bp1 - q * v.bp2); v.bp2 += fc * v.bp1;
                out = v.bp1 * e * 2.2f;
                break;
            }
            case DrumRow::ClosedHat:
            case DrumRow::OpenHat:
            {
                const float tau = row == DrumRow::ClosedHat ? 0.045f : 0.35f;
                env = std::exp (-t / (tau * d));
                const float x = 0.55f * metal (v, 1.0f) + 0.45f * noise();
                out = hp (hp (x, v.hp1, v.hp1x, 0.55f), v.hp2, v.hp2x, 0.55f) * env * 1.4f;
                break;
            }
            case DrumRow::Perc:   // cowbell/rim digital
            {
                v.ph[0] += 560.0 * pm / sampleRate; v.ph[1] += 845.0 * pm / sampleRate;
                v.ph[0] -= std::floor (v.ph[0]); v.ph[1] -= std::floor (v.ph[1]);
                env = std::exp (-t / (0.08f * d));
                out = ((v.ph[0] < 0.5 ? 0.5f : -0.5f) + (v.ph[1] < 0.5 ? 0.5f : -0.5f)) * env * 0.8f;
                break;
            }
            case DrumRow::Tom:
            {
                const double f = 105.0 * pm + 80.0 * pm * std::exp (-t / 0.05);
                v.ph[0] += f / sampleRate; v.ph[0] -= std::floor (v.ph[0]);
                env = std::exp (-t / (0.4f * d));
                out = std::tanh (1.3f * (float) std::sin (twoPi * v.ph[0])) * env;
                break;
            }
            case DrumRow::Crash:
            {
                const float attack = juce::jmin (1.0f, t / 0.002f);
                env = std::exp (-t / (1.3f * d)) * attack;
                const float x = 0.5f * metal (v, 1.35f) + 0.8f * noise();
                out = hp (hp (x, v.hp1, v.hp1x, 0.6f), v.hp2, v.hp2x, 0.6f) * env * 1.1f;
                break;
            }
            case DrumRow::Ride:
            {
                env = std::exp (-t / (0.9f * d));
                v.ph[5] += 1250.0 * pm / sampleRate; v.ph[5] -= std::floor (v.ph[5]);
                const float bell = (float) std::sin (twoPi * v.ph[5]) * std::exp (-t / 0.4f);
                const float x = hp (0.7f * metal (v, 1.7f) + 0.15f * noise(), v.hp1, v.hp1x, 0.65f);
                out = (x * 0.8f + bell * 0.35f) * env;
                break;
            }
            case DrumRow::Bass808:
            {
                // tipos: 0 Clean 1 Sub 2 Distorted 3 Digital 4 Metal 5 Reese 6 Short 7 Long
                const int type = juce::jlimit (0, 7, (int) params.type808->load());
                const float drive = params.drive808->load();
                const double glideSec = juce::jlimit (0.01, 0.5, (double) params.glide808->load());
                v.freq += (v.targetFreq - v.freq) * (1.0 - std::exp (-1.0 / (glideSec * sampleRate)));
                const double boom = type == 1 ? 0.0 : 0.6;                     // SUB: sem "boom" no ataque
                const double f = v.freq * (1.0 + boom * std::exp (-t / 0.012));
                v.ph[0] += f / sampleRate; v.ph[0] -= std::floor (v.ph[0]);
                const float decayMul = type == 6 ? 0.3f : type == 7 ? 2.2f : 1.0f;
                env = std::exp (-t / (1.1f * d * decayMul)) * juce::jmin (1.0f, t / 0.0015f);
                const float s1 = (float) std::sin (twoPi * v.ph[0]);
                float x = s1;
                if (type == 4) x = s1 + 0.35f * (float) std::sin (2.0 * twoPi * v.ph[0]) + 0.2f * (float) std::sin (3.0 * twoPi * v.ph[0]);   // METAL: harmonicos
                if (type == 5)                                                   // REESE: duas fases desafinadas
                {
                    v.ph[1] += f * 1.007 / sampleRate; v.ph[1] -= std::floor (v.ph[1]);
                    x = 0.6f * s1 + 0.6f * (float) (2.0 * v.ph[1] - 1.0);
                }
                float gain = 1.8f + drive * 6.0f;
                if (type == 1) gain = 1.0f + drive * 1.5f;                       // SUB limpo
                if (type == 2) gain = 4.5f + drive * 8.0f;                       // DISTORTED
                x = std::tanh (gain * x) / std::tanh (juce::jmin (gain, 3.0f));
                if (type == 3) x = std::round (x * 7.0f) / 7.0f;                 // DIGITAL: degraus
                out = juce::jlimit (-1.0f, 1.0f, x) * env * 0.9f;
                break;
            }
            default:   // FX: "zap" descendente digital
            {
                const double f = 60.0 + 3000.0 * pm * std::exp (-t / 0.12);
                v.ph[0] += f / sampleRate; v.ph[0] -= std::floor (v.ph[0]);
                env = std::exp (-t / (0.5f * d));
                out = (std::tanh (3.0f * (float) std::sin (twoPi * v.ph[0])) * 0.7f + noise() * 0.15f) * env;
                break;
            }
        }

        if (v.choking)
        {
            v.choke *= 0.995f;   // ~5 ms
            if (v.choke < 0.001f) v.active = false;
        }
        v.t += 1.0 / sampleRate;
        if (env < 0.0003f && v.t > 0.01)
            v.active = false;
        return out * v.vel * v.choke;
    }

    const Params& params;
    const DrumPattern& pattern;
    double sampleRate = 44100.0;
    std::array<Voice, numRows> voices {};
    std::array<Trigger, 256> triggers {};
    int numTriggers = 0;
    std::array<Pending, 512> pending {};
    int numPending = 0;
    juce::int64 sampleClock = 0;
    long long lastStep = -1;
    double internalBeat = 0.0;
    bool wasRunning = false;
    std::atomic<int> uiStep { -1 };
    juce::SmoothedValue<float> volumeSmoothed;
    juce::Random random;
    uint32_t seed = 0x12345678u;
};
} // namespace mm
