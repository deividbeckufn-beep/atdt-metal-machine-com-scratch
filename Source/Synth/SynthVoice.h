#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include "SynthParams.h"
#include "DSP/Oscillator.h"
#include "DSP/NoiseGenerator.h"
#include "DSP/StateVariableFilter.h"

namespace mm
{
class SynthSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

// =============================================================================
//  Voz:
//  OSC1-3 (unison ate 8) + SUB + NOISE -> DRIVE -> FILTRO 12/24 dB -> MIX -> AMP
//  Modulacao (matriz de 8 slots) calculada a cada 16 amostras (control rate),
//  ganhos suavizados por amostra -> sem zipper noise.
// =============================================================================
class SynthVoice : public juce::SynthesiserVoice
{
public:
    static constexpr int maxUnison = 8;

    SynthVoice (const SynthParams&, const PerformanceState&, const GlobalModState&);

    void prepare (double sampleRate, int maxBlockSize);

    bool canPlaySound (juce::SynthesiserSound*) override;
    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int currentPitchWheelPosition) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newPitchWheelValue) override;
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) override;
    using juce::SynthesiserVoice::renderNextBlock;

    // ---- modos MONO / LEGATO (chamados pelo SynthEngine) ----
    void monoNoteOn (int midiNoteNumber, float velocity, bool retrigger);
    int  getMonoNote() const noexcept      { return targetNote; }
    float getCurrentPitch() const noexcept { return currentPitch; }

private:
    void updateEnvelopeParameters();
    void resetDsp();
    void readBlockParameters();
    void controlTick (int sampleIndex);

    const SynthParams& params;
    const PerformanceState& perf;
    const GlobalModState& global;

    Oscillator osc[numOscillators][maxUnison];
    Oscillator subOsc;
    NoiseGenerator noise;
    StateVariableFilter filters[2][2]; // [estagio][canal]
    juce::ADSR ampEnv, filterEnv, modEnv;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoffSmoothed;
    juce::SmoothedValue<float> driveSmoothed, mixSmoothed;
    juce::Random random;

    double sampleRate = 44100.0;
    int   targetNote   = 60;
    float currentPitch = 60.0f;
    float velocity     = 1.0f;
    float velocityGain = 1.0f;
    float pitchBend    = 0.0f;
    float fenvValue    = 0.0f, aenvValue = 0.0f, menvValue = 0.0f;
    float noteRandom   = 0.0f;   // fonte "Random": valor aleatorio por nota (-1..1)
    double samplesSinceStart = 0.0;

    // parametros lidos uma vez por bloco
    struct Block
    {
        Oscillator::Wave wave[numOscillators] {};
        int   wt[numOscillators] {};
        float wtPos[numOscillators] {}, fmRatio[numOscillators] {}, fmAmt[numOscillators] {};
        float level[numOscillators] {}, pan[numOscillators] {}, pitchOffset[numOscillators] {};
        Oscillator::Wave subWave = Oscillator::Wave::Sine;
        float subLevel = 0.0f, subOctave = -1.0f, noiseLevel = 0.0f;
        NoiseGenerator::Type noiseType = NoiseGenerator::Type::White;
        FilterMode mode { 0, true };
        float resonance = 0.0f, envAmount = 0.0f, keyTrack = 0.0f;
        float bendSemis = 0.0f, glideTime = 0.0f, detune = 0.0f;
        int unison = 1;
    } bp;

    // valores modulados (control rate) e seus alvos suavizados
    float driveMod = 0.0f, mixMod = 0.0f;
    float oscL[numOscillators] {}, oscR[numOscillators] {}, oscTargetL[numOscillators] {}, oscTargetR[numOscillators] {};
    float subGain = 0.0f, subTarget = 0.0f, noiseGain = 0.0f, noiseTarget = 0.0f;
    float ampMod = 1.0f, ampModTarget = 1.0f;
    float uniL[maxUnison] {}, uniR[maxUnison] {};
    bool  snapGains = true;

    static constexpr int controlInterval = 16;
    int controlCounter = 0;
};
} // namespace mm
