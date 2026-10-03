#pragma once
#include "Drums/BeatGenerator.h"
#include "Drums/BeatMidi.h"
#include "Sequencer/Scale.h"

namespace mm
{
// =============================================================================
//  SONG GENERATOR - "pensa como produtor": gera uma IDEIA MUSICAL completa
//  HARMONIA -> MOTIF -> melodia/riff/arp/pad derivados do motif
//  RHYTHMIC LOCK: kick e 808 acompanham os acentos do motif
//  808 = tonica de cada acorde, com slide quando o acorde muda
//  RIFF preenche os espacos entre os kicks (call & response)
//  CURVA DE ENERGIA decide as camadas de cada secao; FX e automacao nas transicoes
//  Deterministico pela SEED. Roda no message thread (nunca no audio).
// =============================================================================
namespace Song
{
    enum Track { Drums = 0, Kick, Snare, Hats, Perc, B808, Bass, Riff, Melody, Arp, Pad, Fx, NumTracks };
    inline const juce::StringArray& trackNames()
    {
        static const juce::StringArray n { "DRUMS", "KICK", "SNARE", "HATS", "PERC", "808", "BASS", "SYNTH RIFF",
                                           "MELODY", "ARP", "PAD", "FX" };
        return n;
    }
    // elementos travaveis (bit): DRUMS, 808, BASS, RIFF, MELODY, ARP, PAD, FX
    enum Lock { LDrums = 0, L808, LBass, LRiff, LMelody, LArp, LPad, LFx, NumLocks };

    enum SecType { Intro = 0, Verse, Pre, Chorus, Breakdown, Bridge, FinalChorus, Drop, Outro };
    inline const juce::StringArray& secNames()
    {
        static const juce::StringArray n { "INTRO", "VERSE", "PRE", "CHORUS", "BREAKDOWN", "BRIDGE", "FINAL CHORUS", "DROP", "OUTRO" };
        return n;
    }
    inline const juce::StringArray& styleNames()
    {
        static const juce::StringArray n { "Modern Metalcore", "Electronic Metalcore", "Dark Metalcore", "Trap Metalcore",
                                           "Industrial Metalcore", "Cinematic Metalcore", "Digital Metalcore", "Emotional Metalcore",
                                           "Heavy Electronic", "Modern Post-Hardcore", "Nu Metal Modern", "Electronicore",
                                           "Darkcore", "Cyber Metal", "Modern Trap Rock", "Industrial Modern Metal" };
        return n;
    }
    inline const juce::StringArray& moodNames()
    {
        static const juce::StringArray n { "Dark", "Emotional", "Epic", "Aggressive", "Melancholic", "Tense", "Cinematic" };
        return n;
    }
}

struct SongSettings
{
    double bpm = 150.0;
    int root = 6, scale = 0;                 // F# minor
    int style = 0, mood = 0, structure = 1;  // structure: 0 Short, 1 Standard, 2 Extended
    float heaviness = 0.8f, electronic = 0.6f, trap = 0.4f, darkness = 0.7f, melodic = 0.7f,
          modernity = 0.9f, texture = 0.6f, fxDensity = 0.6f;
    bool vocalSpace = true, quickIdea = false;
    int seed = 48291;
};

struct SongNote { int tick = 0, len = 0, note = 60, vel = 100; };
struct SongCC   { int tick = 0, cc = 74, value = 0; };

struct SongSection
{
    int type = Song::Verse, bars = 8, startBar = 0, repeat = 0;
    float energy = 0.5f;
    BeatSettings beat;                         // para o PREVIEW (recria o padrao exato)
    std::array<int8_t, 16> riffDegree {};      // riff da secao em graus da escala (preview no sequencer)
    std::array<uint8_t, 16> riffOn {};
};

struct SongData
{
    SongSettings settings;
    std::vector<SongSection> sections;
    std::vector<int> chordDegree;              // um acorde por compasso (grau da escala)
    std::vector<std::pair<int, int>> motif;    // (passo 0..15, grau relativo a tonica do acorde)
    std::array<std::vector<SongNote>, Song::NumTracks> tracks;
    std::array<std::vector<SongCC>, Song::NumTracks> ccs;
    int totalBars = 0;
    bool valid = false;
};

class SongGenerator
{
public:
    static constexpr int ppq = 960, step = ppq / 4, barTicks = ppq * 4;

