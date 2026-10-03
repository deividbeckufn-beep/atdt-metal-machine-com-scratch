#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MetalLookAndFeel.h"
#include "Hardware.h"
#include "MIDI/MidiLearn.h"
#include "Synth/ModulationDefs.h"
#include "Parameters/Parameters.h"

namespace mm
{
using APVTS = juce::AudioProcessorValueTreeState;

// -----------------------------------------------------------------------------
// Slider rotativo: arrastar = ajuste normal | SHIFT + arrastar = ajuste fino
// duplo clique = valor padrao | roda do mouse = ajuste
// -----------------------------------------------------------------------------
class FineSlider : public juce::Slider
{
public:
    FineSlider() : juce::Slider (RotaryHorizontalVerticalDrag, TextBoxBelow) {}

    std::function<void()> onRightClick;

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            if (onRightClick) onRightClick();
            return;
        }
        lastPos = e.position;
        dragProportion = valueToProportionOfLength (getValue());
        juce::Slider::mouseDown (e);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu())
            juce::Slider::mouseUp (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! isEnabled() || e.mods.isPopupMenu())
            return;

        const float delta = (e.position.x - lastPos.x) + (lastPos.y - e.position.y);
        lastPos = e.position;
        const double sensitivity = e.mods.isShiftDown() ? 1.0 / 1500.0 : 1.0 / 220.0;
        dragProportion = juce::jlimit (0.0, 1.0, dragProportion + delta * sensitivity);
        setValue (proportionOfLengthToValue (dragProportion), juce::sendNotificationSync);
    }

private:
    juce::Point<float> lastPos;
    double dragProportion = 0.0;
};

// -----------------------------------------------------------------------------
// Knob ligado a um parametro.
//  RIGHT CLICK = MIDI Learn / remover CC / remover modulacoes / valor padrao
//  Anel externo = faixa de modulacao real (soma dos slots da matriz neste destino)
//  Aceita fontes arrastadas (LFO, ENV, macros...) -> cria o slot na matriz
// -----------------------------------------------------------------------------
class Knob : public juce::Component, public juce::DragAndDropTarget, private juce::Timer
{
public:
    Knob (APVTS& s, const juce::String& paramID, const juce::String& name, bool bipolar = false)
        : state (s)
    {
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 14);
        slider.getProperties().set ("bipolar", bipolar);
        addAndMakeVisible (slider);

        label.setText (name.toUpperCase(), juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);

        attachment = std::make_unique<APVTS::SliderAttachment> (state, paramID, slider);
        if (auto* p = state.getParameter (paramID))
        {
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
            slider.setTooltip (p->getName (64));
            parameter = p;
        }

        if (auto* provider = dynamic_cast<MidiLearnProvider*> (&state.processor))
            midiLearn = &provider->getMidiLearn();

        dest = destForParamID (paramID);
        if (dest != ModDest::None)
            startTimerHz (10);

