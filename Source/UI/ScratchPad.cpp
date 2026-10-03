#include "ScratchPad.h"
#include "Hardware.h"

using namespace juce;

namespace mm
{
ScratchPad::ScratchPad (MetalMachineAudioProcessor& p) : processor (p)
{
    setMouseCursor (MouseCursor::DraggingHandCursor);
    startTimerHz (60);
}

void ScratchPad::timerCallback()
{
    // o disco gira com a velocidade do prato (amostras/amostra -> radianos por quadro); parado = parado
    const float spd = processor.getScratch().platterSpeed.load();
    const bool on = processor.apvts.getRawParameterValue ("scr_on")->load() > 0.5f;
    angle += on ? jlimit (-0.6f, 0.6f, spd * 0.35f) : 0.0f;
    repaint();
}

void ScratchPad::resized()
{
    auto r = getLocalBounds().reduced (4);
    discArea = r.removeFromLeft (jmin (r.getHeight(), r.getWidth() / 3));
    r.removeFromLeft (10);
    waveArea = r;
}

float ScratchPad::regionNorm (float s) const
{
    const float st = processor.apvts.getRawParameterValue ("scr_start")->load();
    const float en = jmax (st + 0.01f, processor.apvts.getRawParameterValue ("scr_end")->load());
    return jlimit (0.0f, 1.0f, (s - st) / (en - st));
}

void ScratchPad::mouseDown (const MouseEvent& e)
{
    auto& sc = processor.getScratch();
    if (auto* prm = processor.apvts.getParameter ("scr_on")) if (prm->getValue() < 0.5f) prm->setValueNotifyingHost (1.0f);
    draggingDisc = discArea.contains (e.getPosition());
    const auto c = discArea.getCentre().toFloat();
    lastAngleMouse = std::atan2 ((float) e.y - c.y, (float) e.x - c.x);
    target = regionNorm (sc.displayPos.load());
    if (! draggingDisc) mouseDrag (e);
    sc.padTarget = target;
    sc.padActive = true;
}

void ScratchPad::mouseDrag (const MouseEvent& e)
{
    auto& sc = processor.getScratch();
    if (draggingDisc)   // circular: angulo da mao vira deslocamento do disco
    {
        const auto c = discArea.getCentre().toFloat();
        const float a = std::atan2 ((float) e.y - c.y, (float) e.x - c.x);
        float d = a - lastAngleMouse;
        if (d > MathConstants<float>::pi) d -= MathConstants<float>::twoPi;
        if (d < -MathConstants<float>::pi) d += MathConstants<float>::twoPi;
        lastAngleMouse = a;
        target = jlimit (0.0f, 1.0f, target + d / MathConstants<float>::twoPi * 0.35f);
    }
    else   // horizontal: x na forma de onda = posicao no sample (considera o zoom)
    {
        const float xn = jlimit (0.0f, 1.0f, (float) (e.x - waveArea.getX()) / (float) waveArea.getWidth());
        const float span = 1.0f / zoom, left = jlimit (0.0f, 1.0f - span, zoomCentre - span * 0.5f);
        target = regionNorm (left + xn * span);
    }
    sc.padTarget = target;
}

void ScratchPad::mouseUp (const MouseEvent&) { processor.getScratch().padActive = false; }

void ScratchPad::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    if (! waveArea.contains (e.getPosition())) return;
    zoomCentre = jlimit (0.0f, 1.0f, (float) (e.x - waveArea.getX()) / (float) waveArea.getWidth());
    zoom = jlimit (1.0f, 16.0f, zoom * (w.deltaY > 0 ? 1.25f : 0.8f));
}

