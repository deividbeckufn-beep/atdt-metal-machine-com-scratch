#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "DrumPattern.h"

namespace mm
{
// =============================================================================
//  BeatMidi - exporta o padrao da bateria como arquivo MIDI real (.mid)
//  - Bateria no mapa GM (canal 10): 36 kick, 38 snare, 39 clap, 42 hat, 46 open hat,
//    37 perc, 45 tom, 49 crash, 51 ride, 52 fx
//  - 808 no canal 2, notas da escala (legato: slides funcionam em qualquer 808)
//  - RATCHET vira notas reais do roll
//  - multiTrack = MIDI tipo 1 com uma trilha por instrumento (o REAPER cria trilhas separadas)
// =============================================================================
class BeatMidi
{
public:
    static constexpr int ppq = 960;

    static int gmNote (int row)
    {
        static const int n[] { 36, 38, 39, 42, 46, 37, 45, 49, 51, 52, 0 };
        return n[juce::jlimit (0, DrumRow::Count - 1, row)];
    }

    static juce::MidiFile create (const DrumPattern& p, int length, double bpm, int root, bool multiTrack, int repeats = 1)
    {
        juce::MidiFile file;
        file.setTicksPerQuarterNote (ppq);
        const int stepTicks = ppq / 4;
        const int total = length * juce::jmax (1, repeats);

        juce::MidiMessageSequence tempo;
        tempo.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / juce::jlimit (20.0, 999.0, bpm))), 0);
        tempo.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
        tempo.addEvent (juce::MidiMessage::textMetaEvent (3, "ATDT SYNTH MACHINE - " + p.name), 0);
        file.addTrack (tempo);

        auto rowEvents = [&] (int r, juce::MidiMessageSequence& seq)
        {
            const bool is808 = r == DrumRow::Bass808;
            const int channel = is808 ? 2 : 10;
            for (int i = 0; i < total; ++i)
            {
                const int st = i % length;
                const uint8_t vel = p.cells[(size_t) r][(size_t) st].load();
                if (vel == 0) continue;
                const int rt = juce::jlimit (1, 4, (int) p.ratchet[(size_t) r][(size_t) st].load());
                for (int j = 0; j < rt; ++j)
                {
                    const double on = (double) (i * stepTicks + j * stepTicks / rt);
                    const uint8_t v = (uint8_t) juce::jlimit (1, 127, juce::roundToInt (vel * (1.0f - 0.1f * j)));
                    int note = gmNote (r), lenTicks = stepTicks / rt / 2;
                    if (is808)
                    {
                        note = juce::jlimit (0, 127, 24 + root + p.bassNote[(size_t) st].load());
                        int next = i + 1;                       // legato ate o proximo golpe do 808
                        while (next < total && p.cells[(size_t) r][(size_t) (next % length)].load() == 0) ++next;
                        lenTicks = (next - i) * stepTicks;
                    }
                    seq.addEvent (juce::MidiMessage::noteOn (channel, note, v), on);
                    seq.addEvent (juce::MidiMessage::noteOff (channel, note), on + lenTicks);
                }
            }
            seq.updateMatchedPairs();
        };

        if (multiTrack)
        {
            for (int r = 0; r < DrumRow::Count; ++r)
            {
                juce::MidiMessageSequence seq;
                seq.addEvent (juce::MidiMessage::textMetaEvent (3, drumRowNames()[r]), 0);   // nome da trilha
                rowEvents (r, seq);
                if (seq.getNumEvents() > 1) file.addTrack (seq);
            }
        }
        else
        {
            juce::MidiMessageSequence drums, bass;
            drums.addEvent (juce::MidiMessage::textMetaEvent (3, "Drums"), 0);
            bass.addEvent (juce::MidiMessage::textMetaEvent (3, "808"), 0);
            for (int r = 0; r < DrumRow::Count; ++r) rowEvents (r, r == DrumRow::Bass808 ? bass : drums);
            drums.sort(); bass.sort();
            file.addTrack (drums);
            if (bass.getNumEvents() > 1) file.addTrack (bass);
        }
        return file;
    }

    static bool write (const juce::MidiFile& f, const juce::File& dest)
    {
        dest.getParentDirectory().createDirectory();
        dest.deleteFile();
        juce::FileOutputStream out (dest);
        return out.openedOk() && f.writeTo (out, 1);
    }
};
} // namespace mm
