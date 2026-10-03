#pragma once
#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <atomic>
#include <vector>

namespace mm
{
// =============================================================================
//  DrumPattern - 10 canais x ate 64 passos (1/16). Cada celula guarda a
//  velocity (0 = vazio). Atomico: a interface edita, o audio thread le sem travas.
//  Salvo no projeto do REAPER e nos presets do usuario.
// =============================================================================
namespace DrumRow { enum { Kick = 0, Snare, Clap, ClosedHat, OpenHat, Perc, Tom, Crash, Ride, Fx, Bass808, Count }; }

inline const juce::StringArray& drumRowNames()
{
    static const juce::StringArray n { "KICK", "SNARE", "CLAP", "HAT", "OPEN HAT", "PERC", "TOM", "CRASH", "RIDE", "FX", "808" };
    return n;
}

// Notas MIDI (padrao GM) para tocar a bateria pelo teclado
inline int drumRowForNote (int note) noexcept
{
    switch (note)
    {
        case 35: case 36: return DrumRow::Kick;
        case 38: case 40: return DrumRow::Snare;
        case 39:          return DrumRow::Clap;
        case 42: case 44: return DrumRow::ClosedHat;
        case 46:          return DrumRow::OpenHat;
        case 37: case 56: return DrumRow::Perc;
        case 41: case 43: case 45: case 47: case 48: case 50: return DrumRow::Tom;
        case 49: case 57: return DrumRow::Crash;
        case 51: case 59: return DrumRow::Ride;
        case 52: case 55: return DrumRow::Fx;
        default:          return -1;
    }
}

struct DrumPatternDef
{
    const char* name;
    const char* genre;     // METAL, METALCORE, TRAP, DUBSTEP, INDUSTRIAL, ELECTRONIC, HYBRID
    int length;            // passos de 1/16
    const char* rows[DrumRow::Count];   // '.' vazio  'o' ghost  'x' normal  'X' acento
};

class DrumPattern
{
public:
    static constexpr int numRows = DrumRow::Count;
    static constexpr int maxSteps = 64;

    std::array<std::array<std::atomic<uint8_t>, maxSteps>, numRows> cells {};
    // RATCHET: 1..4 golpes dentro do passo (rolls de trap em 1/32 e 1/64 numa grade de 1/16)
    std::array<std::array<std::atomic<uint8_t>, maxSteps>, numRows> ratchet {};
    // 808: semitons em relacao a tonica da KEY (sempre gerados dentro da escala)
    std::array<std::atomic<int8_t>, maxSteps> bassNote {};
    // 808 SLIDE: 1 = desliza da nota anterior ate esta (glide), 0 = ataque seco
    std::array<std::atomic<uint8_t>, maxSteps> slide808 {};
    // pitch dos rolls de hat: -1 desce, 0 reto, +1 sobe (movimento eletronico)
    std::array<std::atomic<int8_t>, maxSteps> hatRollPitch {};
    std::atomic<uint32_t> revision { 0 };
    juce::String name { "Init" };   // so message thread

    DrumPattern()
    {
        for (auto& r : ratchet) for (auto& c : r) c.store (1);   // 1 golpe por passo (0 seria silencio)
    }

    void touch() noexcept { revision.fetch_add (1); }

    void clear()
    {
        for (auto& r : cells) for (auto& c : r) c.store (0);
        for (auto& r : ratchet) for (auto& c : r) c.store (1);
        for (auto& n : bassNote) n.store (0);
        for (auto& n : slide808) n.store (0);
        for (auto& n : hatRollPitch) n.store (0);
        name = "Empty";
        touch();
    }

    static uint8_t velocityFor (char ch) noexcept
    {
        switch (ch) { case 'o': return 55; case 'x': return 105; case 'X': return 127; default: return 0; }
    }
    static char charFor (uint8_t v) noexcept
    {
        return v == 0 ? '.' : v < 80 ? 'o' : v < 120 ? 'x' : 'X';
    }

