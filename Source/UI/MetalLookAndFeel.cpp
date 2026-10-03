#include "MetalLookAndFeel.h"
#include "Hardware.h"

using namespace juce;

namespace mm
{
MetalLookAndFeel::MetalLookAndFeel() { setTheme (Theme::metal()); }

void MetalLookAndFeel::setTheme (const Theme& t)
{
    theme = &t;
    setColour (Slider::textBoxTextColourId,        t.text.withAlpha (0.8f));
    setColour (Slider::textBoxOutlineColourId,     Colours::black);          // mini-display escuro
    setColour (Label::outlineColourId,             Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId,  t.display);
    setColour (Slider::textBoxHighlightColourId,   t.accent.withAlpha (0.4f));
    setColour (Label::textColourId,                t.textDim);
    setColour (ScrollBar::thumbColourId,           t.accent.withAlpha (0.7f));
    setColour (ScrollBar::trackColourId,           t.track);
    setColour (Slider::trackColourId,              t.accent.withAlpha (0.55f));
    setColour (Slider::backgroundColourId,         t.display);
    setColour (TextEditor::outlineColourId,        t.edge);
    setColour (TextEditor::focusedOutlineColourId, t.accent);
    setColour (ComboBox::backgroundColourId,       t.display);
    setColour (ComboBox::textColourId,             t.text);
    setColour (ComboBox::outlineColourId,          t.edge);
    setColour (ComboBox::arrowColourId,            t.accent);
    setColour (PopupMenu::backgroundColourId,      t.panelRaised);
    setColour (PopupMenu::textColourId,            t.text);
    setColour (PopupMenu::headerTextColourId,      t.accent);
    setColour (PopupMenu::highlightedBackgroundColourId, t.accent);
    setColour (PopupMenu::highlightedTextColourId, t.background);
    setColour (TextButton::buttonColourId,         t.panelRaised);
    setColour (TextButton::buttonOnColourId,       t.accent);
    setColour (TextButton::textColourOffId,        t.text);
    setColour (TextButton::textColourOnId,         t.background);
    setColour (TextEditor::backgroundColourId,     t.display);
    setColour (TextEditor::textColourId,           t.text);
    setColour (TextEditor::highlightColourId,      t.accent.withAlpha (0.4f));
    setColour (CaretComponent::caretColourId,      t.accent);
}

void MetalLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int width, int height,
                                         float pos, float startAngle, float endAngle, Slider& slider)
{
    // KNOB DE HARDWARE: sombra, anel serrilhado, escala gravada, tampa metalica, arco neon
    const auto& t = getTheme();
    auto bounds = Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
    const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    if (radius < 6.0f) return;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool bipolar = (bool) slider.getProperties().getWithDefault ("bipolar", false);
    const bool hot = slider.isMouseOverOrDragging() && slider.isEnabled();
    const float arcR = radius - 1.5f;

    // escala gravada (marcas)
    g.setColour (t.textDim.withAlpha (0.35f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle) - MathConstants<float>::halfPi;
        const float r1 = arcR + 0.5f, r0 = arcR - (i % 5 == 0 ? 3.0f : 1.8f);
        g.drawLine (c.x + std::cos (a) * r0, c.y + std::sin (a) * r0, c.x + std::cos (a) * r1, c.y + std::sin (a) * r1, 0.8f);
    }

    // trilho + arco de valor (neon)
    Path track;
    track.addCentredArc (c.x, c.y, arcR - 4.0f, arcR - 4.0f, 0.0f, startAngle, endAngle, true);
    g.setColour (Colours::black.withAlpha (0.8f));
    g.strokePath (track, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::butt));
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.01f)
    {
        Path arc;
        arc.addCentredArc (c.x, c.y, arcR - 4.0f, arcR - 4.0f, 0.0f, jmin (from, angle), jmax (from, angle), true);
        strokeNeon (g, arc, slider.isEnabled() ? t.accent : t.textDim, 2.0f);
    }

    // corpo: sombra, anel serrilhado, tampa
    const float bodyR = arcR - 7.0f;
    if (bodyR < 4.0f) return;
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillEllipse (c.x - bodyR - 1.0f, c.y - bodyR + 1.5f, (bodyR + 1.0f) * 2.0f, (bodyR + 1.0f) * 2.0f);
    g.setGradientFill (ColourGradient (Colour (0xff3a3d43), c.x, c.y - bodyR, Colour (0xff101113), c.x, c.y + bodyR, false));
    g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (Colours::black.withAlpha (0.55f));                               // serrilhado
    const int knurls = jmax (12, (int) (bodyR * 1.6f));
    for (int i = 0; i < knurls; ++i)
    {
        const float a = (float) i / (float) knurls * MathConstants<float>::twoPi;
        g.drawLine (c.x + std::cos (a) * bodyR * 0.82f, c.y + std::sin (a) * bodyR * 0.82f,
                    c.x + std::cos (a) * bodyR, c.y + std::sin (a) * bodyR, 0.7f);
    }
    const float capR = bodyR * 0.74f;                                             // tampa (luz de cima)
    g.setGradientFill (ColourGradient (Colour (0xff4a4e55), c.x - capR * 0.4f, c.y - capR,
                                       Colour (0xff17191c), c.x + capR * 0.3f, c.y + capR, true));
    g.fillEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);
    g.setColour (Colours::white.withAlpha (0.10f));
    g.drawEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f, 0.8f);
    if (hot)
    {
        g.setColour (t.accent.withAlpha (0.10f));
        g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    }

    // ponteiro (vermelho quando em uso)
    Path pointer;
    pointer.addRoundedRectangle (-1.1f, -bodyR + 1.5f, 2.2f, bodyR * 0.55f, 1.0f);
    g.setColour (hot ? t.accentSoft : t.text.withAlpha (0.85f));
    g.fillPath (pointer, AffineTransform::rotation (angle).translated (c.x, c.y));
}

