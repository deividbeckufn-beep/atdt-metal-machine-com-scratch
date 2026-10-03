#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// =============================================================================
//  Parametros do ATDT SYNTH MACHINE (todos automatizaveis no REAPER).
//  Regra: nunca renomear um ID ja publicado. Apenas ADICIONAR novos.
// =============================================================================
namespace mm
{
namespace ParamIDs
{
    inline juce::String osc (int index, const char* name) { return "osc" + juce::String (index) + "_" + name; }

    inline constexpr const char* subWave      = "sub_wave";
    inline constexpr const char* subOctave    = "sub_octave";
    inline constexpr const char* subLevel     = "sub_level";

    inline constexpr const char* noiseType    = "noise_type";
    inline constexpr const char* noiseLevel   = "noise_level";

    inline constexpr const char* filterType   = "filter_type";
    inline constexpr const char* cutoff       = "filter_cutoff";
    inline constexpr const char* resonance    = "filter_resonance";
    inline constexpr const char* filterDrive  = "filter_drive";
    inline constexpr const char* keyTrack     = "filter_keytrack";
    inline constexpr const char* filterMix    = "filter_mix";
    inline constexpr const char* filterEnvAmt = "filter_env_amount";

    inline constexpr const char* fenvAttack   = "fenv_attack";
    inline constexpr const char* fenvDecay    = "fenv_decay";
    inline constexpr const char* fenvSustain  = "fenv_sustain";
    inline constexpr const char* fenvRelease  = "fenv_release";

    inline constexpr const char* ampAttack    = "amp_attack";
    inline constexpr const char* ampDecay     = "amp_decay";
    inline constexpr const char* ampSustain   = "amp_sustain";
    inline constexpr const char* ampRelease   = "amp_release";

    // ---- envelope de modulacao (ENV 3, v0.4) ----
    inline constexpr const char* menvAttack   = "menv_attack";
    inline constexpr const char* menvDecay    = "menv_decay";
    inline constexpr const char* menvSustain  = "menv_sustain";
    inline constexpr const char* menvRelease  = "menv_release";

    inline constexpr const char* masterGain   = "master_gain";
    inline constexpr const char* bendRange    = "pitch_bend_range";

    inline constexpr const char* voiceMode    = "voice_mode";
    inline constexpr const char* voiceCount   = "voice_count";
    inline constexpr const char* glide        = "voice_glide";
    inline constexpr const char* unisonVoices = "unison_voices";
    inline constexpr const char* unisonDetune = "unison_detune";
    inline constexpr const char* unisonSpread = "unison_spread";
    inline constexpr const char* bassMono     = "bass_mono";

    // ---- sequencer / arpeggiator / key (v0.3) ----
    inline constexpr const char* seqMode      = "seq_mode";
    inline constexpr const char* seqTrigger   = "seq_trigger";
    inline constexpr const char* seqRate      = "seq_rate";
    inline constexpr const char* timeFeel     = "time_feel";
    inline constexpr const char* seqLength    = "seq_length";
    inline constexpr const char* seqGate      = "seq_gate";
    inline constexpr const char* seqOctave    = "seq_octave";
    inline constexpr const char* keyRoot      = "key_root";
    inline constexpr const char* keyScale     = "key_scale";
    inline constexpr const char* scaleLock    = "scale_lock";
    inline constexpr const char* arpMode      = "arp_mode";
    inline constexpr const char* arpOctaves   = "arp_octaves";
    inline constexpr const char* humanizeVel  = "humanize_vel";
    inline constexpr const char* humanizeTime = "humanize_time";
    inline constexpr const char* midiOut      = "midi_out";
    inline constexpr const char* seqSwing     = "seq_swing";       // v0.5
    inline constexpr const char* seqTranspose = "seq_transpose";   // v0.5

