#pragma once
#include <juce_graphics/juce_graphics.h>

namespace mm
{
// =============================================================================
//  Sistema de temas. METAL = tema oficial (preto + vermelho neon).
//  Para criar um tema novo: copie um bloco abaixo e registre em all().
// =============================================================================
struct Theme
{
    juce::String name;
    juce::Colour background, panel, panelRaised, edge, track, display,
                 accent, accentSoft, accentDark, text, textDim;

    static const Theme& metal()
    {
        // ATDT AUDIO TOOLS: black industrial + dark steel + neon red (vermelho so funcional)
        static const Theme t { "METAL",
            juce::Colour (0xff090a0c), juce::Colour (0xff15171a), juce::Colour (0xff202328),
            juce::Colour (0xff34373d), juce::Colour (0xff24272c), juce::Colour (0xff060708),
            juce::Colour (0xffff2030), juce::Colour (0xffff5a64), juce::Colour (0xff5c0a12),
            juce::Colour (0xffd9dce1), juce::Colour (0xff7a8089) };
        return t;
    }

    static const Theme& cyber()
    {
        static const Theme t { "CYBER",
            juce::Colour (0xff04070a), juce::Colour (0xff0c1116), juce::Colour (0xff131a21),
            juce::Colour (0xff16303a), juce::Colour (0xff222a31), juce::Colour (0xff060a0d),
            juce::Colour (0xff00d9ff), juce::Colour (0xff66e8ff), juce::Colour (0xff04506a),
            juce::Colour (0xffe4eef2), juce::Colour (0xff7a8a94) };
        return t;
    }

    static juce::Array<const Theme*> all() { return { &metal(), &cyber() }; }

    static const Theme& byName (const juce::String& n)
    {
        for (auto* t : all())
            if (t->name == n)
                return *t;
        return metal();
    }
};
} // namespace mm
