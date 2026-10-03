#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

// =============================================================================
//  ATDT AUDIO TOOLS - materiais de hardware (design system)
//  Metal escovado, paineis chanfrados com bisel, parafusos, telas embutidas e
//  LEDs. Tudo desenhado em codigo (sem imagens), identico em todos os modulos.
//  Vermelho = funcional (ativo/selecionado/sinal). Inativo = grafite.
// =============================================================================
namespace mm::hw
{
// textura de metal escovado: linhas horizontais finas, deterministicas por posicao
inline void brushed (juce::Graphics& g, juce::Rectangle<float> r, float strength = 1.0f)
{
    juce::Random rng ((juce::int64) (r.getX() * 131.0f + r.getY() * 17.0f + r.getWidth()));
    for (float y = r.getY() + 1.0f; y < r.getBottom() - 1.0f; y += 2.0f)
    {
        const float a = (0.010f + 0.022f * rng.nextFloat()) * strength;
        g.setColour (rng.nextBool() ? juce::Colours::white.withAlpha (a) : juce::Colours::black.withAlpha (a * 1.5f));
        g.drawHorizontalLine ((int) y, r.getX() + 1.0f, r.getRight() - 1.0f);
    }
}

inline juce::Path chamfer (juce::Rectangle<float> r, float c)
{
    juce::Path p;
    p.startNewSubPath (r.getX() + c, r.getY());
    p.lineTo (r.getRight() - c, r.getY());   p.lineTo (r.getRight(), r.getY() + c);
    p.lineTo (r.getRight(), r.getBottom() - c); p.lineTo (r.getRight() - c, r.getBottom());
    p.lineTo (r.getX() + c, r.getBottom());  p.lineTo (r.getX(), r.getBottom() - c);
    p.lineTo (r.getX(), r.getY() + c);
    p.closeSubPath();
    return p;
}

// placa de metal chanfrada, com bisel (luz vinda de cima) e textura escovada
inline void plate (juce::Graphics& g, juce::Rectangle<float> r, const Theme& t, float c = 7.0f, bool raised = true)
{
    const auto shape = chamfer (r, c);
    g.setColour (juce::Colours::black.withAlpha (0.55f));                       // sombra projetada
    g.fillPath (chamfer (r.translated (0.0f, 2.0f), c));
    g.setGradientFill (juce::ColourGradient (raised ? t.panelRaised : t.panel, 0.0f, r.getY(),
                                             t.panel.darker (0.35f), 0.0f, r.getBottom(), false));
    g.fillPath (shape);
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (shape);
        brushed (g, r, raised ? 1.0f : 0.6f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.9f));                        // contorno
    g.strokePath (shape, juce::PathStrokeType (1.2f));
    juce::Path top;                                                              // bisel superior (luz)
    top.startNewSubPath (r.getX() + c, r.getY() + 1.0f);
    top.lineTo (r.getRight() - c, r.getY() + 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.strokePath (top, juce::PathStrokeType (1.0f));
    g.setColour (t.edge.withAlpha (0.8f));                                      // aresta interna
    g.strokePath (chamfer (r.reduced (1.5f), juce::jmax (1.0f, c - 1.0f)), juce::PathStrokeType (0.8f));
}

// parafuso de cabeca fenda (angulo varia com a posicao: nao fica "copiado")
inline void screw (juce::Graphics& g, juce::Point<float> c, float radius = 3.2f)
{
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillEllipse (c.x - radius - 0.8f, c.y - radius - 0.2f, (radius + 0.8f) * 2.0f, (radius + 0.8f) * 2.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a6e75), c.x - radius, c.y - radius,
                                             juce::Colour (0xff1c1e22), c.x + radius, c.y + radius, false));
    g.fillEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);
    const float ang = std::fmod (c.x * 0.37f + c.y * 0.11f, juce::MathConstants<float>::pi);
    const float dx = std::cos (ang) * radius * 0.75f, dy = std::sin (ang) * radius * 0.75f;
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.drawLine (c.x - dx, c.y - dy, c.x + dx, c.y + dy, 1.1f);
}

inline void screws (juce::Graphics& g, juce::Rectangle<float> r, float inset = 6.0f, float radius = 2.6f)
{
    for (auto p : { r.getTopLeft().translated (inset, inset), r.getTopRight().translated (-inset, inset),
                    r.getBottomLeft().translated (inset, -inset), r.getBottomRight().translated (-inset, -inset) })
        screw (g, p, radius);
}

// tela embutida (display) com sombra interna
inline void inset (juce::Graphics& g, juce::Rectangle<float> r, const Theme& t, float corner = 3.0f)
{
    g.setColour (t.display);
    g.fillRoundedRectangle (r, corner);
    juce::ColourGradient sh (juce::Colours::black.withAlpha (0.75f), 0.0f, r.getY(),
                             juce::Colours::transparentBlack, 0.0f, r.getY() + juce::jmin (10.0f, r.getHeight() * 0.3f), false);
    g.setGradientFill (sh);
    g.fillRoundedRectangle (r, corner);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (r, corner, 1.2f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));                       // brilho na borda inferior
    g.drawHorizontalLine ((int) r.getBottom(), r.getX() + corner, r.getRight() - corner);
}

inline void led (juce::Graphics& g, juce::Point<float> c, float radius, float amount, const Theme& t)
{
    g.setColour (juce::Colours::black);
    g.fillEllipse (c.x - radius - 1.0f, c.y - radius - 1.0f, (radius + 1.0f) * 2.0f, (radius + 1.0f) * 2.0f);
    g.setColour (t.accentDark.withAlpha (0.6f));
    g.fillEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);
    if (amount > 0.01f)
    {
        g.setColour (t.accent.withAlpha (0.25f * amount));                      // halo
        g.fillEllipse (c.x - radius * 2.6f, c.y - radius * 2.6f, radius * 5.2f, radius * 5.2f);
        g.setColour (t.accent.withAlpha (amount));
        g.fillEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.6f * amount));
        g.fillEllipse (c.x - radius * 0.4f, c.y - radius * 0.5f, radius * 0.6f, radius * 0.6f);
    }
}

// microtexto tecnico (pequeno, espacado, discreto)
inline void micro (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r, juce::Justification j,
                   juce::Colour c, float size = 9.0f)
{
    g.setColour (c);
    g.setFont (juce::FontOptions (size).withKerningFactor (0.12f));
    g.drawText (text.toUpperCase(), r, j, false);
}
} // namespace mm::hw
