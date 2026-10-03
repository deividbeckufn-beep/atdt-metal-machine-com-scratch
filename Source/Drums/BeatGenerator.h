#pragma once
#include "DrumPattern.h"
#include "Sequencer/Scale.h"
#include <juce_core/juce_core.h>

namespace mm
{
// =============================================================================
//  BeatGenerator - gerador de beats por REGRAS de estilo (nao e sorteio solto).
//  Cada estilo tem uma "gramatica": pesos de posicao do kick, feel (full/half),
//  hats/ride, camadas eletronicas, rolls de trap e 808 na escala.
//  Deterministico: mesmo ajuste + mesma seed = mesmo beat. Roda no message thread.
// =============================================================================
namespace Beat
{
    enum Style { ModernMetal = 0, Metalcore, ElectronicMetal, Industrial, NuMetal, TrapMetal,
                 Electronicore, DubstepMetal, HalfTime, DarkElectronic, CinematicMetal, Hybrid,
                 // v0.11: familias TRAP e HYBRID modernas
                 ModernTrap, DarkTrap, ModernElectronicore, ModernTrapRock, IndustrialMetalcore, Trapcore, NumStyles };
    enum Section { Intro = 0, Verse, PreChorus, Chorus, Breakdown, Bridge, Outro, PreDrop, Drop, NumSections };