    // Gera a musica. 'previous' + 'locks': elementos travados sao copiados da ideia anterior
    static SongData generate (const SongSettings& s, const SongData* previous = nullptr, uint32_t locks = 0)
    {
        SongData song;
        song.settings = s;
        juce::Random rng ((juce::int64) s.seed * 7919 + 13);
        buildStructure (song, s);
        buildHarmony (song, s, rng);
        buildMotif (song, s, rng);

        const auto& sc = scaleDef (s.scale);
        auto pitch = [&] (int degree, int octave)   // grau da escala -> nota MIDI
        {
            const int sz = sc.size;
            const int d = ((degree % sz) + sz) % sz, oct = (degree - d) / sz;
            return juce::jlimit (0, 127, 12 * (octave + 1) + s.root + sc.steps[(size_t) d] + 12 * oct);
        };

        std::map<int, int> repeats;
        for (auto& sec : song.sections)
        {
            sec.repeat = repeats[sec.type]++;
            renderSection (song, sec, s, rng, pitch);
        }
        buildFx (song, s);

        // TRAVAS: se a estrutura for a mesma, elementos travados voltam da ideia anterior
        if (previous != nullptr && previous->valid && previous->totalBars == song.totalBars)
        {
            auto keepTracks = [&] (std::initializer_list<int> ts) { for (int t : ts) { song.tracks[(size_t) t] = previous->tracks[(size_t) t]; song.ccs[(size_t) t] = previous->ccs[(size_t) t]; } };
            if (locks & (1u << Song::LDrums))  keepTracks ({ Song::Drums, Song::Kick, Song::Snare, Song::Hats, Song::Perc });
            if (locks & (1u << Song::L808))    keepTracks ({ Song::B808 });
            if (locks & (1u << Song::LBass))   keepTracks ({ Song::Bass });
            if (locks & (1u << Song::LRiff))   keepTracks ({ Song::Riff });
            if (locks & (1u << Song::LMelody)) keepTracks ({ Song::Melody });
            if (locks & (1u << Song::LArp))    keepTracks ({ Song::Arp });
            if (locks & (1u << Song::LPad))    keepTracks ({ Song::Pad });
            if (locks & (1u << Song::LFx))     keepTracks ({ Song::Fx });
            if (locks & (1u << Song::LDrums))
                for (size_t i = 0; i < song.sections.size(); ++i) song.sections[i].beat = previous->sections[i].beat;
            if (locks & (1u << Song::LRiff))
                for (size_t i = 0; i < song.sections.size(); ++i)
                { song.sections[i].riffDegree = previous->sections[i].riffDegree; song.sections[i].riffOn = previous->sections[i].riffOn; }
        }
        // ordem deterministica (notas simultaneas: desempate pela altura)
        for (auto& t : song.tracks)
            std::sort (t.begin(), t.end(), [] (const SongNote& a, const SongNote& b) { return a.tick != b.tick ? a.tick < b.tick : a.note < b.note; });
        song.valid = true;
        return song;
    }