    juce::ValueTree toValueTree() const
    {
        juce::ValueTree t ("DRUMS");
        t.setProperty ("name", name, nullptr);
        for (int r = 0; r < numRows; ++r)
        {
            juce::String row;
            for (int s = 0; s < maxSteps; ++s)
                row << charFor (cells[(size_t) r][(size_t) s].load());
            t.setProperty ("r" + juce::String (r), row, nullptr);
            juce::String rt;
            for (int s = 0; s < maxSteps; ++s)
                rt << juce::String ((int) juce::jlimit<uint8_t> (1, 4, ratchet[(size_t) r][(size_t) s].load()));
            t.setProperty ("rt" + juce::String (r), rt, nullptr);
        }
        juce::StringArray notes;
        for (int s = 0; s < maxSteps; ++s) notes.add (juce::String ((int) bassNote[(size_t) s].load()));
        t.setProperty ("notes808", notes.joinIntoString (","), nullptr);
        juce::String sl, hp;
        for (int s = 0; s < maxSteps; ++s)
        {
            sl << (slide808[(size_t) s].load() ? '1' : '0');
            const int d = hatRollPitch[(size_t) s].load();
            hp << (d > 0 ? '+' : d < 0 ? '-' : '0');
        }
        t.setProperty ("slide808", sl, nullptr);
        t.setProperty ("hatpitch", hp, nullptr);
        return t;
    }