    inline const juce::StringArray& styleNames()
    {
        static const juce::StringArray n { "Modern Metal", "Metalcore", "Electronic Metal", "Industrial", "Nu Metal",
                                           "Trap Metal", "Electronicore", "Dubstep Metal", "Half-Time", "Dark Electronic",
                                           "Cinematic Metal", "Hybrid", "Modern Trap", "Dark Trap", "Modern Electronicore",
                                           "Modern Trap Rock", "Industrial Metalcore", "Trapcore" };
        return n;
    }
    inline const juce::StringArray& sectionNames()
    {
        static const juce::StringArray n { "Intro", "Verse", "Pre-Chorus", "Chorus", "Breakdown", "Bridge", "Outro", "Pre-Drop", "Drop" };
        return n;
    }
}

struct BeatSettings
{
    int style = Beat::Metalcore, section = Beat::Verse, variation = 0;   // variacao A/B/C/D = 0..3
    float energy = 0.6f, density = 0.5f, aggression = 0.5f, electronic = 0.3f, trap = 0.0f, glitch = 0.0f;
    // v0.11: MODERNITY (tradicional -> hibrido contemporaneo), SYNCOPATION, SPACE, COMPLEXITY
    float modernity = 0.3f, syncopation = 0.3f, space = 0.2f, complexity = 0.4f;
    int halfTime = 0;      // 0 Auto (pelo estilo/secao), 1 Full Time, 2 Half Time
    int doubleKick = 0;    // 0 Off, 1 Light, 2 Medium, 3 Heavy, 4 Extreme
    int bars = 2;          // 1 ou 2 compassos (16 / 32 passos de 1/16)
    int root = 4, scale = 0;
    int seed = 1;
    uint32_t locks = 0;    // bit por canal: canal travado nao e alterado
};

class BeatGenerator
{
public:
    // Gera o beat inteiro (respeitando canais travados). Retorna o comprimento em passos.
    // Algoritmo v0.11: ESPACO + GROOVE + VELOCITY + SINCOPE + SUB/808 + MICRO-FILLS + TRANSICOES + CONTRASTE
    static int generate (DrumPattern& p, const BeatSettings& in)
    {
        BeatSettings s = in;
        if (s.variation == 1) s.seed += 7919;
        if (s.variation == 2) { s.density = juce::jmin (1.0f, s.density + 0.15f); s.energy = juce::jmin (1.0f, s.energy + 0.1f); }
        juce::Random rng ((juce::int64) s.seed * 104729 + s.style * 131 + s.section * 17);
        auto chance = [&] (float pr) { return rng.nextFloat() < juce::jlimit (0.0f, 1.0f, pr); };

        const auto prof = profile (s.style);
        const float mod = s.modernity;
        // MODERNITY puxa o beat para o hibrido contemporaneo (trap, eletronico, glitch, sincope)
        const float tr  = juce::jlimit (0.0f, 1.0f, s.trap + prof.trap + 0.35f * mod * (1.0f - s.trap));
        const float el  = juce::jlimit (0.0f, 1.0f, s.electronic + prof.electronic + 0.3f * mod);
        const float gl  = juce::jlimit (0.0f, 1.0f, s.glitch + prof.glitch + 0.2f * mod);
        const float syn = juce::jlimit (0.0f, 1.0f, s.syncopation + 0.25f * mod);
        const float cx  = s.complexity;
        const int sec = s.section;
        // SPACE: verso deixa espaco para a voz; breakdown usa o silencio; refrao/drop mais cheios
        const float space = juce::jlimit (0.0f, 1.0f, s.space + (sec == Beat::Verse ? 0.15f : 0.0f)
                                                   + (sec == Beat::Breakdown ? 0.1f : 0.0f) - (sec == Beat::Chorus || sec == Beat::Drop ? 0.1f : 0.0f));
        const bool half = s.halfTime == 2 || (s.halfTime == 0 && (prof.half || sec == Beat::Breakdown || sec == Beat::Drop));
        const int length = juce::jlimit (1, 2, s.bars) * 16;
        const int nBars = length / 16;

        Grid v {}, rt {}, keep {};   // keep = escolha deliberada (double kick, rolls): o SPACE nao remove
        for (auto& r : rt) r.fill (1);
        std::array<int8_t, DrumPattern::maxSteps> notes {};
        std::array<uint8_t, DrumPattern::maxSteps> slides {};
        std::array<int8_t, DrumPattern::maxSteps> hatDir {};

        auto vel = [&] (float base) { return (uint8_t) juce::jlimit (30, 127, juce::roundToInt (base * (0.75f + 0.25f * s.energy) * 127.0f)); };
        const uint8_t accent = (uint8_t) (s.aggression > 0.6f ? 127 : 118);

        for (int bar = 0; bar < nBars; ++bar)
        {
            const int o = bar * 16;
            const bool lastBar = bar == nBars - 1;
            const bool preDropCut = sec == Beat::PreDrop && lastBar;   // pre-drop: a bateria sai no ultimo compasso

            // ---------------- SNARE: standard / half / sincopada / trap
            std::vector<int> snares;
            if (half) snares = { 8 }; else snares = { 4, 12 };
            if (! half && syn > 0.7f && prof.metal && chance (0.4f)) snares = { 4, 11 };
            if (half && tr > 0.5f && lastBar && chance (0.5f + 0.3f * cx)) snares.push_back (15);   // resposta de trap no fim da frase
            if (! preDropCut)
                for (int st : snares) v[DrumRow::Snare][(size_t) (o + st)] = st == 15 ? vel (0.75f) : accent;
            if (! preDropCut && cx > 0.35f && sec != Beat::Breakdown)                               // GHOST de caixa
                for (int st : { 7, 10, 15, 2 })
                    if (v[DrumRow::Snare][(size_t) (o + st)] == 0 && chance ((cx - 0.3f) * 0.7f * (1.0f - 0.5f * tr)))
                        v[DrumRow::Snare][(size_t) (o + st)] = 48;
            if (! preDropCut && (el > 0.4f || tr > 0.45f || sec == Beat::Chorus || sec == Beat::Drop))  // CLAP em camada
                for (int st : snares) if (st != 15) v[DrumRow::Clap][(size_t) (o + st)] = vel (0.85f);

            // ---------------- KICK: pesos + sincope + antecipacao + ghost kicks + kick duplo
            if (! preDropCut)
            {
                const float* w = (sec == Beat::Breakdown ? kBreakdown : prof.kick);
                const float threshold = 1.0f - (0.25f + 0.6f * s.density + 0.15f * s.energy);
                for (int st = 0; st < 16; ++st)
                {
                    const float weight = w[st] + (s.variation == 1 ? (rng.nextFloat() - 0.5f) * 0.8f : 0.0f);
                    if (st == 0 || weight >= threshold)
                    {
                        int pos = st;
                        if (st != 0 && st % 2 == 0 && chance (syn * 0.55f))   // SINCOPE: vai para o contratempo vizinho
                            pos = st + (chance (0.6f) ? -1 : 1);
                        pos = juce::jlimit (0, 15, pos);
                        const bool onSnare = v[DrumRow::Snare][(size_t) (o + pos)] >= 100;
                        if (onSnare && ! (prof.metal && s.aggression > 0.75f)) continue;
                        v[DrumRow::Kick][(size_t) (o + pos)] = pos == 0 || pos == 8 ? vel (1.0f) : vel (0.85f + 0.15f * s.aggression);
                    }
                }
                if (chance (syn * 0.5f) && v[DrumRow::Snare][(size_t) (o + 15)] == 0) v[DrumRow::Kick][(size_t) (o + 15)] = vel (0.8f);
                for (int st : { 5, 9, 13 })
                    if (v[DrumRow::Kick][(size_t) (o + st)] == 0 && chance ((cx - 0.45f) * 0.6f)) v[DrumRow::Kick][(size_t) (o + st)] = 55;
                if (tr > 0.4f && chance (cx * tr * 0.6f))
                    for (int st : { 3, 7, 11, 14 })
                        if (v[DrumRow::Kick][(size_t) (o + st)] > 0) { rt[DrumRow::Kick][(size_t) (o + st)] = 2; break; }
            }
            const int dk = s.doubleKick;                                          // DOUBLE KICK com funcao
            if (dk > 0 && prof.metal && ! preDropCut)
            {
                const int from = dk == 1 ? 12 : dk == 2 ? 8 : 0;
                if (dk >= 2 || lastBar)
                    for (int st = from; st < 16; ++st)
                    {
                        v[DrumRow::Kick][(size_t) (o + st)] = st % 4 == 0 ? vel (1.0f) : vel (0.8f);
                        keep[DrumRow::Kick][(size_t) (o + st)] = 1;
                    }
                if (dk == 4)
                    for (int st = 12; st < 16; ++st) rt[DrumRow::Kick][(size_t) (o + st)] = 2;
            }

            // ---------------- HATS: dinamica FORTE/fraco/medio/fraco; ride no refrao de metal
            const bool ride = prof.metal && tr < 0.5f && (sec == Beat::Chorus || sec == Beat::Bridge);
            const int hatRow = ride ? DrumRow::Ride : DrumRow::ClosedHat;
            const bool sixteenths = (s.density > 0.55f || tr > 0.3f) != (s.variation == 1);
            for (int st = 0; st < 16; ++st)
            {
                if (sec == Beat::Breakdown && ! ride && st % 4 != 0) continue;
                if (! sixteenths && st % 2 != 0) continue;
                const float base = st % 4 == 0 ? 0.86f : (st % 2 == 0 ? 0.62f : 0.44f);
                v[(size_t) hatRow][(size_t) (o + st)] = vel (base + (rng.nextFloat() - 0.5f) * 0.1f);
            }
            if (! ride)   // ROLLS INTELIGENTES: antes da caixa e no fim da frase / transicao
            {
                std::vector<int> spots;
                for (int st : snares) if (st > 0) spots.push_back (st - 1);
                if (lastBar) { spots.push_back (14); spots.push_back (15); }
                for (int st : spots)
                    if (chance (tr * 0.75f + cx * 0.2f + (lastBar ? 0.15f : 0.0f)))
                    {
                        const int idx = o + st;
                        v[DrumRow::ClosedHat][(size_t) idx] = vel (0.7f);
                        keep[DrumRow::ClosedHat][(size_t) idx] = 1;
                        rt[DrumRow::ClosedHat][(size_t) idx] = (uint8_t) (st >= 14 && (tr > 0.6f || sec == Beat::PreChorus) ? 4
                                                                         : (chance (0.35f + 0.3f * cx) ? 3 : 2));
                        hatDir[(size_t) idx] = (int8_t) (lastBar && st >= 14 ? 1 : (chance (0.5f) ? 1 : -1));
                    }
            }
            if (! ride && (el > 0.55f || sec == Beat::Chorus || sec == Beat::Drop))
            {
                for (int st : { 2, 6, 10, 14 })
                    if (rt[DrumRow::ClosedHat][(size_t) (o + st)] == 1 && chance (0.5f + 0.4f * el))
                    { v[DrumRow::OpenHat][(size_t) (o + st)] = vel (0.7f); v[DrumRow::ClosedHat][(size_t) (o + st)] = 0; }
            }
            else if (sec == Beat::Verse && lastBar && ! ride)
                v[DrumRow::OpenHat][(size_t) (o + 14)] = vel (0.7f);

            // ---------------- PERCUSSAO DIGITAL / INDUSTRIAL
            if (el > 0.3f || prof.industrial)
                for (int st : { 3, 6, 11, 13, 9 })
                    if (chance (el * 0.55f + (prof.industrial ? 0.35f : 0.0f) - space * 0.3f))
                        v[DrumRow::Perc][(size_t) (o + st)] = vel (st % 2 ? 0.55f : 0.7f);

            // ---------------- CRASH / IMPACTOS
            if (bar == 0 && (sec == Beat::Chorus || sec == Beat::Breakdown || sec == Beat::Intro || sec == Beat::Drop || s.variation == 2))
                v[DrumRow::Crash][(size_t) o] = vel (1.0f);
            if (bar == 0 && (sec == Beat::Drop || (sec == Beat::Breakdown && el > 0.5f)))
                v[DrumRow::Fx][(size_t) o] = vel (1.0f);

            // ---------------- MICRO-FILLS (1/32) no fim de compassos
            if (! preDropCut && cx > 0.4f)
            {
                if (chance ((cx - 0.3f) * 0.6f)) { v[DrumRow::Kick][(size_t) (o + 15)] = vel (0.7f); rt[DrumRow::Kick][(size_t) (o + 15)] = 2; }
                if (chance ((cx - 0.3f) * 0.5f) && v[DrumRow::Snare][(size_t) (o + 15)] == 0)
                { v[DrumRow::Snare][(size_t) (o + 15)] = 60; rt[DrumRow::Snare][(size_t) (o + 15)] = 2; }
            }
            if (gl > 0.3f && ! preDropCut)                                       // GLITCH
            {
                v[DrumRow::Fx][(size_t) (o + 15)] = vel (0.75f);
                if (gl > 0.55f) { rt[DrumRow::Snare][(size_t) (o + (half ? 8 : 12))] = 2; rt[DrumRow::ClosedHat][(size_t) (o + 13)] = 3; }
            }
        }

        // ---------------- PRE-CHORUS / PRE-DROP: hats aceleram 1/16 -> 1/64, silencio antes do impacto
        if (sec == Beat::PreChorus || sec == Beat::PreDrop)
        {
            const int o = (nBars - 1) * 16;
            for (int st = 0; st < 16; ++st)
            {
                v[DrumRow::ClosedHat][(size_t) (o + st)] = vel (0.5f + 0.03f * st);
                rt[DrumRow::ClosedHat][(size_t) (o + st)] = (uint8_t) (st < 4 ? 1 : st < 8 ? 2 : st < 12 ? 3 : 4);
                hatDir[(size_t) (o + st)] = (int8_t) (st >= 8 ? 1 : 0);
            }
            if (sec == Beat::PreChorus)
                for (int st = 12; st < 16; ++st)   // snare build SOMA a caixa (nunca enfraquece o backbeat)
                    v[DrumRow::Snare][(size_t) (o + st)] = juce::jmax (v[DrumRow::Snare][(size_t) (o + st)], vel (0.6f + 0.1f * (st - 12)));
            for (int r = 0; r < DrumPattern::numRows; ++r)                       // SILENCE ENGINE: ultimo 1/8 vazio
                for (int st = 14; st < 16; ++st) v[(size_t) r][(size_t) (o + st)] = 0;
            v[DrumRow::Fx][(size_t) (o + 12)] = vel (0.8f);
        }

        // ---------------- 808: interage com o kick, fraseado na escala, slides
        const float activity808 = tr * 1.1f + el * 0.35f + (prof.trapFamily ? 0.4f : 0.0f) + (s.style == Beat::DubstepMetal ? 0.5f : 0.0f)
                                  - (prof.metal && mod < 0.25f && tr < 0.2f ? 0.4f : 0.0f);
        if (activity808 > 0.35f && sec != Beat::PreDrop)
        {
            const auto& sc = scaleDef (s.scale);
            auto degree = [&] (int d) { return sc.steps[(size_t) (d % sc.size)] + 12 * (d / sc.size); };
            const bool dark = s.style == Beat::DarkTrap || s.style == Beat::DarkElectronic || s.style == Beat::IndustrialMetalcore;
            const std::vector<int> vocab = dark ? std::vector<int> { 0, 0, 1, 5, 4 } : std::vector<int> { 0, 0, 2, 4, 6 };
            int prev = 0, count = 0;
            for (int st = 0; st < length; ++st)
            {
                const bool kick = v[DrumRow::Kick][(size_t) st] >= 70;
                const bool bar1 = st % 16 == 0;
                bool hit = bar1 || (kick && chance (0.55f + 0.35f * tr));
                if (! hit && ! kick && st % 2 == 1 && tr > 0.5f && chance (syn * 0.25f)) hit = true;   // 808 fora do kick
                if (! hit) continue;
                int note = bar1 ? 0 : degree (vocab[(size_t) rng.nextInt ((int) vocab.size())]);
                if (! bar1 && chance (0.12f + 0.2f * tr)) note = 12;                                   // oitava
                if (st % 16 >= 14 && chance (0.5f)) note = degree (4);                                  // 5o no fim da frase
                v[DrumRow::Bass808][(size_t) st] = vel (0.95f);
                notes[(size_t) st] = (int8_t) note;
                if (count > 0 && note != prev && chance (0.25f + 0.5f * tr)) slides[(size_t) st] = 1;   // SLIDE
                prev = note; ++count;
            }
        }

        // ---------------- SPACE: remove golpes pouco importantes (nunca o 1, o backbeat, crash/fx nem o 808 do 1)
        if (space > 0.05f)
            for (int r = 0; r < DrumPattern::numRows; ++r)
                for (int st = 0; st < length; ++st)
                {
                    const uint8_t val = v[(size_t) r][(size_t) st];
                    if (val == 0) continue;
                    const bool anchor = st % 16 == 0 || (r == DrumRow::Snare && val >= 100) || r == DrumRow::Crash || r == DrumRow::Fx
                                        || keep[(size_t) r][(size_t) st] != 0;
                    if (anchor) continue;
                    const float importance = r == DrumRow::Kick ? 0.6f : r == DrumRow::Bass808 ? 0.55f : r == DrumRow::Snare ? 0.3f
                                           : (r == DrumRow::ClosedHat || r == DrumRow::Ride) ? (val > 90 ? 0.5f : 0.15f) : 0.25f;
                    if (chance (space * 0.6f * (1.0f - importance)))
                    { v[(size_t) r][(size_t) st] = 0; rt[(size_t) r][(size_t) st] = 1; }
                }

        if (s.variation == 3) addFill (v, rt, length, s, rng);

        for (int r = 0; r < DrumPattern::numRows; ++r)
        {
            if ((s.locks >> r) & 1u) continue;
            for (int st = 0; st < DrumPattern::maxSteps; ++st)
            {
                p.cells[(size_t) r][(size_t) st].store (st < length ? v[(size_t) r][(size_t) st] : 0);
                p.ratchet[(size_t) r][(size_t) st].store (st < length ? rt[(size_t) r][(size_t) st] : 1);
                if (r == DrumRow::Bass808)
                {
                    p.bassNote[(size_t) st].store (st < length ? notes[(size_t) st] : 0);
                    p.slide808[(size_t) st].store (st < length ? slides[(size_t) st] : 0);
                }
                if (r == DrumRow::ClosedHat) p.hatRollPitch[(size_t) st].store (st < length ? hatDir[(size_t) st] : 0);
            }
        }
        p.name = Beat::styleNames()[s.style] + " " + Beat::sectionNames()[s.section] + " " + juce::String::charToString ((juce::juce_wchar) ('A' + s.variation));
        p.touch();
        return length;
    }