        slider.onRightClick = [this] { showContextMenu(); };
    }

    ~Knob() override { stopTimer(); }

    void setName (const juce::String& n) { label.setText (n.toUpperCase(), juce::dontSendNotification); }

    void paintOverChildren (juce::Graphics& g) override
    {
        const auto& t = themeOf (*this);
        paintModRing (g, t);

        if (dragHover)
        {
            g.setColour (t.accent);
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 4.0f, 1.5f);
        }

        if (midiLearn == nullptr || parameter == nullptr)
            return;

        const int idx = parameter->getParameterIndex();
        if (midiLearn->isLearning (idx))
        {
            g.setColour (t.accent.withAlpha (blink ? 0.9f : 0.25f));
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 4.0f, 1.5f);
            g.setFont (juce::FontOptions (10.0f));
            g.drawText ("LEARN", getLocalBounds().removeFromTop (12), juce::Justification::centredRight);
        }
        else if (const int cc = midiLearn->getCCFor (idx); cc >= 0)
        {
            g.setColour (t.accentSoft);
            g.setFont (juce::FontOptions (9.0f));
            g.drawText ("CC" + juce::String (cc), getLocalBounds().removeFromTop (12), juce::Justification::centredRight);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (13));
        slider.setBounds (r);
    }

    // ---- arrastar-e-soltar de fontes de modulacao ----
    bool isInterestedInDragSource (const SourceDetails& d) override
    {
        return dest != ModDest::None && d.description.toString().startsWith ("modsrc:");
    }
    void itemDragEnter (const SourceDetails&) override { dragHover = true;  repaint(); }
    void itemDragExit  (const SourceDetails&) override { dragHover = false; repaint(); }
    void itemDropped (const SourceDetails& d) override
    {
        dragHover = false;
        const int src = d.description.toString().fromFirstOccurrenceOf (":", false, false).getIntValue();
        if (! assignModulation (state, src, (int) dest))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "MOD MATRIX",
                                                    "Os 8 slots da matriz estao ocupados.\nRemova um slot na aba MOD.");
        repaint();
    }

    // Cria (ou reaproveita) um slot src -> dst. Retorna false se a matriz estiver cheia.
    static bool assignModulation (APVTS& s, int src, int dst)
    {
        auto get = [&s] (int slot, const char* n) { return s.getParameter (ParamIDs::mod (slot, n)); };
        auto setReal = [] (juce::RangedAudioParameter* p, float v)
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (v));
            p->endChangeGesture();
        };

        for (int i = 1; i <= numModSlots; ++i)
            if ((int) get (i, "src")->convertFrom0to1 (get (i, "src")->getValue()) == src
                && (int) get (i, "dst")->convertFrom0to1 (get (i, "dst")->getValue()) == dst)
                return true;   // ja existe

        for (int i = 1; i <= numModSlots; ++i)
            if ((int) get (i, "src")->convertFrom0to1 (get (i, "src")->getValue()) == 0
                && (int) get (i, "dst")->convertFrom0to1 (get (i, "dst")->getValue()) == 0)
            {
                setReal (get (i, "src"), (float) src);
                setReal (get (i, "dst"), (float) dst);
                setReal (get (i, "amt"), 0.5f);
                setReal (get (i, "curve"), 0.0f);
                setReal (get (i, "mode"), 0.0f);
                return true;
            }
        return false;
    }

    static void clearSlot (APVTS& s, int slot)
    {
        for (auto* n : { "src", "dst", "amt", "curve", "mode" })
            if (auto* p = s.getParameter (ParamIDs::mod (slot, n)))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->getDefaultValue());
                p->endChangeGesture();
            }
        // o slot 1 tem padrao MW -> Cutoff; "limpar" deve zerar de verdade
        for (auto* n : { "src", "dst" })
            if (auto* p = s.getParameter (ParamIDs::mod (slot, n)))
                p->setValueNotifyingHost (0.0f);
    }