    // MIDI tipo 1: tempo + marcadores das secoes + 12 trilhas nomeadas
    static juce::MidiFile toMidi (const SongData& song)
    {
        juce::MidiFile f;
        f.setTicksPerQuarterNote (ppq);
        juce::MidiMessageSequence meta;
        meta.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / juce::jlimit (20.0, 999.0, song.settings.bpm))), 0);
        meta.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
        meta.addEvent (juce::MidiMessage::textMetaEvent (3, "ATDT SYNTH MACHINE - " + Song::styleNames()[song.settings.style]
                                                         + " seed " + juce::String (song.settings.seed)), 0);
        for (const auto& sec : song.sections)   // marcadores: o REAPER mostra as secoes na regua
            meta.addEvent (juce::MidiMessage::textMetaEvent (6, Song::secNames()[sec.type] + (sec.repeat > 0 ? " " + juce::String (sec.repeat + 1) : "")),
                           (double) sec.startBar * barTicks);
        f.addTrack (meta);
        for (int t = 0; t < Song::NumTracks; ++t)
        {
            juce::MidiMessageSequence seq;
            seq.addEvent (juce::MidiMessage::textMetaEvent (3, juce::String (t + 1).paddedLeft ('0', 2) + " " + Song::trackNames()[t]), 0);
            const int ch = t <= Song::Perc ? 10 : (t == Song::B808 ? 2 : 1);
            for (const auto& n : song.tracks[(size_t) t])
            {
                seq.addEvent (juce::MidiMessage::noteOn (ch, n.note, (juce::uint8) juce::jlimit (1, 127, n.vel)), n.tick);
                seq.addEvent (juce::MidiMessage::noteOff (ch, n.note), n.tick + juce::jmax (1, n.len));
            }
            for (const auto& c : song.ccs[(size_t) t])
                seq.addEvent (juce::MidiMessage::controllerEvent (ch, c.cc, c.value), c.tick);
            seq.sort();
            seq.updateMatchedPairs();
            f.addTrack (seq);
        }
        return f;
    }

    // Padrao de bateria de uma secao (o MESMO usado no MIDI e no PREVIEW): Beat Engine + RHYTHMIC LOCK
    static void sectionPattern (DrumPattern& pat, const SongData& song, const SongSection& sec)
    {
        BeatGenerator::generate (pat, sec.beat);
        const bool strong = sec.type == Song::Chorus || sec.type == Song::FinalChorus || sec.type == Song::Drop || sec.type == Song::Breakdown;
        if (strong)   // nas secoes fortes o kick acompanha os acentos do motif
            for (auto [st, deg] : song.motif)
                for (int bar = 0; bar < 2; ++bar)
                {
                    const int i = bar * 16 + st;
                    if (pat.cells[DrumRow::Snare][(size_t) i].load() < 100 && pat.cells[DrumRow::Kick][(size_t) i].load() == 0)
                        pat.cells[DrumRow::Kick][(size_t) i].store (110);
                }
        pat.name = "SONG " + Song::secNames()[sec.type];
        pat.touch();
    }

    // relaciona o estilo da musica ao estilo do Beat Engine
    static int beatStyleFor (int songStyle)
    {
        static const int map[] { Beat::Metalcore, Beat::ModernElectronicore, Beat::IndustrialMetalcore, Beat::Trapcore,
                                 Beat::IndustrialMetalcore, Beat::CinematicMetal, Beat::ElectronicMetal, Beat::Metalcore,
                                 Beat::ElectronicMetal, Beat::Metalcore, Beat::NuMetal, Beat::ModernElectronicore,
                                 Beat::DarkTrap, Beat::Electronicore, Beat::ModernTrapRock, Beat::IndustrialMetalcore };
        return map[juce::jlimit (0, 15, songStyle)];
    }