    // MAKE A FILL: reescreve so o ultimo tempo do padrao atual (toms + roll de caixa), respeitando travas
    static void makeFill (DrumPattern& p, int length, const BeatSettings& s)
    {
        std::array<std::array<uint8_t, DrumPattern::maxSteps>, DrumPattern::numRows> v {};
        std::array<std::array<uint8_t, DrumPattern::maxSteps>, DrumPattern::numRows> rt {};
        for (int r = 0; r < DrumPattern::numRows; ++r)
            for (int st = 0; st < DrumPattern::maxSteps; ++st)
            {
                v[(size_t) r][(size_t) st] = p.cells[(size_t) r][(size_t) st].load();
                rt[(size_t) r][(size_t) st] = p.ratchet[(size_t) r][(size_t) st].load();
            }
        juce::Random rng ((juce::int64) s.seed * 31 + 7);
        addFill (v, rt, length, s, rng);
        for (int r = 0; r < DrumPattern::numRows; ++r)
        {
            if ((s.locks >> r) & 1u) continue;
            for (int st = juce::jmax (0, length - 4); st < length; ++st)
            {
                p.cells[(size_t) r][(size_t) st].store (v[(size_t) r][(size_t) st]);
                p.ratchet[(size_t) r][(size_t) st].store (rt[(size_t) r][(size_t) st]);
            }
        }
        p.touch();
    }