    bool fromValueTree (const juce::ValueTree& t)
    {
        if (! t.isValid())
            return false;
        for (int r = 0; r < numRows; ++r)
        {
            const auto row = t.getProperty ("r" + juce::String (r)).toString();
            for (int s = 0; s < maxSteps; ++s)
                cells[(size_t) r][(size_t) s].store (s < row.length() ? velocityFor ((char) row[s]) : 0);
            const auto rt = t.getProperty ("rt" + juce::String (r)).toString();   // ausente em padroes antigos = 1
            for (int s = 0; s < maxSteps; ++s)
                ratchet[(size_t) r][(size_t) s].store ((uint8_t) (s < rt.length() ? juce::jlimit (1, 4, rt.substring (s, s + 1).getIntValue()) : 1));
        }
        const auto notes = juce::StringArray::fromTokens (t.getProperty ("notes808").toString(), ",", "");
        for (int s = 0; s < maxSteps; ++s)
            bassNote[(size_t) s].store ((int8_t) (s < notes.size() ? juce::jlimit (-24, 24, notes[s].getIntValue()) : 0));
        const auto sl = t.getProperty ("slide808").toString(), hp = t.getProperty ("hatpitch").toString();
        for (int s = 0; s < maxSteps; ++s)
        {
            slide808[(size_t) s].store ((uint8_t) (s < sl.length() && sl[s] == '1' ? 1 : 0));
            hatRollPitch[(size_t) s].store ((int8_t) (s < hp.length() ? (hp[s] == '+' ? 1 : hp[s] == '-' ? -1 : 0) : 0));
        }
        name = t.getProperty ("name", "Init").toString();
        touch();
        return true;
    }
};

// ------------------------------------------------------------------ biblioteca
inline const std::vector<DrumPatternDef>& getDrumLibrary()
{
    //                     KICK                SNARE               CLAP                HAT                 OPEN HAT            PERC                TOM                 CRASH               RIDE                FX
    static const std::vector<DrumPatternDef> lib
    {
        { "Basic Rock", "METAL", 16, { "x.......x.x.....", "....x.......x...", "", "x.x.x.x.x.x.x.x.", "", "", "", "x...............", "", "" } },
        { "Double Kick Groove", "METAL", 16, { "xxxxxxxxxxxxxxxx", "....X.......X...", "", "", "", "", "", "x...............", "x.x.x.x.x.x.x.x.", "" } },
        { "Metal Gallop", "METAL", 16, { "x.xxx.xxx.xxx.xx", "....X.......X...", "", "", "", "", "", "X...............", "x.x.x.x.x.x.x.x.", "" } },
        { "Blast Beat", "METAL", 16, { "x.x.x.x.x.x.x.x.", ".x.x.x.x.x.x.x.x", "", "", "", "", "", "X...............", "xxxxxxxxxxxxxxxx", "" } },

        { "Heavy Half Time", "METALCORE", 16, { "x.....x...x.....", "........X.......", "", "x.x.x.x.x.x.x.x.", "", "", "", "X...............", "", "" } },
        { "Breakdown Chug", "METALCORE", 16, { "X..x..X..x..X...", "........X.......", "", "", "", "", "", "X.......X.......", "", "" } },
        { "Dark Half Time", "METALCORE", 32, { "x.......x.x.....x.....x.x.......", "........X...............X.......", "", "x.x.x.x.x.x.x...x.x.x.x.x.x.x...", "..............x...............x.", "", "", "X...............................", "", "" } },
        { "Half-Time Destroy", "METALCORE", 32, { "X..X..X.........X..X..X.X.......", "........X...............X.......", "", "", "", "", "............................xxxx", "X.......X.......X.......X.......", "", "" } },

        { "Trap 01", "TRAP", 32, { "x......x..x.....x.....x...x.....", "........X...............X.......", "........x...............x.......", "x.x.x.x.xxx.x.x.x.x.x.x.x.xxxxxx", "..............x.................", "", "", "", "", "" } },
        { "Dark Trap", "TRAP", 32, { "x.........x.....x..x......x.....", "........X...............X.......", "........x...............x.......", "x.x.x.xxx.x.x.x.x.x.x.xxx.x.xxxx", "", "...x.......x...........x........", "", "", "", "" } },
        { "Metal Trap", "TRAP", 32, { "x..x......x.x...x..x......xxxx..", "........X...............X.......", "", "xxx.x.x.xxx.x.x.xxx.x.x.xxxxxxxx", "", "", "", "X...............................", "", "..............................x." } },
        { "Trap Metal", "TRAP", 32, { "xxxx..x...x.x...xxxx..x...xx.x..", "........X...............X.....X.", "", "x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.", "......x...............x.........", "", "", "X...............X...............", "", "" } },

        { "Dubstep 01", "DUBSTEP", 32, { "x.................x.x...........", "................X...............", "", "x...x...x...x...x...x...x...x...", "..x...x...x...x...x...x...x...x.", "", "", "", "", "" } },
        { "Heavy Dubstep", "DUBSTEP", 32, { "x.....x...x.......x.x......x....", "................X.............X.", "................x...............", "x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.", "", "", "", "X...............................", "", "..............x................." } },

        { "Industrial Stomp", "INDUSTRIAL", 16, { "x..x..x.x..x..x.", "....X.......X...", "....x.......x...", "xxxxxxxxxxxxxxxx", "", "x.x.x.x.x.x.x.x.", "..............xx", "", "", "" } },
        { "Electronic Metal", "ELECTRONIC", 16, { "x...x...x...x...", "....x.......x...", "", "x.x.x.x.x.x.x.x.", "..x...x...x...x.", "", "", "", "", "" } },
        { "Hybrid Groove", "HYBRID", 16, { "x..x...xx..x....", "....X..o....X..o", "", "x.xxx.xxx.xxx.xx", "", "", "", "x...............", "", "" } },
    };
    return lib;
}

inline int findDrumPattern (const juce::String& name)
{
    const auto& lib = getDrumLibrary();
    for (int i = 0; i < (int) lib.size(); ++i)
        if (name == lib[(size_t) i].name)
            return i;
    return -1;
}

inline int applyDrumPattern (DrumPattern& p, const DrumPatternDef& def)
{
    for (int r = 0; r < DrumPattern::numRows; ++r)
    {
        const juce::String row (def.rows[r] != nullptr ? def.rows[r] : "");
        for (int s = 0; s < DrumPattern::maxSteps; ++s)
            p.cells[(size_t) r][(size_t) s].store (s < row.length() ? DrumPattern::velocityFor ((char) row[s]) : 0);
    }
    for (auto& r : p.ratchet) for (auto& c : r) c.store (1);
    for (auto& n : p.bassNote) n.store (0);
    for (auto& n : p.slide808) n.store (0);
    for (auto& n : p.hatRollPitch) n.store (0);
    p.name = def.name;
    p.touch();
    return def.length;
}

inline void setInitDrums (DrumPattern& p)
{
    applyDrumPattern (p, getDrumLibrary()[0]);
}
} // namespace mm
