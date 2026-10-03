#include "SynthVoice.h"

namespace mm
{
namespace
{
    inline double noteToHz (double semis) noexcept { return 440.0 * std::exp2 ((semis - 69.0) / 12.0); }
    constexpr float gainSmoothing = 0.02f;   // ~1 ms a 48 kHz
}

SynthVoice::SynthVoice (const SynthParams& p, const PerformanceState& ps, const GlobalModState& g)
    : params (p), perf (ps), global (g) {}

void SynthVoice::prepare (double sr, int)
{
    sampleRate = sr;
    for (auto& row : osc)
        for (auto& o : row)
            o.prepare (sr);
    subOsc.prepare (sr);
    for (auto& stage : filters)
        for (auto& f : stage)
            f.prepare (sr);
    ampEnv.setSampleRate (sr);
    filterEnv.setSampleRate (sr);
    modEnv.setSampleRate (sr);
    cutoffSmoothed.reset (sr, 0.02);
    driveSmoothed.reset (sr, 0.02);
    mixSmoothed.reset (sr, 0.02);
    cutoffSmoothed.setCurrentAndTargetValue (params.cutoff->load());
    driveSmoothed.setCurrentAndTargetValue (params.drive->load());
    mixSmoothed.setCurrentAndTargetValue (params.mix->load());
}

bool SynthVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<SynthSound*> (sound) != nullptr;
}

void SynthVoice::updateEnvelopeParameters()
{
    ampEnv.setParameters ({ params.aA->load(), params.aD->load(), params.aS->load(), params.aR->load() });
    filterEnv.setParameters ({ params.fA->load(), params.fD->load(), params.fS->load(), params.fR->load() });
    modEnv.setParameters ({ params.mA->load(), params.mD->load(), params.mS->load(), params.mR->load() });
}

void SynthVoice::resetDsp()
{
    ampEnv.reset();
    filterEnv.reset();
    modEnv.reset();
    for (auto& stage : filters)
        for (auto& f : stage)
            f.reset();
    noise.reset();
}

void SynthVoice::startNote (int midiNoteNumber, float vel, juce::SynthesiserSound*, int pitchWheelPos)
{
    targetNote   = midiNoteNumber;
    velocity     = vel;
    velocityGain = juce::jmap (vel, 0.3f, 1.0f);
    pitchBend    = (float) (pitchWheelPos - 8192) / 8192.0f;

    const float from = global.glideFromNote;
    currentPitch = (params.glide->load() > 0.0005f && from >= 0.0f) ? from : (float) midiNoteNumber;

    // fases: voz principal usa PHASE; vozes de unison comecam em fases aleatorias (som mais largo)
    const int unison = juce::jlimit (1, maxUnison, (int) params.unisonVoices->load());
    for (int i = 0; i < numOscillators; ++i)
    {
        const double basePhase = params.osc[i].phase->load();
        for (int j = 0; j < maxUnison; ++j)
            osc[i][j].reset (j == 0 || unison == 1 ? basePhase : random.nextDouble());
    }
    subOsc.reset (0.0);

    for (auto& stage : filters)
        for (auto& f : stage)
            f.reset();

    cutoffSmoothed.setCurrentAndTargetValue (params.cutoff->load());
    driveSmoothed.setCurrentAndTargetValue (params.drive->load());
    mixSmoothed.setCurrentAndTargetValue (params.mix->load());

    fenvValue = aenvValue = 0.0f;
    samplesSinceStart = 0.0;
    controlCounter = 0;
    snapGains = true;

    updateEnvelopeParameters();
    ampEnv.noteOn();
    filterEnv.noteOn();
    modEnv.noteOn();
    noteRandom = random.nextFloat() * 2.0f - 1.0f;
}

void SynthVoice::monoNoteOn (int midiNoteNumber, float vel, bool retrigger)
{
    targetNote = midiNoteNumber;
    if (params.glide->load() <= 0.0005f)
        currentPitch = (float) midiNoteNumber;

    if (retrigger)
    {
        velocity     = vel;
        velocityGain = juce::jmap (vel, 0.3f, 1.0f);
        samplesSinceStart = 0.0;
        updateEnvelopeParameters();
        ampEnv.noteOn();     // o ADSR do JUCE continua do valor atual: sem click
        filterEnv.noteOn();
        modEnv.noteOn();
        noteRandom = random.nextFloat() * 2.0f - 1.0f;
    }
}

void SynthVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        ampEnv.noteOff();
        filterEnv.noteOff();
        modEnv.noteOff();
    }
    else
    {
        clearCurrentNote();
        resetDsp();
    }
}