    // =========================================================================
    //  MAKE IT ... : transforma o beat ATUAL sem destruir o groove (respeita travas)
    //  0 MODERN  1 HEAVIER  2 MORE TRAP  3 MORE ELECTRONIC  4 DARKER  5 BIGGER  6 TIGHTER  7 GROOVIER
    // =========================================================================
    static const juce::StringArray& transformNames()
    {
        static const juce::StringArray n { "MODERN", "HEAVIER", "MORE TRAP", "ELECTRONIC", "DARKER", "BIGGER", "TIGHTER", "GROOVIER" };
        return n;
    }

    static void transform (DrumPattern& p, int length, int kind, const BeatSettings& s)
    {
        length = juce::jlimit (1, DrumPattern::maxSteps, length);   // LENGTH pode ser 1: nenhum indice pode ficar negativo
        Grid v {}, rt {};
        std::array<int8_t, DrumPattern::maxSteps> notes {}, hatDir {};
        std::array<uint8_t, DrumPattern::maxSteps> slides {};
        for (int r = 0; r < DrumPattern::numRows; ++r)
            for (int st = 0; st < DrumPattern::maxSteps; ++st)
            { v[(size_t) r][(size_t) st] = p.cells[(size_t) r][(size_t) st].load(); rt[(size_t) r][(size_t) st] = p.ratchet[(size_t) r][(size_t) st].load(); }
        for (int st = 0; st < DrumPattern::maxSteps; ++st)
        { notes[(size_t) st] = p.bassNote[(size_t) st].load(); slides[(size_t) st] = p.slide808[(size_t) st].load(); hatDir[(size_t) st] = p.hatRollPitch[(size_t) st].load(); }

        juce::Random rng ((juce::int64) s.seed * 977 + kind * 31 + 5);
        auto chance = [&] (float pr) { return rng.nextFloat() < pr; };
        const auto& sc = scaleDef (s.scale);
        auto degree = [&] (int d) { return sc.steps[(size_t) (d % sc.size)] + 12 * (d / sc.size); };
        auto isSnare = [&] (int st) { return v[DrumRow::Snare][(size_t) st] >= 100; };
        auto add808OnKicks = [&] (float pr)
        {
            for (int st = 0; st < length; ++st)
                if (v[DrumRow::Kick][(size_t) st] >= 70 && v[DrumRow::Bass808][(size_t) st] == 0 && (st % 16 == 0 || chance (pr)))
                {
                    v[DrumRow::Bass808][(size_t) st] = 115;
                    notes[(size_t) st] = (int8_t) (st % 16 == 0 ? 0 : degree (chance (0.6f) ? 0 : 4));
                }
        };
        auto rollsBeforeSnare = [&] (float pr, int maxRt)
        {
            for (int st = 1; st < length; ++st)
                if (isSnare (st) && chance (pr))
                {
                    v[DrumRow::ClosedHat][(size_t) (st - 1)] = 90;
                    rt[DrumRow::ClosedHat][(size_t) (st - 1)] = (uint8_t) juce::jmax (2, maxRt - rng.nextInt (2));
                    hatDir[(size_t) (st - 1)] = (int8_t) (chance (0.5f) ? 1 : -1);
                }
        };
        auto hatDynamics = [&]
        {
            for (int st = 0; st < length; ++st)
                if (v[DrumRow::ClosedHat][(size_t) st] > 0 && rt[DrumRow::ClosedHat][(size_t) st] == 1)
                    v[DrumRow::ClosedHat][(size_t) st] = (uint8_t) (st % 4 == 0 ? 108 : st % 2 == 0 ? 78 : 56);
        };

        switch (kind)
        {
            case 0:   // MODERN: 808 no kick, rolls antes da caixa, ghosts, dinamica, perc digital, glitch leve
                add808OnKicks (0.6f); rollsBeforeSnare (0.7f, 3); hatDynamics();
                for (int st = 0; st < length; ++st)
                {
                    if (st % 16 == 7 && v[DrumRow::Snare][(size_t) st] == 0 && chance (0.6f)) v[DrumRow::Snare][(size_t) st] = 48;
                    if ((st % 16 == 3 || st % 16 == 11) && chance (0.6f)) v[DrumRow::Perc][(size_t) st] = 70;
                }
                v[DrumRow::Fx][(size_t) (length - 1)] = 90;
                break;
            case 1:   // HEAVIER: acentos, kick no 3, double kick no ultimo tempo, crash no 1
                for (int st = 0; st < length; ++st)
                {
                    if (v[DrumRow::Kick][(size_t) st] > 0) v[DrumRow::Kick][(size_t) st] = (uint8_t) juce::jmax (115, (int) v[DrumRow::Kick][(size_t) st]);
                    if (isSnare (st)) v[DrumRow::Snare][(size_t) st] = 127;
                    if (st % 16 == 8 && ! isSnare (st)) v[DrumRow::Kick][(size_t) st] = 120;
                }
                for (int st = juce::jmax (0, length - 4); st < length; ++st) if (! isSnare (st)) v[DrumRow::Kick][(size_t) st] = 110;
                for (int st = 0; st < length; st += 16) v[DrumRow::Crash][(size_t) st] = 127;
                break;
            case 2:   // MORE TRAP: hats em 1/16 com dinamica, rolls 1/64, 808 extra, clap na caixa
                for (int st = 0; st < length; ++st) if (v[DrumRow::Ride][(size_t) st] == 0) v[DrumRow::ClosedHat][(size_t) st] = 60;
                hatDynamics(); rollsBeforeSnare (0.9f, 4); add808OnKicks (0.9f);
                for (int st = 0; st < length; ++st)
                {
                    if (isSnare (st)) v[DrumRow::Clap][(size_t) st] = 110;
                    if (st % 2 == 1 && v[DrumRow::Bass808][(size_t) st] == 0 && v[DrumRow::Kick][(size_t) st] == 0 && chance (0.12f))
                    { v[DrumRow::Bass808][(size_t) st] = 105; notes[(size_t) st] = (int8_t) degree (4); slides[(size_t) st] = 1; }
                }
                break;
            case 3:   // MORE ELECTRONIC: clap, perc digital, open hats no contratempo, zaps
                for (int st = 0; st < length; ++st)
                {
                    if (isSnare (st)) v[DrumRow::Clap][(size_t) st] = 110;
                    if ((st % 16 == 3 || st % 16 == 6 || st % 16 == 13) && chance (0.7f)) v[DrumRow::Perc][(size_t) st] = 75;
                    if (st % 4 == 2 && chance (0.6f)) { v[DrumRow::OpenHat][(size_t) st] = 85; v[DrumRow::ClosedHat][(size_t) st] = 0; }
                }
                v[DrumRow::Fx][(size_t) (length - 1)] = 100;
                break;
            case 4:   // DARKER: 808 grave (sem oitavas), menos brilho (sem open hat), mais espaco
                for (int st = 0; st < length; ++st)
                {
                    if (notes[(size_t) st] >= 12) notes[(size_t) st] = (int8_t) (notes[(size_t) st] - 12);
                    v[DrumRow::OpenHat][(size_t) st] = 0;
                    if (v[DrumRow::ClosedHat][(size_t) st] > 0 && v[DrumRow::ClosedHat][(size_t) st] < 70 && chance (0.5f))
                    { v[DrumRow::ClosedHat][(size_t) st] = 0; rt[DrumRow::ClosedHat][(size_t) st] = 1; }
                    if (v[DrumRow::Perc][(size_t) st] > 0 && chance (0.5f)) v[DrumRow::Perc][(size_t) st] = 0;
                }
                break;
            case 5:   // BIGGER: crash por compasso, impacto no 1, open hat de fim, clap
                for (int st = 0; st < length; st += 16) { v[DrumRow::Crash][(size_t) st] = 120; }
                v[DrumRow::Fx][0] = 115;
                if (length >= 2) v[DrumRow::OpenHat][(size_t) (length - 2)] = 100;
                for (int st = 0; st < length; ++st) if (isSnare (st)) v[DrumRow::Clap][(size_t) st] = 105;
                break;
            case 6:   // TIGHTER: sem ghosts de kick/caixa, velocity consistente, 808 so junto do kick
                for (int st = 0; st < length; ++st)
                {
                    for (int r : { (int) DrumRow::Kick, (int) DrumRow::Snare })
                    {
                        auto& c = v[(size_t) r][(size_t) st];
                        if (c > 0 && c < 80) { c = 0; rt[(size_t) r][(size_t) st] = 1; }
                        else if (c > 0) c = (uint8_t) (r == DrumRow::Kick ? 115 : 122);
                    }
                    rt[DrumRow::Kick][(size_t) st] = 1;
                    if (v[DrumRow::Bass808][(size_t) st] > 0 && v[DrumRow::Kick][(size_t) st] == 0 && st % 16 != 0)
                        v[DrumRow::Bass808][(size_t) st] = 0;
                }
                break;
            default:  // GROOVIER: ghosts de caixa/kick, acentos de hat, antecipacao
                hatDynamics();
                for (int st = 0; st < length; ++st)
                {
                    const int b = st % 16;
                    if ((b == 7 || b == 10 || b == 15) && v[DrumRow::Snare][(size_t) st] == 0 && chance (0.55f)) v[DrumRow::Snare][(size_t) st] = 45;
                    if ((b == 5 || b == 13) && v[DrumRow::Kick][(size_t) st] == 0 && ! isSnare (st) && chance (0.4f)) v[DrumRow::Kick][(size_t) st] = 58;
                    if (b == 15 && v[DrumRow::Kick][(size_t) st] == 0 && chance (0.4f)) v[DrumRow::Kick][(size_t) st] = 90;
                }
                break;
        }

        for (int r = 0; r < DrumPattern::numRows; ++r)
        {
            if ((s.locks >> r) & 1u) continue;
            for (int st = 0; st < length; ++st)
            {
                p.cells[(size_t) r][(size_t) st].store (v[(size_t) r][(size_t) st]);
                p.ratchet[(size_t) r][(size_t) st].store (rt[(size_t) r][(size_t) st]);
                if (r == DrumRow::Bass808) { p.bassNote[(size_t) st].store (notes[(size_t) st]); p.slide808[(size_t) st].store (slides[(size_t) st]); }
                if (r == DrumRow::ClosedHat) p.hatRollPitch[(size_t) st].store (hatDir[(size_t) st]);
            }
        }
        p.touch();
    }

private:
    using Grid = std::array<std::array<uint8_t, DrumPattern::maxSteps>, DrumPattern::numRows>;
    struct Profile { const float* kick; bool half, metal; float electronic, trap, glitch; bool industrial = false, trapFamily = false; };

