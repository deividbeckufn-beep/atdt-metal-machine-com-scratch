#include "SynthPage.h"
#include "DSP/Oscillator.h"
#include "Parameters/Parameters.h"

using namespace juce;
namespace ids = mm::ParamIDs;

namespace mm
{
// ------------------------------------------------------------------ OSC
SynthPage::OscPanel::OscPanel (APVTS& s, int i)
    : Panel ("OSC " + String (i)),
      wave (s, ids::osc (i, "wave")), display (s, i),
      oct   (s, ids::osc (i, "octave"), "Oct", true),
      semi  (s, ids::osc (i, "semi"), "Semi", true),
      fine  (s, ids::osc (i, "fine"), "Fine", true),
      phase (s, ids::osc (i, "phase"), "Phase"),
      level (s, ids::osc (i, "level"), "Level"),
      pan   (s, ids::osc (i, "pan"), "Pan", true),
      table (s, ids::osc (i, "wt")), ratio (s, ids::osc (i, "fmratio")),
      wtPos (s, ids::osc (i, "wtpos"), "WT Pos"),
      fmAmt (s, ids::osc (i, "fmamt"), "FM Amt"),
      waveParam (s.getRawParameterValue (ids::osc (i, "wave")))
{
    for (auto* c : std::initializer_list<Component*> { &wave, &display, &oct, &semi, &fine, &phase, &level, &pan })
        addAndMakeVisible (c);
    for (auto* c : std::initializer_list<Component*> { &table, &ratio, &wtPos, &fmAmt })
        addChildComponent (c);
    timerCallback();
    startTimerHz (10);
}

void SynthPage::OscPanel::timerCallback()
{
    const int w = (int) waveParam->load();
    if (w == lastWave)
        return;
    lastWave = w;
    const bool isWt = w == (int) Oscillator::Wave::Wavetable, isFm = w == (int) Oscillator::Wave::FM;
    table.setVisible (isWt); wtPos.setVisible (isWt);
    ratio.setVisible (isFm); fmAmt.setVisible (isFm);
    resized();
}

void SynthPage::OscPanel::resized()
{
    auto bar = getTitleBar();
    wave.setBounds (bar.removeFromRight (100));
    bar.removeFromRight (4);
    table.setBounds (bar.removeFromRight (112));
    ratio.setBounds (table.getBounds().withWidth (60).withX (table.getRight() - 60));

    auto r = getContentArea();
    display.setBounds (r.removeFromTop (r.getHeight() - 96).reduced (2, 2));
    r.removeFromTop (4);
    if (wtPos.isVisible())      layoutRow (r, { &oct, &semi, &fine, &phase, &level, &pan, &wtPos });
    else if (fmAmt.isVisible()) layoutRow (r, { &oct, &semi, &fine, &phase, &level, &pan, &fmAmt });
    else                        layoutRow (r, { &oct, &semi, &fine, &phase, &level, &pan });
}

// ------------------------------------------------------------------ SUB / NOISE
SynthPage::SubPanel::SubPanel (APVTS& s)
    : Panel ("SUB"), wave (s, ids::subWave, "Wave"),
      octave (s, ids::subOctave, "Oct", true), level (s, ids::subLevel, "Level")
{
    addAndMakeVisible (wave); addAndMakeVisible (octave); addAndMakeVisible (level);
}

void SynthPage::SubPanel::resized()
{
    layoutRow (getContentArea(), { &wave, &octave, &level });
}

SynthPage::NoisePanel::NoisePanel (APVTS& s)
    : Panel ("NOISE"), type (s, ids::noiseType, "Type"), level (s, ids::noiseLevel, "Level")
{
    addAndMakeVisible (type); addAndMakeVisible (level);
}

void SynthPage::NoisePanel::resized()
{
    layoutRow (getContentArea(), { &type, &level });
}

// ------------------------------------------------------------------ FILTER
SynthPage::FilterPanel::FilterPanel (APVTS& s)
    : Panel ("FILTER"), type (s, ids::filterType), display (s),
      cutoff (s, ids::cutoff, "Cutoff"), reso (s, ids::resonance, "Reso"),
      drive (s, ids::filterDrive, "Drive"), keytrack (s, ids::keyTrack, "Key Trk"),
      mix (s, ids::filterMix, "Mix")
{
    for (auto* c : std::initializer_list<Component*> { &type, &display, &cutoff, &reso, &drive, &keytrack, &mix })
        addAndMakeVisible (c);
}

void SynthPage::FilterPanel::resized()
{
    type.setBounds (getTitleBar().removeFromRight (100));
    auto r = getContentArea();
    display.setBounds (r.removeFromTop (r.getHeight() - 96).reduced (2, 2));
    r.removeFromTop (4);
    layoutRow (r, { &cutoff, &reso, &drive, &keytrack, &mix });
}

// ------------------------------------------------------------------ ENV
SynthPage::EnvPanel::EnvPanel (APVTS& s, const String& panelTitle, const String& a, const String& d,
                               const String& su, const String& r, const String& amountID)
    : Panel (panelTitle), display (s, a, d, su, r),
      attack (s, a, "Attack"), decay (s, d, "Decay"), sustain (s, su, "Sustain"), release (s, r, "Release")
{
    for (auto* c : std::initializer_list<Component*> { &display, &attack, &decay, &sustain, &release })
        addAndMakeVisible (c);
    if (amountID.isNotEmpty())
    {
        amount = std::make_unique<Knob> (s, amountID, "Amount", true);
        addAndMakeVisible (*amount);
    }
}

void SynthPage::EnvPanel::resized()
{
    auto r = getContentArea();
    display.setBounds (r.removeFromTop (r.getHeight() - 96).reduced (2, 2));
    r.removeFromTop (4);
    if (amount != nullptr) layoutRow (r, { &attack, &decay, &sustain, &release, amount.get() });
    else                   layoutRow (r, { &attack, &decay, &sustain, &release });
}

// ------------------------------------------------------------------ VOICE
SynthPage::VoicePanel::VoicePanel (APVTS& s)
    : Panel ("VOICE"), mode (s, ids::voiceMode, "Mode"), voices (s, ids::voiceCount, "Voices"),
      glide (s, ids::glide, "Glide"), unison (s, ids::unisonVoices, "Unison"),
      detune (s, ids::unisonDetune, "Detune"), spread (s, ids::unisonSpread, "Spread")
{
    for (auto* c : std::initializer_list<Component*> { &mode, &voices, &glide, &unison, &detune, &spread })
        addAndMakeVisible (c);
}

void SynthPage::VoicePanel::resized()
{
    auto r = getContentArea();
    layoutRow (r.removeFromTop (40), { &mode, &voices });
    r.removeFromTop (4);
    auto top = r.removeFromTop (r.getHeight() / 2);
    layoutRow (top, { &glide, &unison });
    layoutRow (r, { &detune, &spread });
}

// ------------------------------------------------------------------ MASTER
SynthPage::MasterPanel::MasterPanel (APVTS& s)
    : Panel ("MASTER"), gain (s, ids::masterGain, "Gain"),
      bend (s, ids::bendRange, "Bend"), bassMono (s, ids::bassMono, "Mono <")
{
    addAndMakeVisible (gain); addAndMakeVisible (bend); addAndMakeVisible (bassMono);
}

void SynthPage::MasterPanel::resized()
{
    auto r = getContentArea();
    const int h = r.getHeight() / 3;
    gain.setBounds (r.removeFromTop (h));
    bend.setBounds (r.removeFromTop (h));
    bassMono.setBounds (r);
}

// ------------------------------------------------------------------ PAGE
SynthPage::SynthPage (APVTS& s)
    : sub (s), noise (s), filter (s),
      filterEnv (s, "FILTER ENV", ids::fenvAttack, ids::fenvDecay, ids::fenvSustain, ids::fenvRelease, ids::filterEnvAmt),
      ampEnv (s, "AMP ENV", ids::ampAttack, ids::ampDecay, ids::ampSustain, ids::ampRelease, {}),
      voice (s), master (s)
{
    for (int i = 1; i <= numOscillators; ++i)
    {
        oscs.push_back (std::make_unique<OscPanel> (s, i));
        addAndMakeVisible (*oscs.back());
    }
    for (auto* c : std::initializer_list<Component*> { &sub, &noise, &filter, &filterEnv, &ampEnv, &voice, &master })
        addAndMakeVisible (c);
}

void SynthPage::resized()
{
    constexpr int gap = 8;
    auto r = getLocalBounds();
    auto rowA = r.removeFromTop ((r.getHeight() - gap) / 2);
    r.removeFromTop (gap);
    auto rowB = r;

    // linha A: OSC1 | OSC2 | OSC3 | (SUB / NOISE)
    auto side = rowA.removeFromRight (250);
    rowA.removeFromRight (gap);
    const int oscW = (rowA.getWidth() - 2 * gap) / 3;
    for (auto& o : oscs)
    {
        o->setBounds (rowA.removeFromLeft (oscW));
        rowA.removeFromLeft (gap);
    }
    sub.setBounds (side.removeFromTop ((side.getHeight() - gap) / 2));
    side.removeFromTop (gap);
    noise.setBounds (side);

    // linha B: FILTER | FILTER ENV | AMP ENV | VOICE | MASTER
    master.setBounds (rowB.removeFromRight (96));
    rowB.removeFromRight (gap);
    voice.setBounds (rowB.removeFromRight (190));
    rowB.removeFromRight (gap);
    const int unit = (rowB.getWidth() - 2 * gap) / 14;
    filter.setBounds (rowB.removeFromLeft (unit * 5));
    rowB.removeFromLeft (gap);
    filterEnv.setBounds (rowB.removeFromLeft (unit * 5));
    rowB.removeFromLeft (gap);
    ampEnv.setBounds (rowB);
}
} // namespace mm
