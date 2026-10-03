#include "Displays.h"
#include "Hardware.h"
#include "DSP/Oscillator.h"
#include "DSP/WavetableBank.h"
#include "Synth/SynthParams.h"
#include <complex>

using namespace juce;

namespace mm
{
// ============================================================== base
void WatchingDisplay::paintBackground (Graphics& g, Rectangle<float> r, int vLines, int hLines) const
{
    const auto& t = themeOf (*this);
    hw::inset (g, r, t, 3.0f);                         // monitor embutido com sombra interna
    g.setColour (t.accentDark.withAlpha (0.22f));      // grade tecnica (vermelho muito escuro)
    for (int i = 1; i < vLines; ++i)
    {
        const float x = r.getX() + r.getWidth() * (float) i / (float) vLines;
        g.drawVerticalLine (roundToInt (x), r.getY() + 2, r.getBottom() - 2);
    }
    for (int i = 1; i < hLines; ++i)
    {
        const float y = r.getY() + r.getHeight() * (float) i / (float) hLines;
        g.drawHorizontalLine (roundToInt (y), r.getX() + 2, r.getRight() - 2);
    }
}

// ============================================================== waveform
WaveformDisplay::WaveformDisplay (APVTS& st, int i)
    : wave   (st.getRawParameterValue (ParamIDs::osc (i, "wave"))),
      phase  (st.getRawParameterValue (ParamIDs::osc (i, "phase"))),
      level  (st.getRawParameterValue (ParamIDs::osc (i, "level"))),
      octave (st.getRawParameterValue (ParamIDs::osc (i, "octave"))),
      wt     (st.getRawParameterValue (ParamIDs::osc (i, "wt"))),
      wtpos  (st.getRawParameterValue (ParamIDs::osc (i, "wtpos"))),
      fmratio (st.getRawParameterValue (ParamIDs::osc (i, "fmratio"))),
      fmamt  (st.getRawParameterValue (ParamIDs::osc (i, "fmamt")))
{
}

void WaveformDisplay::takeSnapshot (Snapshot& s) const
{
    s = { wave->load(), phase->load(), level->load(), octave->load(),
          wt->load(), wtpos->load(), fmratio->load(), fmamt->load() };
}

static float waveShape (int w, float t, int index)
{
    switch (w)
    {
        case 0: return std::sin (MathConstants<float>::twoPi * t);
        case 1: return 1.0f - 4.0f * std::abs (t - 0.5f);
        case 2: return 2.0f * t - 1.0f;
        case 3: return t < 0.5f ? 1.0f : -1.0f;
        case 4: return (t < 0.25f ? 1.0f : -1.0f) + 0.5f;
        default:
        {
            uint32_t h = (uint32_t) index * 2654435761u;
            h ^= h >> 13; h *= 1274126177u; h ^= h >> 16;
            return (float) (h & 0xffff) / 32767.5f - 1.0f;
        }
    }
}

void WaveformDisplay::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    auto bounds = getLocalBounds().toFloat();
    paintBackground (g, bounds, 8, 4);

    const int w = jlimit (0, 7, (int) wave->load());
    const float lvl = level->load();
    const float ph = phase->load();
    const int table = jlimit (0, WavetableBank::numTables() - 1, (int) wt->load());
    const float pos = jlimit (0.0f, 1.0f, wtpos->load());
    const float ratio = fmRatios[jlimit (0, 7, (int) fmratio->load())];
    const float fmIndex = jlimit (0.0f, 1.0f, fmamt->load()) * 8.0f;
    const auto& bank = WavetableBank::get();
    const float cycles = (w == (int) Oscillator::Wave::Wavetable) ? 1.0f : 2.0f;
    auto area = bounds.reduced (12.0f, 14.0f);