    // pesos de posicao do kick (16 semicolcheias); 1 = sempre, 0 = nunca
    static constexpr float kMetal[16]      { 1, 0, .3f, .5f, 0, 0, .6f, .2f, .9f, 0, .7f, .3f, 0, 0, .5f, .4f };
    static constexpr float kMetalcore[16]  { 1, 0, .2f, .6f, 0, 0, .7f, 0, .8f, .2f, .6f, 0, 0, .3f, .6f, 0 };
    static constexpr float kNu[16]         { 1, 0, 0, .7f, 0, 0, .5f, 0, .6f, 0, .8f, 0, 0, 0, .4f, 0 };
    static constexpr float kFour[16]       { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, .25f, 0 };
    static constexpr float kTrap[16]       { 1, 0, 0, .4f, 0, 0, .3f, .7f, 0, 0, .8f, .2f, 0, .3f, .2f, .5f };
    static constexpr float kDubstep[16]    { 1, 0, 0, 0, 0, 0, .2f, 0, 0, 0, .7f, .3f, 0, 0, .4f, 0 };
    static constexpr float kCinematic[16]  { 1, 0, 0, 0, 0, 0, .4f, 0, .7f, 0, 0, 0, 0, 0, .3f, 0 };
    static constexpr float kBreakdown[16]  { 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0 };