private:
    struct SlotRef { std::atomic<float>* src; std::atomic<float>* dst; std::atomic<float>* amt; std::atomic<float>* mode; };

    SlotRef slotRef (int i) const
    {
        return { state.getRawParameterValue (ParamIDs::mod (i, "src")), state.getRawParameterValue (ParamIDs::mod (i, "dst")),
                 state.getRawParameterValue (ParamIDs::mod (i, "amt")), state.getRawParameterValue (ParamIDs::mod (i, "mode")) };
    }

    // soma das profundidades (positiva e negativa) de todos os slots que apontam para este knob
    void modRange (float& up, float& down) const
    {
        up = down = 0.0f;
        for (int i = 1; i <= numModSlots; ++i)
        {
            const auto r = slotRef (i);
            const int src = (int) r.src->load();
            if (src == 0 || (int) r.dst->load() != (int) dest)
                continue;
            const float a = r.amt->load();
            if (isBipolarSource (src)) { up += std::abs (a); down += std::abs (a); }
            else if (a >= 0.0f)        up += a;
            else                       down += -a;
        }
    }

    void paintModRing (juce::Graphics& g, const Theme& t)
    {
        if (dest == ModDest::None || parameter == nullptr)
            return;

        float up, down;
        modRange (up, down);
        if (up <= 0.0f && down <= 0.0f)
            return;

        const auto rp = slider.getRotaryParameters();
        auto area = slider.getBounds().withTrimmedBottom (14).toFloat();
        const float radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
        const auto c = area.getCentre();
        const float span = destKnobSpan (dest);
        const float pos = parameter->getValue();
        auto angleOf = [&] (float p) { return rp.startAngleRadians + juce::jlimit (0.0f, 1.0f, p) * (rp.endAngleRadians - rp.startAngleRadians); };

        juce::Path ring;
        ring.addCentredArc (c.x, c.y, radius + 0.5f, radius + 0.5f, 0.0f,
                            angleOf (pos - down * span), angleOf (pos + up * span), true);
        g.setColour (t.accentSoft.withAlpha (0.85f));
        g.strokePath (ring, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    float modHash() const
    {
        float h = 0.0f;
        for (int i = 1; i <= numModSlots; ++i)
        {
            const auto r = slotRef (i);
            h += (float) i * (r.src->load() * 7.0f + r.dst->load() * 131.0f + r.amt->load() * 3.1f + r.mode->load());
        }
        return h;
    }

    void showContextMenu()
    {
        if (parameter == nullptr)
            return;

        const int idx = parameter->getParameterIndex();
        juce::PopupMenu m;
        m.addSectionHeader (parameter->getName (40));

        if (midiLearn != nullptr)
        {
            if (midiLearn->isLearning (idx))
                m.addItem ("Cancelar MIDI Learn", [this] { midiLearn->cancelLearning(); repaint(); });
            else
                m.addItem ("MIDI Learn", [this, idx] { midiLearn->startLearning (idx); startTimerHz (10); repaint(); });

            const int cc = midiLearn->getCCFor (idx);
            m.addItem ("Remover MIDI (CC " + (cc >= 0 ? juce::String (cc) : juce::String ("-")) + ")", cc >= 0,
                       false, [this, idx] { midiLearn->clearMapping (idx); repaint(); });
        }

        if (dest != ModDest::None)
        {
            juce::PopupMenu mods;
            for (int i = 1; i <= numModSlots; ++i)
            {
                const auto r = slotRef (i);
                const int src = (int) r.src->load();
                if (src == 0 || (int) r.dst->load() != (int) dest)
                    continue;
                mods.addItem ("Remover " + modSourceNames()[src] + " ("
                                  + juce::String (juce::roundToInt (r.amt->load() * 100.0f)) + "%)",
                              [this, i] { clearSlot (state, i); repaint(); });
            }
            m.addSeparator();
            m.addSubMenu ("Modulacoes", mods, mods.getNumItems() > 0);
        }

        m.addSeparator();
        m.addItem ("Voltar ao padrao", [this]
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->getDefaultValue());
            parameter->endChangeGesture();
        });

        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&slider));
    }

    void timerCallback() override
    {
        const bool learning = midiLearn != nullptr && parameter != nullptr
                              && midiLearn->isLearning (parameter->getParameterIndex());
        bool dirty = false;

        if (learning)          { if (++tick % 3 == 0) { blink = ! blink; dirty = true; } }
        else if (wasLearning)  dirty = true;
        wasLearning = learning;

        if (dest != ModDest::None)
        {
            const float h = modHash();
            if (std::abs (h - lastHash) > 1.0e-6f) { lastHash = h; dirty = true; }
        }
        else if (! learning)
        {
            stopTimer();
        }

        if (dirty)
            repaint();
    }

    APVTS& state;
    juce::RangedAudioParameter* parameter = nullptr;
    MidiLearn* midiLearn = nullptr;
    ModDest dest = ModDest::None;
    bool blink = false, wasLearning = false, dragHover = false;
    int tick = 0;
    float lastHash = -1.0f;

    FineSlider slider;
    juce::Label label;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// -----------------------------------------------------------------------------