private:
    static void buildStructure (SongData& song, const SongSettings& s)
    {
        using namespace Song;
        std::vector<std::pair<int, int>> plan;   // (tipo, compassos)
        if (s.quickIdea || s.structure == 0)
            plan = { { Intro, 4 }, { Verse, 8 }, { Pre, 4 }, { Chorus, 8 }, { Breakdown, 8 }, { Outro, 4 } };
        else if (s.structure == 1)
            plan = { { Intro, 4 }, { Verse, 8 }, { Pre, 4 }, { Chorus, 8 }, { Verse, 8 }, { Pre, 4 }, { Chorus, 8 },
                     { Breakdown, 8 }, { Bridge, 8 }, { FinalChorus, 8 }, { Outro, 4 } };
        else
            plan = { { Intro, 8 }, { Verse, 8 }, { Pre, 4 }, { Chorus, 8 }, { Verse, 8 }, { Pre, 4 }, { Chorus, 8 },
                     { Breakdown, 8 }, { Bridge, 8 }, { Pre, 4 }, { Drop, 8 }, { FinalChorus, 8 }, { Outro, 8 } };
        // CURVA DE ENERGIA (o peso da musica escala tudo)
        auto energyOf = [&] (int t, int rep)
        {
            switch (t)
            {
                case Intro: return 0.2f; case Verse: return 0.45f + 0.05f * rep; case Pre: return 0.65f;
                case Chorus: return 0.85f + 0.05f * rep; case Breakdown: return 1.0f; case Bridge: return 0.35f;
                case FinalChorus: return 1.0f; case Drop: return 1.0f; default: return 0.25f;
            }
        };
        int bar = 0; std::map<int, int> reps;
        for (auto [t, b] : plan)
        {
            SongSection sec; sec.type = t; sec.bars = b; sec.startBar = bar;
            sec.energy = juce::jlimit (0.1f, 1.0f, energyOf (t, reps[t]++) * (0.75f + 0.25f * s.heaviness));
            song.sections.push_back (sec);
            bar += b;
        }
        song.totalBars = bar;
    }

    // HARMONIA por clima (graus da escala), 1 acorde por compasso; bridge usa a progressao deslocada (contraste)
    static void buildHarmony (SongData& song, const SongSettings& s, juce::Random& rng)
    {
        static const int prog[7][4] { { 0, 5, 2, 6 },   // Dark:        i  VI  III VII
                                      { 5, 2, 6, 0 },   // Emotional:   VI III VII i
                                      { 0, 6, 5, 6 },   // Epic:        i  VII VI  VII
                                      { 0, 1, 0, 6 },   // Aggressive:  i  II  i   VII (II = b2 no frigio)
                                      { 0, 3, 5, 4 },   // Melancholic: i  iv  VI  v
                                      { 0, 1, 4, 0 },   // Tense
                                      { 0, 5, 3, 4 } }; // Cinematic
        const auto* p = prog[juce::jlimit (0, 6, s.mood)];
        juce::ignoreUnused (rng);
        song.chordDegree.assign ((size_t) song.totalBars, 0);
        for (const auto& sec : song.sections)
            for (int b = 0; b < sec.bars; ++b)
            {
                const int shift = sec.type == Song::Bridge ? 2 : 0;
                const int slow = (sec.type == Song::Verse || sec.type == Song::Intro || sec.type == Song::Outro) ? 2 : 1;   // verso: harmonia mais lenta
                song.chordDegree[(size_t) (sec.startBar + b)] = p[(b / slow + shift) % 4];
            }
    }

    // MOTIF: 3-6 notas em 1 compasso; notas de acorde com movimento por grau conjunto
    static void buildMotif (SongData& song, const SongSettings& s, juce::Random& rng)
    {
        const int count = 3 + juce::jlimit (0, 3, (int) (s.melodic * 3.0f + rng.nextFloat()));
        std::vector<int> slots { 0, 2, 3, 4, 6, 7, 8, 10, 11, 12, 14 };
        std::vector<int> pos { 0 };
        while ((int) pos.size() < count)
        {
            const int c = slots[(size_t) (1 + rng.nextInt ((int) slots.size() - 1))];
            if (std::find (pos.begin(), pos.end(), c) == pos.end()) pos.push_back (c);
        }
        std::sort (pos.begin(), pos.end());
        const int tones[] { 0, 2, 4, 1, 3, 5, 7 };   // tons de acorde primeiro
        int deg = s.darkness > 0.6f ? 0 : tones[rng.nextInt (3)];
        for (int st : pos)
        {
            song.motif.push_back ({ st, deg });
            const int move = rng.nextFloat() < 0.6f ? (rng.nextBool() ? 1 : -1) : tones[rng.nextInt (3)] - deg;   // grau conjunto
            deg = juce::jlimit (-2, 7, deg + juce::jlimit (-3, 3, move));
        }
    }

    template <typename PitchFn>
    static void renderSection (SongData& song, SongSection& sec, const SongSettings& s, juce::Random& rng, PitchFn pitch)
    {
        using namespace Song;
        const int t = sec.type;
        const bool heavy = t == Breakdown || t == Drop;
        const bool big = t == Chorus || t == FinalChorus || t == Drop;
        const float e = sec.energy;

        // ---------------- BATERIA: Beat Engine por secao, variacao por repeticao
        BeatSettings b;
        b.style = beatStyleFor (s.style);
        static const int secMap[] { Beat::Intro, Beat::Verse, Beat::PreChorus, Beat::Chorus, Beat::Breakdown, Beat::Bridge, Beat::Chorus, Beat::Drop, Beat::Outro };
        b.section = secMap[t];
        b.variation = juce::jlimit (0, 2, sec.repeat + (t == FinalChorus ? 2 : 0));
        b.energy = e; b.density = 0.35f + 0.5f * e; b.aggression = s.heaviness;
        b.electronic = s.electronic; b.trap = s.trap; b.glitch = s.electronic * 0.4f;
        b.modernity = s.modernity; b.syncopation = 0.2f + 0.5f * s.trap; b.complexity = 0.3f + 0.4f * e;
        b.space = (t == Verse && s.vocalSpace) ? 0.45f : (t == Intro || t == Bridge || t == Outro ? 0.6f : 0.15f);
        b.doubleKick = heavy && s.heaviness > 0.7f ? 2 : (big && s.heaviness > 0.85f ? 1 : 0);
        b.root = s.root; b.scale = s.scale; b.bars = 2;
        b.seed = s.seed + t * 101;
        sec.beat = b;
        DrumPattern pat;
        sectionPattern (pat, song, sec);
        const bool drumsOn = t != Intro || e > 0.3f;
        for (int bar = 0; bar < sec.bars; ++bar)
        {
            const int absBar = sec.startBar + bar;
            const bool lastBar = bar == sec.bars - 1;
            DrumPattern fillPat;   // ultimo compasso da secao: fill (transicao)
            const DrumPattern* src = &pat;
            if (lastBar && t != Outro)
            {
                for (int r = 0; r < DrumPattern::numRows; ++r)
                    for (int i = 0; i < 32; ++i)
                    {
                        fillPat.cells[(size_t) r][(size_t) i].store (pat.cells[(size_t) r][(size_t) i].load());
                        fillPat.ratchet[(size_t) r][(size_t) i].store (pat.ratchet[(size_t) r][(size_t) i].load());
                    }
                BeatGenerator::makeFill (fillPat, 32, b);
                src = &fillPat;
            }
            const int half = bar % 2;   // padrao de 2 compassos
            for (int st = 0; st < 16 && drumsOn; ++st)
            {
                const int i = half * 16 + st;
                const int tick = absBar * barTicks + st * step;
                for (int r = 0; r < DrumRow::Bass808; ++r)
                {
                    const int v = src->cells[(size_t) r][(size_t) i].load();
                    if (v == 0) continue;
                    const int rt = juce::jlimit (1, 4, (int) src->ratchet[(size_t) r][(size_t) i].load());
                    for (int j = 0; j < rt; ++j)
                    {
                        SongNote n { tick + j * step / rt, step / rt / 2, BeatMidi::gmNote (r), (int) (v * (1.0f - 0.1f * j)) };
                        song.tracks[Drums].push_back (n);
                        const int dst = r == DrumRow::Kick ? Kick : (r == DrumRow::Snare || r == DrumRow::Clap) ? Snare
                                      : (r == DrumRow::ClosedHat || r == DrumRow::OpenHat || r == DrumRow::Ride || r == DrumRow::Crash) ? Hats : Perc;
                        song.tracks[(size_t) dst].push_back (n);
                    }
                }
                // 808 = TONICA DO ACORDE (a melodia manda); slide quando o acorde muda
                if (src->cells[DrumRow::Bass808][(size_t) i].load() > 0)
                {
                    const int chord = song.chordDegree[(size_t) absBar];
                    song.tracks[B808].push_back ({ tick, step * 2, pitch (chord, 1), 110 });
                }
            }
            // ---------------- BAIXO: tonica do acorde nos kicks (travado ao groove)
            if (t != Intro && t != Bridge && ! (s.trap > 0.6f && t == Verse))
                for (int st = 0; st < 16; ++st)
                    if (pat.cells[DrumRow::Kick][(size_t) (half * 16 + st)].load() >= 70)
                        song.tracks[Bass].push_back ({ absBar * barTicks + st * step, step * (heavy ? 1 : 2), pitch (song.chordDegree[(size_t) absBar], 2), 105 });
        }

        // ---------------- PAD: acordes (menos em versos com espaco para voz)
        const bool padOn = t == Intro || t == Bridge || t == Outro || big || (t == Pre) || (t == Verse && ! s.vocalSpace);
        if (padOn && ! s.quickIdea)
            for (int bar = 0; bar < sec.bars; ++bar)
            {
                const int absBar = sec.startBar + bar, ch = song.chordDegree[(size_t) absBar];
                for (int k : { 0, 2, 4 })
                    song.tracks[Pad].push_back ({ absBar * barTicks, barTicks - step, pitch (ch + k, 3), (int) (60 + 40 * e) });
                // SIDECHAIN (CC11): o pad "respira" com o kick nas secoes eletronicas fortes
                if (s.electronic > 0.5f && (big || heavy))
                    for (int st = 0; st < 16; ++st)
                        if (pat.cells[DrumRow::Kick][(size_t) ((bar % 2) * 16 + st)].load() >= 70)
                        {
                            song.ccs[Pad].push_back ({ absBar * barTicks + st * step, 11, 35 });
                            song.ccs[Pad].push_back ({ absBar * barTicks + st * step + step * 2, 11, 127 });
                        }
            }

        // ---------------- MOTIF EVOLUI: intro = pad melody lenta | verso = pluck esparso | pre = sobe |
        //                  refrao = lead completo | breakdown = riff grave distorcido | final = stack de oitavas
        auto motifAt = [&] (int absBar, int octave, int vel, float stretch, int transpose)
        {
            const int ch = song.chordDegree[(size_t) absBar];
            for (size_t m = 0; m < song.motif.size(); ++m)
            {
                const int st = song.motif[m].first;
                const int next = m + 1 < song.motif.size() ? song.motif[m + 1].first : 16;
                const int tick = absBar * barTicks + (int) (st * step * stretch);
                if (stretch > 1.0f && tick >= (absBar + 1) * barTicks) break;
                song.tracks[Melody].push_back ({ tick, (int) ((next - st) * step * stretch * 0.9f), pitch (ch + song.motif[m].second + transpose, octave), vel });
            }
        };
        if (s.melodic > 0.2f)
            for (int bar = 0; bar < sec.bars; ++bar)
            {
                const int absBar = sec.startBar + bar;
                switch (t)
                {
                    case Intro:       if (bar % 2 == 0) motifAt (absBar, 4, 55, 2.0f, 0); break;
                    case Verse:       if (! s.vocalSpace || bar % 4 == 3) motifAt (absBar, 5, 60, 1.0f, 0); break;
                    case Pre:         motifAt (absBar, 5, 70 + bar * 8, 1.0f, bar); break;              // sequencia ascendente
                    case Chorus:      motifAt (absBar, 5, 100, 1.0f, 0); break;
                    case FinalChorus: motifAt (absBar, 5, 105, 1.0f, 0); motifAt (absBar, 6, 85, 1.0f, 0); break;   // stack
                    case Bridge:      if (bar % 2 == 0) motifAt (absBar, 4, 60, 2.0f, 0); break;
                    case Drop:        motifAt (absBar, 4, 110, 1.0f, 0); break;
                    default: break;
                }
            }

        // ---------------- SYNTH RIFF: preenche os espacos entre os kicks (call & response); grave no breakdown
        const bool riffOn = t != Intro && t != Outro && ! (t == Verse && s.vocalSpace && s.electronic < 0.5f);
        for (int st = 0; st < 16; ++st)
        {
            const bool kick = pat.cells[DrumRow::Kick][(size_t) st].load() >= 70;
            const bool accent = std::any_of (song.motif.begin(), song.motif.end(), [st] (auto& m) { return m.first == st; });
            bool on = false; int deg = 0;
            if (heavy)        { on = kick || accent; deg = accent ? 0 : 4; }                       // riff grave travado ao kick
            else              { on = (! kick && (st % 2 == 1 || accent)) && rng.nextFloat() < 0.5f + 0.4f * e; deg = accent ? 2 : (st % 4 == 3 ? 4 : 0); }
            sec.riffOn[(size_t) st] = (uint8_t) on;
            sec.riffDegree[(size_t) st] = (int8_t) deg;
        }
        if (riffOn)
            for (int bar = 0; bar < sec.bars; ++bar)
            {
                const int absBar = sec.startBar + bar, ch = song.chordDegree[(size_t) absBar];
                for (int st = 0; st < 16; ++st)
                    if (sec.riffOn[(size_t) st])
                        song.tracks[Riff].push_back ({ absBar * barTicks + st * step, step * 3 / 4, pitch (ch + sec.riffDegree[(size_t) st], heavy ? 2 : 4),
                                                       (int) (70 + 40 * e + (st % 4 == 0 ? 10 : 0)) });
                // AUTOMACAO: filtro (CC74) abre ao longo do pre-chorus
                if (t == Pre)
                    for (int q = 0; q < 4; ++q)
                        song.ccs[Riff].push_back ({ absBar * barTicks + q * ppq, 74, juce::jlimit (0, 127, 30 + (bar * 4 + q) * 97 / juce::jmax (1, sec.bars * 4 - 1)) });
            }

        // ---------------- ARP MODERNO: tons do acorde com pausas, saltos de oitava, repeticoes
        const bool arpOn = ! s.quickIdea && (t == Pre || big || t == Bridge || (t == Verse && ! s.vocalSpace));
        if (arpOn)
            for (int bar = 0; bar < sec.bars; ++bar)
            {
                const int absBar = sec.startBar + bar, ch = song.chordDegree[(size_t) absBar];
                const int tones[] { 0, 2, 4, 7 };
                for (int st = 0; st < 16; ++st)
                {
                    if (rng.nextFloat() < (t == Bridge ? 0.5f : 0.22f)) continue;                     // pausas (gaps ritmicos)
                    int deg = tones[(st + bar) % 4];
                    const int oct = (st % 8 == 6 && rng.nextBool()) ? 6 : 5;                          // salto de oitava
                    if (st > 0 && rng.nextFloat() < 0.15f) deg = tones[(st - 1 + bar) % 4];            // repeticao
                    song.tracks[Arp].push_back ({ absBar * barTicks + st * step, step / 2, pitch (ch + deg, oct), st % 4 == 0 ? 100 : 72 });
                }
            }
    }

    // FX nas transicoes: riser antes do refrao/drop, impacto na entrada, sub drop no breakdown, stutter, downlifter
    // notas: C3 riser | D3 impact | E3 sub drop | F3 downlifter | G3 stutter | A3 reverse | B3 texture
    static void buildFx (SongData& song, const SongSettings& s)
    {
        using namespace Song;
        auto fx = [&] (int bar, int beats, int lenBeats, int note, int vel)
        { song.tracks[Fx].push_back ({ bar * barTicks + beats * ppq, lenBeats * ppq, note, vel }); };
        for (size_t i = 0; i < song.sections.size(); ++i)
        {
            const auto& sec = song.sections[i];
            const bool strong = sec.type == Chorus || sec.type == FinalChorus || sec.type == Drop || sec.type == Breakdown;
            if (strong)
            {
                fx (sec.startBar, 0, 2, 50, 120);                                                   // IMPACT na entrada
                if (sec.startBar >= 2 && s.fxDensity > 0.2f) fx (sec.startBar - 2, 0, 8, 48, 100);  // RISER 2 compassos antes
                if (sec.startBar >= 1 && s.fxDensity > 0.5f) fx (sec.startBar - 1, 3, 1, 55, 100);  // STUTTER no ultimo tempo
            }
            if (sec.type == Breakdown || sec.type == Drop) fx (sec.startBar, 0, 4, 52, 115);        // SUB DROP
            if ((sec.type == Chorus || sec.type == FinalChorus) && i + 1 < song.sections.size() && s.fxDensity > 0.4f)
                fx (sec.startBar + sec.bars, 0, 4, 53, 90);                                         // DOWNLIFTER na saida
            if ((sec.type == Intro || sec.type == Bridge) && s.texture > 0.2f)
                fx (sec.startBar, 0, sec.bars * 4, 59, (int) (40 + 60 * s.texture));               // TEXTURA de fundo
            if (sec.type != Intro && s.texture > 0.5f && sec.startBar >= 1)
                fx (sec.startBar - 1, 3, 1, 57, 80);                                                // REVERSE antes da secao
        }
    }
};
} // namespace mm
