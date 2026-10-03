#include "FxPage.h"

using namespace juce;
namespace ids = mm::ParamIDs;

namespace mm
{
FxPage::FxPage (APVTS& s)
    : dist (s, "DISTORTION", ids::fxDistOn), sat (s, "SATURATION", "fx_sat_on"), crush (s, "BITCRUSH", "fx_cr_on"),
      eq (s, "EQ", ids::fxEqOn), chorus (s, "CHORUS", ids::fxChOn),
      phaser (s, "PHASER", "fx_ph_on"), flanger (s, "FLANGER", "fx_fl_on"), glitch (s, "GLITCH", "fx_gl_on"),
      delay (s, "DELAY", ids::fxDlOn), reverb (s, "REVERB", ids::fxRvOn), comp (s, "COMPRESSOR", ids::fxCpOn),
      limiter (s, "LIMITER", ids::fxLimOn)
{
    dist.setTitleChoice (new ChoiceBox (s, ids::fxDistType));
    dist.addKnob (new Knob (s, ids::fxDistDrive, "Drive"));
    dist.addKnob (new Knob (s, ids::fxDistTone, "Tone"));
    dist.addKnob (new Knob (s, ids::fxDistMix, "Mix"));

    sat.setTitleChoice (new ChoiceBox (s, "fx_sat_type"));
    sat.addKnob (new Knob (s, "fx_sat_drive", "Drive"));
    sat.addKnob (new Knob (s, "fx_sat_mix", "Mix"));

    crush.addKnob (new Knob (s, "fx_cr_bits", "Bits"));
    crush.addKnob (new Knob (s, "fx_cr_down", "Down"));
    crush.addKnob (new Knob (s, "fx_cr_mix", "Mix"));

    phaser.addKnob (new Knob (s, "fx_ph_rate", "Rate"));
    phaser.addKnob (new Knob (s, "fx_ph_depth", "Depth"));
    phaser.addKnob (new Knob (s, "fx_ph_fb", "Fdbk", true));
    phaser.addKnob (new Knob (s, "fx_ph_mix", "Mix"));

    flanger.addKnob (new Knob (s, "fx_fl_rate", "Rate"));
    flanger.addKnob (new Knob (s, "fx_fl_depth", "Depth"));
    flanger.addKnob (new Knob (s, "fx_fl_fb", "Fdbk", true));
    flanger.addKnob (new Knob (s, "fx_fl_mix", "Mix"));

    glitch.setTitleChoice (new ChoiceBox (s, "fx_gl_mode"));
    glitchDiv = std::make_unique<ChoiceBox> (s, "fx_gl_div", "Division");
    glitch.addComponent (glitchDiv.get());
    glitch.addKnob (new Knob (s, "fx_gl_mix", "Mix"));

    eq.addKnob (new Knob (s, ids::fxEqLow, "Low", true));
    eq.addKnob (new Knob (s, ids::fxEqMid, "Mid", true));
    eq.addKnob (new Knob (s, ids::fxEqHigh, "High", true));

    chorus.addKnob (new Knob (s, ids::fxChRate, "Rate"));
    chorus.addKnob (new Knob (s, ids::fxChDepth, "Depth"));
    chorus.addKnob (new Knob (s, ids::fxChMix, "Mix"));

    delay.setTitleChoice (new ChoiceBox (s, ids::fxDlDiv));
    delay.addKnob (new Knob (s, ids::fxDlFb, "Feedback"));
    delay.addKnob (new Knob (s, ids::fxDlMix, "Mix"));
    pingButton.setClickingTogglesState (true);
    pingAttachment = std::make_unique<APVTS::ButtonAttachment> (s, ids::fxDlPing, pingButton);
    delay.setExtraButton (&pingButton);

    reverb.addKnob (new Knob (s, ids::fxRvSize, "Size"));
    reverb.addKnob (new Knob (s, ids::fxRvDamp, "Damp"));
    reverb.addKnob (new Knob (s, ids::fxRvWidth, "Width"));
    reverb.addKnob (new Knob (s, ids::fxRvMix, "Mix"));

    comp.addKnob (new Knob (s, ids::fxCpThresh, "Thresh"));
    comp.addKnob (new Knob (s, ids::fxCpRatio, "Ratio"));
    comp.addKnob (new Knob (s, ids::fxCpAttack, "Attack"));
    comp.addKnob (new Knob (s, ids::fxCpRelease, "Release"));
    comp.addKnob (new Knob (s, ids::fxCpMakeup, "Makeup"));

    limiterInfo.setText ("CEILING\n-0.3 dBFS", juce::dontSendNotification);
    limiterInfo.setJustificationType (juce::Justification::centred);
    limiter.addComponent (&limiterInfo);

    for (auto* p : { &dist, &sat, &crush, &eq, &chorus, &phaser, &flanger, &glitch, &delay, &reverb, &comp, &limiter })
    {
        addAndMakeVisible (p);
        p->updateBypassLook();
    }
    startTimerHz (8);
}

void FxPage::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    // ordem real do sinal (FxEngine)
    static const char* chain[] { "SYNTH", "DIST", "SAT", "CRUSH", "EQ", "CHORUS", "PHASER", "FLANGER", "GLITCH", "DELAY", "REVERB", "COMP", "MASTER", "LIMITER" };
    auto r = chainArea;
    const int n = (int) std::size (chain);
    const int w = r.getWidth() / n;
    g.setFont (FontOptions (11.0f, Font::bold));
    for (int i = 0; i < n; ++i)
    {
        auto cell = r.removeFromLeft (w);
        // so acende em vermelho o modulo LIGADO (SYNTH e MASTER sempre neutros)
        static const int panelIndex[] { -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, -1, 11 };
        const FxModulePanel* ps[] { &dist, &sat, &crush, &eq, &chorus, &phaser, &flanger, &glitch, &delay, &reverb, &comp, &limiter };
        const bool lit = panelIndex[i] >= 0 && ps[panelIndex[i]]->isOn();
        g.setColour (panelIndex[i] < 0 ? t.text.withAlpha (0.7f) : (lit ? t.accentSoft : t.textDim.withAlpha (0.55f)));
        g.drawText (chain[i], cell, Justification::centred);
        if (i < n - 1)
        {
            g.setColour (t.textDim.withAlpha (0.5f));
            g.drawText (">", cell.withX (cell.getRight() - 6).withWidth (12), Justification::centred);
        }
    }
}