    // mesma matematica do DSP (WavetableBank / FM do Oscillator)
    auto sampleAt = [&] (float tt, int i, float framePos)
    {
        if (w == (int) Oscillator::Wave::Wavetable) return bank.read (table, 0, framePos, tt);
        if (w == (int) Oscillator::Wave::FM)
            return std::sin (MathConstants<float>::twoPi * tt + fmIndex * std::sin (MathConstants<float>::twoPi * tt * ratio));
        return waveShape (w, tt, i);
    };

    auto makePath = [&] (Rectangle<float> r, float amp, float framePos)
    {
        Path p;
        const int n = jmax (2, (int) r.getWidth());
        for (int i = 0; i <= n; ++i)
        {
            const float x = (float) i / (float) n;
            float tt = ph + x * cycles;
            tt -= std::floor (tt);
            const float v = jlimit (-1.0f, 1.0f, sampleAt (tt, i, framePos));
            const Point<float> pt (r.getX() + x * r.getWidth(), r.getCentreY() - v * amp * r.getHeight() * 0.45f);
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        return p;
    };

    // camadas de fundo: na WAVETABLE sao quadros REAIS da tabela (visao 3D); nas outras, eco da forma
    for (int layer = 4; layer >= 1; --layer)
    {
        auto r = area.translated ((float) layer * 7.0f, (float) -layer * 4.0f).withTrimmedRight ((float) layer * 7.0f);
        const float framePos = (w == (int) Oscillator::Wave::Wavetable) ? (float) layer / 4.0f : pos;
        g.setColour (t.accentDark.withAlpha (0.18f + 0.05f * (float) (4 - layer)));
        g.strokePath (makePath (r, 0.75f, framePos), PathStrokeType (1.0f));
    }

    if (lvl <= 0.0001f)
    {
        g.setColour (t.textDim);
        g.setFont (FontOptions (12.0f, Font::bold));
        g.drawText ("OFF", bounds, Justification::centred);
        return;
    }

    strokeNeon (g, makePath (area, 0.2f + 0.8f * lvl, pos), t.accent, 2.0f);

    if (w == (int) Oscillator::Wave::Wavetable || w == (int) Oscillator::Wave::FM)
    {
        g.setFont (FontOptions (10.0f));
        g.setColour (t.textDim);
        g.drawText (w == (int) Oscillator::Wave::Wavetable
                        ? WavetableBank::names()[table].toUpperCase() + "  POS " + String (roundToInt (pos * 100.0f)) + "%"
                        : "FM  RATIO " + fmRatioNames[jlimit (0, 7, (int) fmratio->load())] + "  AMT " + String (roundToInt (fmIndex * 12.5f)) + "%",
                    getLocalBounds().reduced (8, 4), Justification::bottomRight);
    }
}

// ============================================================== filter
FilterDisplay::FilterDisplay (APVTS& st)
    : type (st.getRawParameterValue (ParamIDs::filterType)),
      cutoff (st.getRawParameterValue (ParamIDs::cutoff)),
      resonance (st.getRawParameterValue (ParamIDs::resonance)),
      drive (st.getRawParameterValue (ParamIDs::filterDrive)),
      mix (st.getRawParameterValue (ParamIDs::filterMix))
{
}

void FilterDisplay::takeSnapshot (Snapshot& s) const
{
    s = { type->load(), cutoff->load(), resonance->load(), drive->load(), mix->load() };
}

void FilterDisplay::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    auto bounds = getLocalBounds().toFloat();
    paintBackground (g, bounds, 1, 4);

    const auto mode = FilterMode::fromChoice ((int) type->load());
    const double fc = cutoff->load();
    const double res = resonance->load();
    const double k2 = 2.0 - 1.95 * res;
    const double k1 = mode.twoStages ? 2.0 - 1.95 * res * 0.5 : k2;
    const double mx = mix->load();
    const float dr = drive->load();
    const double gainIn = dr > 0.001f ? driveGain (dr) * driveCompensation (dr) : 1.0; // ganho de pequenos sinais