    static Profile profile (int style)
    {
        switch (style)
        {
            case Beat::ModernMetal:     return { kMetal,     false, true,  0.0f, 0.0f, 0.0f };
            case Beat::Metalcore:       return { kMetalcore, false, true,  0.1f, 0.0f, 0.0f };
            case Beat::ElectronicMetal: return { kMetalcore, false, true,  0.5f, 0.0f, 0.1f };
            case Beat::Industrial:      return { kFour,      false, true,  0.4f, 0.0f, 0.2f, true, false };
            case Beat::NuMetal:         return { kNu,        false, true,  0.2f, 0.0f, 0.0f };
            case Beat::TrapMetal:       return { kTrap,      true,  true,  0.3f, 0.5f, 0.1f, false, true };
            case Beat::Electronicore:   return { kFour,      false, true,  0.6f, 0.1f, 0.2f };
            case Beat::DubstepMetal:    return { kDubstep,   true,  true,  0.5f, 0.2f, 0.3f };
            case Beat::HalfTime:        return { kMetal,     true,  true,  0.1f, 0.0f, 0.0f };
            case Beat::DarkElectronic:  return { kTrap,      true,  false, 0.7f, 0.3f, 0.3f };
            case Beat::CinematicMetal:  return { kCinematic, true,  true,  0.2f, 0.0f, 0.0f };
            case Beat::ModernTrap:          return { kTrap,      true,  false, 0.4f, 0.8f, 0.1f, false, true };
            case Beat::DarkTrap:            return { kTrap,      true,  false, 0.5f, 0.75f, 0.15f, false, true };
            case Beat::ModernElectronicore: return { kMetalcore, false, true,  0.6f, 0.35f, 0.3f, false, false };
            case Beat::ModernTrapRock:      return { kTrap,      true,  true,  0.35f, 0.6f, 0.1f, false, true };
            case Beat::IndustrialMetalcore: return { kMetal,     true,  true,  0.45f, 0.1f, 0.2f, true, false };
            case Beat::Trapcore:            return { kTrap,      true,  true,  0.4f, 0.65f, 0.2f, false, true };
            default:                    return { kMetalcore, false, true,  0.4f, 0.2f, 0.1f };   // Hybrid
        }
    }