void FxPage::resized()
{
    constexpr int gap = 8;
    auto r = getLocalBounds();
    chainArea = r.removeFromTop (22);
    r.removeFromTop (4);

    const int rowH = (r.getHeight() - 2 * gap) / 3;
    auto row = [&] (std::initializer_list<FxModulePanel*> panels, std::initializer_list<int> weights)
    {
        auto line = r.removeFromTop (rowH);
        r.removeFromTop (gap);
        int total = 0; for (int w : weights) total += w;
        const int width = line.getWidth() - gap * ((int) panels.size() - 1);
        auto w = weights.begin();
        for (auto* p : panels)
        {
            p->setBounds (p == *std::prev (panels.end()) ? line : line.removeFromLeft (width * *w / total));
            line.removeFromLeft (gap);
            ++w;
        }
    };
    row ({ &dist, &sat, &crush, &eq },         { 3, 3, 3, 3 });
    row ({ &chorus, &phaser, &flanger, &glitch }, { 3, 4, 4, 3 });
    row ({ &delay, &reverb, &comp, &limiter },  { 3, 4, 5, 2 });

}

void FxPage::timerCallback()
{
    uint32_t mask = 0;
    int bit = 0;
    for (auto* p : { &dist, &sat, &crush, &eq, &chorus, &phaser, &flanger, &glitch, &delay, &reverb, &comp, &limiter })
        mask |= (p->isOn() ? 1u : 0u) << bit++;
    if (mask != lastOnMask) { lastOnMask = mask; repaint (chainArea); }
}
} // namespace mm