    auto stage = [&] (double w, double k)
    {
        const std::complex<double> s (0.0, w);
        const auto den = s * s + k * s + 1.0;
        switch (mode.baseType)
        {
            case 0:  return 1.0 / den;
            case 1:  return (s * s) / den;
            case 2:  return s / den;
            default: return (s * s + 1.0) / den;
        }
    };

    constexpr double minDb = -36.0, maxDb = 30.0, fMin = 20.0, fMax = 20000.0;
    auto area = bounds.reduced (2.0f);
    auto yFor = [&] (double db) { return area.getBottom() - (float) ((jlimit (minDb, maxDb, db) - minDb) / (maxDb - minDb)) * area.getHeight(); };

    // grade de frequencias
    g.setColour (t.track.withAlpha (0.45f));
    for (double f : { 100.0, 1000.0, 10000.0 })
    {
        const float x = area.getX() + (float) (std::log (f / fMin) / std::log (fMax / fMin)) * area.getWidth();
        g.drawVerticalLine (roundToInt (x), area.getY(), area.getBottom());
    }
    g.setColour (t.textDim);
    g.setFont (FontOptions (10.0f));
    g.drawText ("100", Rectangle<float> (area.getX() + area.getWidth() * 0.333f + 3, area.getBottom() - 14, 30, 12), Justification::left);
    g.drawText ("1k",  Rectangle<float> (area.getX() + area.getWidth() * 0.666f + 3, area.getBottom() - 14, 30, 12), Justification::left);

    Path curve;
    const int n = (int) area.getWidth();
    for (int i = 0; i <= n; ++i)
    {
        const double f = fMin * std::pow (fMax / fMin, (double) i / n);
        const double w = f / fc;
        auto h = stage (w, k1);
        if (mode.twoStages) h *= stage (w, k2);
        const auto total = (mx * h + (1.0 - mx)) * gainIn;
        const double db = 20.0 * std::log10 (jmax (1.0e-6, std::abs (total)));
        const Point<float> pt (area.getX() + (float) i, yFor (db));
        if (i == 0) curve.startNewSubPath (pt); else curve.lineTo (pt);
    }

    Path fill (curve);
    fill.lineTo (area.getRight(), area.getBottom());
    fill.lineTo (area.getX(), area.getBottom());
    fill.closeSubPath();
    g.setGradientFill (ColourGradient (t.accent.withAlpha (0.28f), 0, area.getY(), t.accent.withAlpha (0.0f), 0, area.getBottom(), false));
    g.fillPath (fill);
    strokeNeon (g, curve, t.accent, 2.0f);

    // linha de 0 dB
    g.setColour (t.textDim.withAlpha (0.4f));
    g.drawHorizontalLine (roundToInt (yFor (0.0)), area.getX(), area.getRight());
}

// ============================================================== envelope
EnvelopeDisplay::EnvelopeDisplay (APVTS& st, const String& ai, const String& di, const String& si, const String& ri)
    : a (st.getParameter (ai)), d (st.getParameter (di)), s (st.getParameter (si)), r (st.getParameter (ri))
{
    jassert (a && d && s && r);
}

void EnvelopeDisplay::takeSnapshot (Snapshot& snap) const
{
    snap = { a->getValue(), d->getValue(), s->getValue(), r->getValue(), (float) dragging, (float) hover };
}

EnvelopeDisplay::Geometry EnvelopeDisplay::getGeometry() const
{
    Geometry geo;
    geo.area = getLocalBounds().toFloat().reduced (10.0f, 12.0f);
    geo.segW = geo.area.getWidth() * 0.28f;
    geo.holdW = geo.area.getWidth() * 0.12f;
    const float bottom = geo.area.getBottom(), top = geo.area.getY();
    geo.p0 = { geo.area.getX(), bottom };
    geo.attack = { geo.p0.x + a->getValue() * geo.segW, top };
    const float sy = bottom - s->getValue() * geo.area.getHeight();
    geo.decay = { geo.attack.x + d->getValue() * geo.segW, sy };
    geo.sustainEnd = { geo.decay.x + geo.holdW, sy };
    geo.release = { geo.sustainEnd.x + r->getValue() * geo.segW, bottom };
    return geo;
}