void SynthVoice::pitchWheelMoved (int newValue)
{
    pitchBend = (float) (newValue - 8192) / 8192.0f;
}

void SynthVoice::readBlockParameters()
{
    for (int i = 0; i < numOscillators; ++i)
    {
        const auto& op = params.osc[i];
        bp.wave[i]  = (Oscillator::Wave) juce::jlimit (0, 7, (int) op.wave->load());
        bp.wt[i]      = (int) op.wt->load();
        bp.wtPos[i]   = op.wtpos->load();
        bp.fmRatio[i] = fmRatios[juce::jlimit (0, 7, (int) op.fmratio->load())];
        bp.fmAmt[i]   = op.fmamt->load();
        bp.level[i] = op.level->load();
        bp.pan[i]   = op.pan->load();
        bp.pitchOffset[i] = op.octave->load() * 12.0f + op.semi->load() + op.fine->load() * 0.01f;
    }

    static constexpr Oscillator::Wave subWaves[] { Oscillator::Wave::Sine, Oscillator::Wave::Triangle, Oscillator::Wave::Square };
    bp.subWave    = subWaves[juce::jlimit (0, 2, (int) params.subWave->load())];
    bp.subLevel   = params.subLevel->load();
    bp.subOctave  = params.subOctave->load();
    bp.noiseType  = (NoiseGenerator::Type) juce::jlimit (0, 2, (int) params.noiseType->load());
    bp.noiseLevel = params.noiseLevel->load();

    bp.mode      = FilterMode::fromChoice ((int) params.filterType->load());
    bp.resonance = params.resonance->load();
    bp.envAmount = params.envAmount->load();
    bp.keyTrack  = params.keyTrack->load();
    bp.bendSemis = pitchBend * params.bendRange->load();
    bp.glideTime = params.glide->load();
    bp.detune    = params.unisonDetune->load();
    bp.unison    = juce::jlimit (1, maxUnison, (int) params.unisonVoices->load());

    // ganhos de pan do unison (lei de potencia constante, normalizado pelo numero de vozes)
    const float spread = params.unisonSpread->load();
    const float norm = 1.0f / std::sqrt ((float) bp.unison);
    for (int j = 0; j < bp.unison; ++j)
    {
        const float pos = bp.unison > 1 ? spread * (2.0f * (float) j / (float) (bp.unison - 1) - 1.0f) : 0.0f;
        const float angle = (pos + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        uniL[j] = std::cos (angle) * juce::MathConstants<float>::sqrt2 * norm;
        uniR[j] = std::sin (angle) * juce::MathConstants<float>::sqrt2 * norm;
    }
}

// ---------------------------------------------------------------------------
// Matriz de modulacao + frequencias + filtro (a cada 16 amostras)
// ---------------------------------------------------------------------------
void SynthVoice::controlTick (int s)
{
    // ---- fontes ----
    float src[(int) ModSource::Count] {};
    src[(int) ModSource::FilterEnv]  = fenvValue;
    src[(int) ModSource::AmpEnv]     = aenvValue;
    src[(int) ModSource::Velocity]   = velocity;
    src[(int) ModSource::ModWheel]   = perf.modWheel;
    src[(int) ModSource::Aftertouch] = perf.aftertouch;
    src[(int) ModSource::Note]       = (float) (targetNote - 60) / 60.0f;
    src[(int) ModSource::Random]     = noteRandom;
    src[(int) ModSource::ModEnv]     = menvValue;
    for (int m = 0; m < numMacros; ++m)
        src[(int) ModSource::Macro1 + m] = params.macro[m]->load();

    // ---- passada 1: quem controla a profundidade dos LFOs (ex.: Mod Wheel -> LFO1 Amount) ----
    float lfoDepth[numLfos];
    for (int k = 0; k < numLfos; ++k)
        lfoDepth[k] = params.lfo[k].amount->load();

    auto slotValue = [&] (int slotIndex, int srcIndex)
    {
        const auto& sl = params.slot[slotIndex];
        float v = src[srcIndex];
        if ((int) sl.mode->load() == 1)
            v = isBipolarSource (srcIndex) ? -v : 1.0f - v;
        return applyModCurve (v, (int) sl.curve->load()) * sl.amt->load();
    };

    for (int i = 0; i < numModSlots; ++i)
    {
        const int si = (int) params.slot[i].src->load(), di = (int) params.slot[i].dst->load();
        if (di >= (int) ModDest::Lfo1Amount && di <= (int) ModDest::Lfo4Amount
            && si > 0 && ! (si >= (int) ModSource::Lfo1 && si <= (int) ModSource::Lfo4))
            lfoDepth[di - (int) ModDest::Lfo1Amount] += slotValue (i, si);
    }

    const double t = samplesSinceStart / sampleRate;
    for (int k = 0; k < numLfos; ++k)
    {
        const float fade = params.lfo[k].fade->load();
        const float fadeGain = fade > 0.0005f ? (float) juce::jmin (1.0, t / fade) : 1.0f;
        const float raw = global.lfo[k] != nullptr ? global.lfo[k][s - global.chunkStart] : 0.0f;
        src[(int) ModSource::Lfo1 + k] = raw * juce::jlimit (0.0f, 1.0f, lfoDepth[k]) * fadeGain;
    }

    // ---- passada 2: todos os destinos ----
    float acc[(int) ModDest::Count] {};
    for (int i = 0; i < numModSlots; ++i)
    {
        const int si = (int) params.slot[i].src->load(), di = (int) params.slot[i].dst->load();
        if (si <= 0 || di <= 0 || si >= (int) ModSource::Count || di >= (int) ModDest::Count)
            continue;
        if (isLfoAmountDest (di))   // ja aplicado na passada 1
            continue;
        acc[di] += slotValue (i, si);
    }

    // ---- glide ----
    if (bp.glideTime > 0.0005f)
    {
        const float coef = 1.0f - std::exp (-(float) controlInterval / (bp.glideTime * (float) sampleRate));
        currentPitch += ((float) targetNote - currentPitch) * coef;
    }
    else
    {
        currentPitch = (float) targetNote;
    }

    // ---- frequencias (unison com detune) ----
    const float pitchAll = currentPitch + bp.bendSemis + acc[(int) ModDest::PitchAll] * ModScale::pitchSemitones;
    const float detuneCents = juce::jlimit (0.0f, 1.0f, bp.detune + acc[(int) ModDest::UnisonDetune]) * 50.0f;

    for (int i = 0; i < numOscillators; ++i)
    {
        const float base = pitchAll + bp.pitchOffset[i] + acc[(int) ModDest::Osc1Pitch + i] * ModScale::pitchSemitones;
        for (int j = 0; j < bp.unison; ++j)
        {
            const float spreadPos = bp.unison > 1 ? 2.0f * (float) j / (float) (bp.unison - 1) - 1.0f : 0.0f;
            osc[i][j].setFrequency (noteToHz (base + spreadPos * detuneCents * 0.01f));
        }

        // wavetable e FM (modulaveis pela matriz)
        const float wtPos = juce::jlimit (0.0f, 1.0f, bp.wtPos[i] + acc[(int) ModDest::Osc1WtPos + i]);
        const float fmAmt = juce::jlimit (0.0f, 1.0f, bp.fmAmt[i] + acc[(int) ModDest::Osc1FmAmount + i]);
        for (int j = 0; j < bp.unison; ++j)
        {
            osc[i][j].setWavetable (bp.wt[i], wtPos);
            osc[i][j].setFM (bp.fmRatio[i], fmAmt);
        }
        const float level = juce::jlimit (0.0f, 1.0f, bp.level[i] + acc[(int) ModDest::Osc1Level + i]);
        const float pan   = juce::jlimit (-1.0f, 1.0f, bp.pan[i] + acc[(int) ModDest::Pan]);
        const float angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        oscTargetL[i] = level * std::cos (angle);
        oscTargetR[i] = level * std::sin (angle);
    }

    subOsc.setFrequency (noteToHz (pitchAll + bp.subOctave * 12.0f));
    subTarget   = juce::jlimit (0.0f, 1.0f, bp.subLevel + acc[(int) ModDest::SubLevel]) * 0.7071f;
    noiseTarget = juce::jlimit (0.0f, 1.0f, bp.noiseLevel + acc[(int) ModDest::NoiseLevel]) * 0.7071f;
    ampModTarget = juce::jlimit (0.0f, 2.0f, 1.0f + acc[(int) ModDest::Amp]);
    driveMod = acc[(int) ModDest::Drive];
    mixMod   = acc[(int) ModDest::FilterMix];

    // ---- filtro ----
    const float keyOct = bp.keyTrack * (currentPitch - 60.0f) / 12.0f;
    const float cutoff = cutoffSmoothed.getCurrentValue()
                         * std::exp2 (bp.envAmount * fenvValue * 7.0f + keyOct + acc[(int) ModDest::Cutoff] * ModScale::cutoffOctaves);
    const float reso = juce::jlimit (0.0f, 1.0f, bp.resonance + acc[(int) ModDest::Resonance]);
    const float firstRes = bp.mode.twoStages ? reso * 0.5f : reso;
    for (auto& f : filters[0]) f.setParameters (cutoff, firstRes);
    if (bp.mode.twoStages)
        for (auto& f : filters[1]) f.setParameters (cutoff, reso);

    if (snapGains)
    {
        snapGains = false;
        for (int i = 0; i < numOscillators; ++i) { oscL[i] = oscTargetL[i]; oscR[i] = oscTargetR[i]; }
        subGain = subTarget; noiseGain = noiseTarget; ampMod = ampModTarget;
    }
}

void SynthVoice::renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples)
{
    if (! isVoiceActive())
        return;

    updateEnvelopeParameters();
    readBlockParameters();

    cutoffSmoothed.setTargetValue (params.cutoff->load());
    driveSmoothed.setTargetValue (params.drive->load());
    mixSmoothed.setTargetValue (params.mix->load());

    const auto svfType = (StateVariableFilter::Type) bp.mode.baseType;
    constexpr float headroom = 0.3f;
    float* left  = output.getWritePointer (0);
    float* right = output.getNumChannels() > 1 ? output.getWritePointer (1) : nullptr;

    for (int s = startSample; s < startSample + numSamples; ++s)
    {
        fenvValue = filterEnv.getNextSample();
        menvValue = modEnv.getNextSample();
        cutoffSmoothed.getNextValue();

        if (--controlCounter <= 0)
        {
            controlCounter = controlInterval;
            controlTick (s);
        }

        // suavizacao dos ganhos modulados
        for (int i = 0; i < numOscillators; ++i)
        {
            oscL[i] += (oscTargetL[i] - oscL[i]) * gainSmoothing;
            oscR[i] += (oscTargetR[i] - oscR[i]) * gainSmoothing;
        }
        subGain   += (subTarget - subGain) * gainSmoothing;
        noiseGain += (noiseTarget - noiseGain) * gainSmoothing;
        ampMod    += (ampModTarget - ampMod) * gainSmoothing;

        float sumL = 0.0f, sumR = 0.0f;
        for (int i = 0; i < numOscillators; ++i)
        {
            if (oscL[i] + oscR[i] < 0.00005f && oscTargetL[i] + oscTargetR[i] < 0.00005f)
                continue;

            float accL = 0.0f, accR = 0.0f;
            for (int j = 0; j < bp.unison; ++j)
            {
                const float v = osc[i][j].process (bp.wave[i]);
                accL += v * uniL[j];
                accR += v * uniR[j];
            }
            sumL += accL * oscL[i];
            sumR += accR * oscR[i];
        }

        if (subGain > 0.00005f || subTarget > 0.00005f)
        {
            const float v = subOsc.process (bp.subWave) * subGain;
            sumL += v; sumR += v;
        }
        if (noiseGain > 0.00005f || noiseTarget > 0.00005f)
        {
            const float v = noise.process (bp.noiseType) * noiseGain;
            sumL += v; sumR += v;
        }

        const float drive = juce::jlimit (0.0f, 1.0f, driveSmoothed.getNextValue() + driveMod);
        const float mix   = juce::jlimit (0.0f, 1.0f, mixSmoothed.getNextValue() + mixMod);

        if (drive > 0.001f)
        {
            const float g = driveGain (drive), c = driveCompensation (drive);
            sumL = std::tanh (sumL * g) * c;
            sumR = std::tanh (sumR * g) * c;
        }

        float fL = filters[0][0].process (sumL, svfType);
        float fR = filters[0][1].process (sumR, svfType);
        if (bp.mode.twoStages)
        {
            fL = filters[1][0].process (fL, svfType);
            fR = filters[1][1].process (fR, svfType);
        }

        aenvValue = ampEnv.getNextSample();
        const float amp = aenvValue * velocityGain * headroom * ampMod;
        const float outL = (mix * fL + (1.0f - mix) * sumL) * amp;
        const float outR = (mix * fR + (1.0f - mix) * sumR) * amp;

        if (right != nullptr) { left[s] += outL; right[s] += outR; }
        else                  { left[s] += 0.5f * (outL + outR); }

        samplesSinceStart += 1.0;

        if (! ampEnv.isActive())
        {
            clearCurrentNote();
            resetDsp();
            break;
        }
    }
}
} // namespace mm