    // ---- efeitos (v0.6) ----
    inline constexpr const char* fxDistOn     = "fx_dist_on";
    inline constexpr const char* fxDistType   = "fx_dist_type";
    inline constexpr const char* fxDistDrive  = "fx_dist_drive";
    inline constexpr const char* fxDistTone   = "fx_dist_tone";
    inline constexpr const char* fxDistMix    = "fx_dist_mix";
    inline constexpr const char* fxEqOn       = "fx_eq_on";
    inline constexpr const char* fxEqLow      = "fx_eq_low";
    inline constexpr const char* fxEqMid      = "fx_eq_mid";
    inline constexpr const char* fxEqHigh     = "fx_eq_high";
    inline constexpr const char* fxChOn       = "fx_ch_on";
    inline constexpr const char* fxChRate     = "fx_ch_rate";
    inline constexpr const char* fxChDepth    = "fx_ch_depth";
    inline constexpr const char* fxChMix      = "fx_ch_mix";
    inline constexpr const char* fxDlOn       = "fx_dl_on";
    inline constexpr const char* fxDlDiv      = "fx_dl_div";
    inline constexpr const char* fxDlFb       = "fx_dl_fb";
    inline constexpr const char* fxDlMix      = "fx_dl_mix";
    inline constexpr const char* fxDlPing     = "fx_dl_ping";
    inline constexpr const char* fxRvOn       = "fx_rv_on";
    inline constexpr const char* fxRvSize     = "fx_rv_size";
    inline constexpr const char* fxRvDamp     = "fx_rv_damp";
    inline constexpr const char* fxRvWidth    = "fx_rv_width";
    inline constexpr const char* fxRvMix      = "fx_rv_mix";
    inline constexpr const char* fxCpOn       = "fx_cp_on";
    inline constexpr const char* fxCpThresh   = "fx_cp_thresh";
    inline constexpr const char* fxCpRatio    = "fx_cp_ratio";
    inline constexpr const char* fxCpAttack   = "fx_cp_attack";
    inline constexpr const char* fxCpRelease  = "fx_cp_release";
    inline constexpr const char* fxCpMakeup   = "fx_cp_makeup";
    inline constexpr const char* fxLimOn      = "fx_lim_on";

    // ---- bateria (v0.7) ----
    inline constexpr const char* drumOn     = "drum_on";
    inline constexpr const char* drumMode   = "drum_mode";     // 0 Transport, 1 Always
    inline constexpr const char* drumLength = "drum_length";
    inline constexpr const char* drumSwing  = "drum_swing";
    inline constexpr const char* drumVolume = "drum_volume";
    inline constexpr const char* drumKeys   = "drum_keys";
    inline juce::String drum (int row, const char* name) { return "drum" + juce::String (row + 1) + "_" + name; }
    inline constexpr const char* tempoSource  = "tempo_source";
    inline constexpr const char* internalBpm  = "internal_bpm";

    inline juce::String lfo (int index, const char* name)  { return "lfo" + juce::String (index) + "_" + name; }
    inline juce::String mod (int slot, const char* name)   { return "mod" + juce::String (slot) + "_" + name; }
    inline juce::String macro (int index)                  { return "macro" + juce::String (index); }
}

inline const juce::StringArray voiceModeNames  { "Poly", "Mono", "Legato" };
inline const juce::StringArray voiceCountNames { "1", "2", "4", "8", "16", "32" };
inline constexpr int voiceCountValues[] { 1, 2, 4, 8, 16, 32 };

inline const juce::StringArray seqModeNames    { "Off", "Sequence", "Arp" };
inline const juce::StringArray seqTriggerNames { "Key", "Latch", "Transport" };
inline const juce::StringArray seqRateNames    { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" };
inline const juce::StringArray timeFeelNames   { "Half", "Normal", "Double" };
inline const juce::StringArray arpModeNames    { "Up", "Down", "Up/Down", "Random", "Order", "Chord" };
inline const juce::StringArray tempoSourceNames { "Host", "Internal" };

inline const juce::StringArray lfoShapeNames { "Sine", "Triangle", "Saw", "Square", "S&H", "Random", "Reverse Saw" };
inline const juce::StringArray lfoDivNames   { "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/64" };
inline constexpr double lfoDivBeats[] { 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 0.0625 };

inline const juce::StringArray waveNames    { "Sine", "Triangle", "Saw", "Square", "Pulse", "Noise", "Wavetable", "FM" };
inline const juce::StringArray fmRatioNames { "0.5", "1", "1.5", "2", "3", "4", "5", "7" };
inline constexpr float fmRatios[] { 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 5.0f, 7.0f };
inline const juce::StringArray subWaveNames { "Sine", "Triangle", "Square" };
inline const juce::StringArray noiseNames   { "White", "Pink", "Digital" };
inline const juce::StringArray filterNames  { "LP12", "LP24", "HP12", "HP24", "BP", "NOTCH" };

inline constexpr int numOscillators = 3;

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
} // namespace mm