void EnvelopeDisplay::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    paintBackground (g, getLocalBounds().toFloat(), 8, 4);
    const auto geo = getGeometry();

    Path env;   // segmentos lineares, identicos ao juce::ADSR usado no DSP
    env.startNewSubPath (geo.p0);
    env.lineTo (geo.attack);
    env.lineTo (geo.decay);
    env.lineTo (geo.sustainEnd);
    env.lineTo (geo.release);

    Path fill (env);
    fill.lineTo (geo.p0);
    fill.closeSubPath();
    g.setGradientFill (ColourGradient (t.accent.withAlpha (0.22f), 0, geo.area.getY(), t.accent.withAlpha (0.0f), 0, geo.area.getBottom(), false));
    g.fillPath (fill);
    strokeNeon (g, env, t.accent, 2.0f);

    const Point<float> handles[] { geo.attack, geo.decay, geo.release };
    for (int i = 0; i < 3; ++i)
    {
        const bool active = (i == dragging || i == hover);
        const float rad = active ? 6.0f : 4.5f;
        g.setColour (t.display);
        g.fillEllipse (handles[i].x - rad, handles[i].y - rad, rad * 2, rad * 2);
        g.setColour (active ? t.text : t.accentSoft);
        g.drawEllipse (handles[i].x - rad, handles[i].y - rad, rad * 2, rad * 2, 2.0f);
    }
}

int EnvelopeDisplay::hitTestHandle (Point<float> p) const
{
    const auto geo = getGeometry();
    const Point<float> handles[] { geo.attack, geo.decay, geo.release };
    int best = -1; float bestDist = 12.0f;
    for (int i = 0; i < 3; ++i)
        if (auto dist = handles[i].getDistanceFrom (p); dist < bestDist) { best = i; bestDist = dist; }
    return best;
}

void EnvelopeDisplay::setNormalised (RangedAudioParameter* p, float v)
{
    p->setValueNotifyingHost (jlimit (0.0f, 1.0f, v));
}

void EnvelopeDisplay::mouseMove (const MouseEvent& e)
{
    hover = hitTestHandle (e.position);
    setMouseCursor (hover >= 0 ? MouseCursor::DraggingHandCursor : MouseCursor::NormalCursor);
}

void EnvelopeDisplay::mouseExit (const MouseEvent&) { hover = -1; }

void EnvelopeDisplay::mouseDown (const MouseEvent& e)
{
    dragging = hitTestHandle (e.position);
    if (dragging == 0) a->beginChangeGesture();
    if (dragging == 1) { d->beginChangeGesture(); s->beginChangeGesture(); }
    if (dragging == 2) r->beginChangeGesture();
}

void EnvelopeDisplay::mouseDrag (const MouseEvent& e)
{
    if (dragging < 0) return;
    const auto geo = getGeometry();
    const auto p = e.position;

    if (dragging == 0) setNormalised (a, (p.x - geo.p0.x) / geo.segW);
    if (dragging == 1)
    {
        setNormalised (d, (p.x - geo.attack.x) / geo.segW);
        setNormalised (s, (geo.area.getBottom() - p.y) / geo.area.getHeight());
    }
    if (dragging == 2) setNormalised (r, (p.x - geo.sustainEnd.x) / geo.segW);
}

void EnvelopeDisplay::mouseUp (const MouseEvent&)
{
    if (dragging == 0) a->endChangeGesture();
    if (dragging == 1) { d->endChangeGesture(); s->endChangeGesture(); }
    if (dragging == 2) r->endChangeGesture();
    dragging = -1;
}
} // namespace mm