// Fonte de modulacao arrastavel ("chip"): solte sobre um knob para modular.
// -----------------------------------------------------------------------------
class ModSourceChip : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ModSourceChip (int sourceIndex) : source (sourceIndex)
    {
        setTooltip ("Arraste " + modSourceNames()[source] + " sobre um knob para modular");
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = themeOf (*this);
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        // chip = pequeno conector de patch: grafite; acende em vermelho so no hover/arraste
        g.setColour (hover ? t.accent.withAlpha (0.18f) : t.display);
        g.fillRoundedRectangle (r, 2.5f);
        g.setColour (hover ? t.accent : juce::Colours::black);
        g.drawRoundedRectangle (r, 2.5f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawHorizontalLine ((int) r.getBottom() - 1, r.getX() + 2.0f, r.getRight() - 2.0f);
        g.setColour (hover ? t.text : t.textDim.brighter (0.2f));
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (modSourceShortNames()[source], getLocalBounds(), juce::Justification::centred);
    }

    void mouseEnter (const juce::MouseEvent&) override { hover = true;  repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hover = false; repaint(); }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() < 4)
            return;
        if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
            if (! c->isDragAndDropActive())
                c->startDragging ("modsrc:" + juce::String (source), this);
    }

private:
    int source;
    bool hover = false;
};

class ChoiceBox : public juce::Component
{
public:
    ChoiceBox (APVTS& state, const juce::String& paramID, const juce::String& name = {})
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramID)))
            box.addItemList (p->choices, 1);
        box.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (box);

        if (name.isNotEmpty())
        {
            label.setText (name.toUpperCase(), juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (label);
        }
        attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, paramID, box);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        if (label.isVisible())
            label.setBounds (r.removeFromTop (13));
        box.setBounds (r.withSizeKeepingCentre (r.getWidth(), juce::jmin (22, r.getHeight())));
    }

private:
    juce::ComboBox box;
    juce::Label label;
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

// -----------------------------------------------------------------------------
// Painel com barra de titulo
// -----------------------------------------------------------------------------
class Panel : public juce::Component
{
public:
    explicit Panel (juce::String panelTitle) : title (std::move (panelTitle)) {}

    // placa de hardware: metal escovado chanfrado, parafusos nos cantos, titulo gravado
    void paint (juce::Graphics& g) override
    {
        const auto& t = themeOf (*this);
        auto r = getLocalBounds().toFloat().reduced (1.0f, 1.0f).withTrimmedBottom (1.0f);
        hw::plate (g, r, t, 8.0f);

        auto bar = getTitleBar();
        // canaleta do titulo (ranhura gravada)
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawHorizontalLine (bar.getBottom() + 3, r.getX() + 10.0f, r.getRight() - 10.0f);
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.drawHorizontalLine (bar.getBottom() + 4, r.getX() + 10.0f, r.getRight() - 10.0f);

        g.setColour (t.accent);
        g.fillRect (bar.getX() + 6, bar.getCentreY() - 4, 2, 8);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold).withKerningFactor (0.10f));
        g.setColour (t.text.withAlpha (0.92f));
        g.drawText (title.toUpperCase(), bar.withTrimmedLeft (14), juce::Justification::centredLeft);

        if (r.getWidth() > 90.0f && r.getHeight() > 60.0f)
            hw::screws (g, r, 5.5f, 2.2f);
    }

    juce::Rectangle<int> getTitleBar() const    { return getLocalBounds().removeFromTop (30).reduced (12, 5); }
    juce::Rectangle<int> getContentArea() const { return getLocalBounds().reduced (8).withTrimmedTop (24); }

    // distribui componentes lado a lado com larguras iguais
    static void layoutRow (juce::Rectangle<int> area, std::initializer_list<juce::Component*> items)
    {
        const int w = area.getWidth() / (int) items.size();
        for (auto* c : items)
            c->setBounds (area.removeFromLeft (w).reduced (2, 0));
    }

private:
    juce::String title;
};
} // namespace mm