void MetalLookAndFeel::drawComboBox (Graphics& g, int width, int height, bool, int, int, int, int, ComboBox& box)
{
    // seletor = display embutido com seta vermelha
    const auto& t = getTheme();
    auto r = Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
    hw::inset (g, r, t, 2.5f);
    if (box.hasKeyboardFocus (true) || box.isMouseOver (true))
    {
        g.setColour (t.accent.withAlpha (0.45f));
        g.drawRoundedRectangle (r, 2.5f, 1.0f);
    }
    Path arrow;
    const float ax = (float) width - 11.0f, ay = (float) height * 0.5f;
    arrow.addTriangle (ax - 3.5f, ay - 1.5f, ax + 3.5f, ay - 1.5f, ax, ay + 2.5f);
    g.setColour (t.accent);
    g.fillPath (arrow);
}

void MetalLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    // CHAVE FISICA: metal escuro com bisel; ligada = LED vermelho + borda acesa
    const auto& t = getTheme();
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.0f), 2.5f);
    g.setGradientFill (ColourGradient (down ? t.panel : t.panelRaised.brighter (over ? 0.12f : 0.0f), 0.0f, r.getY(),
                                       t.panel.darker (0.4f), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, 2.5f);
    g.setColour (Colours::white.withAlpha (down ? 0.02f : 0.08f));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 3.0f, r.getRight() - 3.0f);
    g.setColour (on ? t.accent.withAlpha (0.85f) : Colours::black);
    g.drawRoundedRectangle (r, 2.5f, on ? 1.2f : 1.0f);
    if (on)
    {
        // faixa de LED no topo
        auto ledBar = r.withHeight (2.0f).reduced (r.getWidth() * 0.22f, 0.0f).translated (0.0f, 2.0f);
        g.setColour (t.accent.withAlpha (0.25f));
        g.fillRect (ledBar.expanded (2.0f, 1.5f));
        g.setColour (t.accent);
        g.fillRect (ledBar);
    }
}

void MetalLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    const auto& t = getTheme();
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.getToggleState() ? t.accentSoft : (b.isEnabled() ? t.text.withAlpha (0.85f) : t.textDim.withAlpha (0.5f)));
    g.drawText (b.getButtonText(), b.getLocalBounds().reduced (3, 0).translated (0, b.getToggleState() ? 1 : 0),
                Justification::centred, true);
}

Font MetalLookAndFeel::getTextButtonFont (TextButton&, int h) { return Font (FontOptions (jmin (12.0f, (float) h * 0.5f), Font::bold)); }
Font MetalLookAndFeel::getComboBoxFont (ComboBox&)             { return Font (FontOptions (12.0f, Font::bold)); }
Font MetalLookAndFeel::getLabelFont (Label& l)                 { return Font (FontOptions ((float) jmin (11, l.getHeight() - 2))); }
Font MetalLookAndFeel::getPopupMenuFont()                      { return Font (FontOptions (13.0f)); }
// Barra linear: se "bipolar", preenche a partir do centro (ex.: AMOUNT da matriz)
void MetalLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int width, int height, float sliderPos,
                                         float minSliderPos, float maxSliderPos, Slider::SliderStyle style, Slider& slider)
{
    if (style != Slider::LinearBar)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }

    const auto& t = getTheme();
    auto r = Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    g.setColour (t.display);
    g.fillRoundedRectangle (r, 3.0f);

    const bool bipolar = (bool) slider.getProperties().getWithDefault ("bipolar", false);
    const float from = bipolar ? r.getCentreX() : r.getX();
    auto fill = Rectangle<float>::leftTopRightBottom (jmin (from, sliderPos), r.getY() + 2.0f,
                                                      jmax (from, sliderPos), r.getBottom() - 2.0f);
    g.setColour (t.accent.withAlpha (0.55f));
    g.fillRect (fill);
    if (bipolar)
    {
        g.setColour (t.textDim.withAlpha (0.6f));
        g.drawVerticalLine (roundToInt (r.getCentreX()), r.getY() + 2.0f, r.getBottom() - 2.0f);
    }
    g.setColour (t.edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
}
// Caixa de valor dos knobs = mini-display embutido (independe das cores copiadas na criacao)
void MetalLookAndFeel::drawLabel (Graphics& g, Label& label)
{
    if (dynamic_cast<Slider*> (label.getParentComponent()) == nullptr || label.isBeingEdited())
    {
        LookAndFeel_V4::drawLabel (g, label);
        return;
    }
    const auto& t = getTheme();
    auto r = label.getLocalBounds().toFloat().reduced (1.0f, 0.5f);
    hw::inset (g, r, t, 2.0f);
    g.setColour (t.text.withAlpha (0.82f));
    g.setFont (FontOptions (10.5f).withKerningFactor (0.03f));
    g.drawText (label.getText(), r.toNearestInt(), Justification::centred, true);
}
} // namespace mm