    template <typename Grid, typename Rt>
    static void addFill (Grid& v, Rt& rt, int length, const BeatSettings& s, juce::Random& rng)
    {
        const int start = juce::jmax (0, length - 4);   // ultimo tempo
        for (int r = 0; r < DrumPattern::numRows; ++r)
            if (r != DrumRow::Bass808)
                for (int st = start; st < length; ++st) { v[(size_t) r][(size_t) st] = 0; rt[(size_t) r][(size_t) st] = 1; }
        const bool electronic = s.electronic > 0.5f || s.style == Beat::DarkElectronic;
        v[DrumRow::Tom][(size_t) start]           = 110;
        v[DrumRow::Tom][(size_t) (start + 1)]     = 100;
        v[DrumRow::Snare][(size_t) (start + 2)]   = 105; rt[DrumRow::Snare][(size_t) (start + 2)] = electronic ? 4 : 2;   // roll
        v[DrumRow::Snare][(size_t) (start + 3)]   = 127; rt[DrumRow::Snare][(size_t) (start + 3)] = electronic ? 3 : 1;
        v[DrumRow::Kick][(size_t) (start + 3)]    = 120;
        if (rng.nextFloat() < 0.5f) v[DrumRow::Tom][(size_t) (start + 3)] = 90;
        if (s.glitch > 0.3f) v[DrumRow::Fx][(size_t) (start + 3)] = 110;
    }
};
} // namespace mm