void ScratchPad::paint (Graphics& g)
{
    const auto& t = themeOf (*this);
    auto& sc = processor.getScratch();
    const bool on = processor.apvts.getRawParameterValue ("scr_on")->load() > 0.5f;
    const bool active = sc.padActive.load() || std::abs (sc.platterSpeed.load()) > 0.02f;

    // ---- disco de vinil
    {
        auto d = discArea.toFloat().reduced (4.0f);
        const auto c = d.getCentre(); const float rad = jmin (d.getWidth(), d.getHeight()) * 0.5f;
        g.setColour (Colours::black.withAlpha (0.7f)); g.fillEllipse (c.x - rad - 3, c.y - rad + 3, rad * 2 + 6, rad * 2 + 6);
        g.setColour (Colour (0xff0b0b0d)); g.fillEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2);
        for (float rr = rad * 0.96f; rr > rad * 0.36f; rr -= 2.6f)
        { g.setColour (Colours::white.withAlpha (0.03f)); g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 0.6f); }
        for (int k = 0; k < 2; ++k)   // reflexos que giram junto com o prato
        {
            Path refl; refl.addPieSegment (c.x - rad, c.y - rad, rad * 2, rad * 2, angle + k * MathConstants<float>::pi, angle + k * MathConstants<float>::pi + 0.45f, 0.38f);
            g.setColour (Colours::white.withAlpha (0.07f)); g.fillPath (refl);
        }
        const float lab = rad * 0.34f;
        g.setColour (on ? t.accent : t.accentDark); g.fillEllipse (c.x - lab, c.y - lab, lab * 2, lab * 2);
        g.setColour (Colours::black.withAlpha (0.8f));   // marca no selo: mostra a rotacao
        g.drawLine (c.x, c.y, c.x + std::cos (angle) * lab * 0.9f, c.y + std::sin (angle) * lab * 0.9f, 2.0f);
        g.fillEllipse (c.x - 4, c.y - 4, 8, 8);
        if (active) { g.setColour (t.accent.withAlpha (0.35f)); g.drawEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2, 2.0f); }
        hw::micro (g, "Drag the record", d.toNearestInt().removeFromBottom (14), Justification::centred, t.textDim, 8.5f);
    }

    // ---- forma de onda + regiao + playhead
    hw::inset (g, waveArea.toFloat(), t, 3.0f);
    auto wa = waveArea.reduced (8, 18).toFloat();
    if (const auto* src = sc.getSourceForDisplay())
    {
        const int n = src->getNumSamples(); const float* data = src->getReadPointer (0);
        const float span = 1.0f / zoom, left = jlimit (0.0f, 1.0f - span, zoomCentre - span * 0.5f);
        auto xOf = [&] (float sn) { return wa.getX() + (sn - left) / span * wa.getWidth(); };
        const float st = processor.apvts.getRawParameterValue ("scr_start")->load();
        const float en = jmax (st + 0.01f, processor.apvts.getRawParameterValue ("scr_end")->load());
        const float ce = st + (en - st) * processor.apvts.getRawParameterValue ("scr_center")->load();
        g.setColour (t.accent.withAlpha (0.08f));
        g.fillRect (Rectangle<float>::leftTopRightBottom (jmax (wa.getX(), xOf (st)), wa.getY(), jmin (wa.getRight(), xOf (en)), wa.getBottom()));
        const int cols = (int) wa.getWidth();
        for (int x = 0; x < cols; ++x)
        {
            const float s0 = left + span * (float) x / (float) cols, s1 = left + span * (float) (x + 1) / (float) cols;
            const int a = (int) (s0 * n), b = jmax (a + 1, jmin (n, (int) (s1 * n)));
            float mx = 0; double sq = 0;
            for (int i = a; i < b; ++i) { mx = jmax (mx, std::abs (data[i])); sq += (double) data[i] * data[i]; }
            const float rms = (float) std::sqrt (sq / jmax (1, b - a));
            const bool inRegion = s0 >= st && s0 <= en;
            const float cy = wa.getCentreY(), hh = wa.getHeight() * 0.48f;
            g.setColour (inRegion ? t.accentDark.withAlpha (0.9f) : t.textDim.withAlpha (0.25f));
            g.drawVerticalLine ((int) wa.getX() + x, cy - mx * hh, cy + mx * hh);
            g.setColour (inRegion ? t.accent.withAlpha (0.9f) : t.textDim.withAlpha (0.4f));
            g.drawVerticalLine ((int) wa.getX() + x, cy - rms * hh, cy + rms * hh);
        }
        auto marker = [&] (float sn, Colour col, const String& label)
        {
            const float x = xOf (sn);
            if (x < wa.getX() - 1 || x > wa.getRight() + 1) return;
            g.setColour (col); g.drawVerticalLine ((int) x, wa.getY() - 12.0f, wa.getBottom() + 4.0f);
            const bool nearRight = x > wa.getRight() - 50.0f;   // perto da borda: rotulo a esquerda da linha
            hw::micro (g, label, { nearRight ? (int) x - 63 : (int) x + 3, (int) wa.getY() - 14, 60, 10 },
                       nearRight ? Justification::topRight : Justification::topLeft, col, 8.0f);
        };
        marker (st, t.text.withAlpha (0.8f), "START");
        marker (en, t.text.withAlpha (0.8f), "END");
        marker (ce, t.textDim, "CENTER");
        const float pos = sc.displayPos.load();   // playhead = posicao real do prato
        const float px = xOf (pos);
        if (px >= wa.getX() && px <= wa.getRight())
        {
            g.setColour (t.accent.withAlpha (0.25f)); g.fillRect (px - 3.0f, wa.getY() - 6.0f, 6.0f, wa.getHeight() + 10.0f);
            g.setColour (t.accent); g.fillRect (px - 1.0f, wa.getY() - 6.0f, 2.0f, wa.getHeight() + 10.0f);
        }
        hw::micro (g, "Zoom " + String (zoom, 1) + "x  (wheel)  |  drag = scratch", waveArea.reduced (8, 3), Justification::bottomRight, t.textDim, 8.0f);
    }
    hw::micro (g, processor.apvts.getRawParameterValue ("scr_engine")->load() > 0.5f
                    ? (sc.hasUserSample() ? "Sample: " + sc.getUserSampleName() : String ("Sample: load a WAV (using built-in voice)"))
                    : "Synth source: " + ScratchEngine::sourceNames()[(int) processor.apvts.getRawParameterValue ("scr_source")->load()],
               waveArea.reduced (8, 3), Justification::bottomLeft, t.accentSoft, 8.5f);   // embaixo: nao cobre os marcadores
}
} // namespace mm
