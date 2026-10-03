#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

namespace mm
{
class MetalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MetalLookAndFeel();

    void setTheme (const Theme& t);
    const Theme& getTheme() const noexcept { return *theme; }

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool isMouseOver, bool isButtonDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool isMouseOver, bool isButtonDown) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;

private:
    const Theme* theme = &Theme::metal();
};

// Tema do componente (cada instancia do plugin pode ter seu tema)
inline const Theme& themeOf (const juce::Component& c)
{
    if (auto* lf = dynamic_cast<MetalLookAndFeel*> (&c.getLookAndFeel()))
        return lf->getTheme();
    return Theme::metal();
}

// Brilho "neon": traco largo transparente + traco fino solido
inline void strokeNeon (juce::Graphics& g, const juce::Path& p, juce::Colour c, float width = 2.0f)
{
    g.setColour (c.withAlpha (0.16f));
    g.strokePath (p, juce::PathStrokeType (width * 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c.withAlpha (0.35f));
    g.strokePath (p, juce::PathStrokeType (width * 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
} // namespace mm
