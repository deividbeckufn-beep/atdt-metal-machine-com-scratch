#include "StepSequencer.h"
#include "Parameters/Parameters.h"

namespace mm
{
void SeqParams::attach (juce::AudioProcessorValueTreeState& s)
{
    auto get = [&s] (const char* id) { auto* p = s.getRawParameterValue (id); jassert (p != nullptr); return p; };
    mode = get (ParamIDs::seqMode);        trigger = get (ParamIDs::seqTrigger);
    rate = get (ParamIDs::seqRate);        feel = get (ParamIDs::timeFeel);
    length = get (ParamIDs::seqLength);    gate = get (ParamIDs::seqGate);
    root = get (ParamIDs::keyRoot);        scale = get (ParamIDs::keyScale);
    octave = get (ParamIDs::seqOctave);    scaleLock = get (ParamIDs::scaleLock);
    arpMode = get (ParamIDs::arpMode);     arpOctaves = get (ParamIDs::arpOctaves);
    humVel = get (ParamIDs::humanizeVel);  humTime = get (ParamIDs::humanizeTime);
    swing = get (ParamIDs::seqSwing);      transpose = get (ParamIDs::seqTranspose);
}

void StepSequencer::prepare (double sr)
{
    sampleRate = sr;
    blockStart = 0;
    internalPpq = 0.0;
    offTime.fill (-1);
    for (auto& p : pending) p.used = false;
    numHeld = numLatched = 0;
    lastPressed = -1;
    arpIndex = 0;
    prevSlide = false; prevNote = -1;
    lastMode = -1;
    wasTransportActive = false;
    uiStep = -1;
}

int StepSequencer::activeCount() const noexcept
{
    int n = 0;
    for (auto t : offTime) if (t >= 0) ++n;
    return n;
}

void StepSequencer::allNotesOff (juce::MidiBuffer& out, int sample)
{
    for (int p = 0; p < 128; ++p)
        if (offTime[(size_t) p] >= 0)
        {
            out.addEvent (juce::MidiMessage::noteOff (1, p), sample);
            offTime[(size_t) p] = -1;
        }
    for (auto& p : pending) p.used = false;
    prevSlide = false;
}

void StepSequencer::schedule (int note, float vel, juce::int64 on, juce::int64 off, bool tie)
{
    for (auto& p : pending)
        if (! p.used)
        {
            p = { on, off, note, vel, tie, true };
            return;
        }
}

void StepSequencer::emitDue (juce::MidiBuffer& out, int numSamples)
{
    const juce::int64 end = blockStart + numSamples;
    for (;;)
    {
        juce::int64 best = std::numeric_limits<juce::int64>::max();
        int kind = -1, idx = -1;
        for (int p = 0; p < 128; ++p)                       // note-offs primeiro em caso de empate
            if (offTime[(size_t) p] >= 0 && offTime[(size_t) p] < end && offTime[(size_t) p] < best)
            { best = offTime[(size_t) p]; kind = 0; idx = p; }
        for (int i = 0; i < maxPending; ++i)
            if (pending[(size_t) i].used && pending[(size_t) i].time < end && pending[(size_t) i].time < best)
            { best = pending[(size_t) i].time; kind = 1; idx = i; }
        if (kind < 0)
            break;

        const int s = juce::jlimit (0, numSamples - 1, (int) (best - blockStart));
        if (kind == 0)
        {
            out.addEvent (juce::MidiMessage::noteOff (1, idx), s);
            offTime[(size_t) idx] = -1;
            continue;
        }

        auto& pe = pending[(size_t) idx];
        pe.used = false;
        const int note = pe.note;
        if (offTime[(size_t) note] >= 0)
        {
            if (pe.tie) { offTime[(size_t) note] = pe.off; continue; }   // slide na mesma nota = liga
            out.addEvent (juce::MidiMessage::noteOff (1, note), s);
        }
        out.addEvent (juce::MidiMessage::noteOn (1, note, juce::jlimit (0.01f, 1.0f, pe.vel)), s);
        offTime[(size_t) note] = juce::jmax (pe.off, pe.time + 1);
    }
}

int StepSequencer::handleInput (const juce::MidiMessage& msg, int pos, juce::MidiBuffer& out, bool useHost, double bps)
{
    const int trig = juce::jlimit (0, 2, (int) params.trigger->load());
    const bool lock = params.scaleLock->load() > 0.5f;
    const int root = (int) params.root->load(), scale = (int) params.scale->load();

    if (msg.isNoteOn())
    {
        const int note = lock ? quantizeToScale (msg.getNoteNumber(), root, scale) : msg.getNoteNumber();
        const bool wasEmpty = numHeld == 0;
        bool present = false;
        for (int i = 0; i < numHeld; ++i) present = present || held[(size_t) i] == note;
        if (! present && numHeld < (int) held.size()) held[(size_t) numHeld++] = note;
        lastPressed = note;

        if (trig == 1)
        {
            if (wasEmpty) numLatched = 0;
            bool inLatch = false;
            for (int i = 0; i < numLatched; ++i) inLatch = inLatch || latched[(size_t) i] == note;
            if (! inLatch && numLatched < (int) latched.size()) latched[(size_t) numLatched++] = note;
        }

        if (wasEmpty && trig != 2)
        {
            arpIndex = 0;
            prevSlide = false;
            if (! useHost)
            {
                internalPpq = -pos * bps;    // relogio interno: o passo 1 cai exatamente nesta tecla
                return pos;
            }
        }
        return -1;
    }

    if (msg.isNoteOff())
    {
        const int raw = msg.getNoteNumber();
        const int q = quantizeToScale (raw, root, scale);
        for (int i = 0; i < numHeld; )
        {
            if (held[(size_t) i] == raw || (lock && held[(size_t) i] == q))
            {
                for (int j = i; j < numHeld - 1; ++j) held[(size_t) j] = held[(size_t) (j + 1)];
                --numHeld;
            }
            else ++i;
        }
        return -1;
    }

    if (msg.isAllNotesOff() || msg.isAllSoundOff())
    {
        numHeld = numLatched = 0;
        allNotesOff (out, pos);
    }
    out.addEvent (msg, pos);   // CCs, pitch bend, aftertouch seguem para o synth
    return -1;
}

void StepSequencer::trigger (juce::int64 k, int sample, double stepSamples, bool transportActive)
{
    const int length = juce::jlimit (1, SequencePattern::maxSteps, (int) params.length->load());
    const int idx = (int) (((k % length) + length) % length);
    const int mode = (int) params.mode->load();
    const int trig = juce::jlimit (0, 2, (int) params.trigger->load());
    const int root = (int) params.root->load(), scale = (int) params.scale->load();

    // notas de origem
    std::array<int, 16> src {};
    int count = 0;
    if (trig == 2)
    {
        if (! transportActive) { uiStep = -1; return; }
        src[0] = 48 + root;       // TRANSPORT: toca a tonica da KEY sem precisar segurar tecla
        count = 1;
    }
    else
    {
        const auto& list = trig == 1 ? latched : held;
        count = trig == 1 ? numLatched : numHeld;
        for (int i = 0; i < count; ++i) src[(size_t) i] = list[(size_t) i];
    }
    if (count == 0) { uiStep = -1; return; }

    uiStep = idx;
    const auto& st = pattern.steps[(size_t) idx];
    if (! st.on.load())
    {
        prevSlide = false;
        return;
    }

    float vel = st.accent.load() ? 1.0f : st.velocity.load();
    if (const float hv = params.humVel->load(); hv > 0.0f)
        vel *= 1.0f + (random.nextFloat() * 2.0f - 1.0f) * hv * 0.5f;
    vel = juce::jlimit (0.05f, 1.0f, vel);

    juce::int64 delay = 0;
    if (const float ht = params.humTime->load(); ht > 0.0f)
        delay = (juce::int64) (random.nextFloat() * ht * 0.3 * stepSamples);

    // SWING: passos pares (contratempos) atrasados; 100% = sensacao de tercina (2/3 + 1/3)
    if ((k & 1) != 0)
        delay += (juce::int64) (params.swing->load() * stepSamples / 3.0);

    const juce::int64 on = blockStart + sample + delay;
    const float gateAmt = juce::jlimit (0.02f, 2.0f, st.gate.load() * params.gate->load());
    const juce::int64 minLen = (juce::int64) (0.005 * sampleRate);
    const juce::int64 len = st.slide.load() ? (juce::int64) stepSamples + 64
                                            : juce::jmax (minLen, (juce::int64) (gateAmt * stepSamples));
    const int octShift = 12 * (int) params.octave->load();
    const bool slideIn = prevSlide;

    const int transposeSemis = (int) params.transpose->load();
    const bool lock = params.scaleLock->load() > 0.5f;
    auto play = [&] (int n, bool allowTie)
    {
        n += octShift + transposeSemis;
        if (lock && transposeSemis != 0)          // TRANSPOSE + SCALE LOCK: continua dentro da escala
            n = quantizeToScale (n, root, scale);
        n = juce::jlimit (0, 127, n);
        schedule (n, vel, on, on + len, allowTie && slideIn && n == prevNote);
        prevNote = n;
    };

    if (mode == 1)   // SEQUENCE: grau da escala relativo a ultima tecla
    {
        const int base = src[(size_t) (count - 1)];
        play (degreeToNote (base, st.degree.load(), root, scale), true);
    }
    else             // ARP: o padrao define o ritmo (on/mute, velocity, gate, accent, slide)
    {
        const int arpMode = juce::jlimit (0, 5, (int) params.arpMode->load());
        const int octaves = juce::jlimit (1, 4, (int) params.arpOctaves->load());

        std::array<int, 16> base = src;
        if (arpMode != 4)   // ORDER mantem a ordem das teclas; os outros ordenam
            std::sort (base.begin(), base.begin() + count);

        std::array<int, 64> list {};
        int n = 0;
        for (int o = 0; o < octaves; ++o)
            for (int i = 0; i < count && n < 64; ++i)
                list[(size_t) n++] = base[(size_t) i] + 12 * o;

        if (arpMode == 5)   // CHORD: todas juntas
        {
            for (int i = 0; i < n; ++i) play (list[(size_t) i], false);
        }
        else
        {
            int note;
            if (arpMode == 3)       note = list[(size_t) random.nextInt (n)];
            else if (arpMode == 1)  note = list[(size_t) (n - 1 - (arpIndex % n))];
            else if (arpMode == 2 && n > 2)
            {
                const int cycle = 2 * n - 2;
                const int p = arpIndex % cycle;
                note = list[(size_t) (p < n ? p : cycle - p)];
            }
            else                    note = list[(size_t) (arpIndex % n)];
            ++arpIndex;
            play (note, true);
        }
    }
    prevSlide = st.slide.load();
}

void StepSequencer::process (const juce::MidiBuffer& in, juce::MidiBuffer& out, int numSamples,
                             double bpm, bool hostPlaying, double hostPpq)
{
    const int mode = juce::jlimit (0, 2, (int) params.mode->load());
    if (mode != lastMode)
    {
        allNotesOff (out, 0);
        lastMode = mode;
        arpIndex = 0;
    }

    if (mode == 0)   // OFF: teclado direto no synth (com SCALE LOCK opcional)
    {
        const bool lock = params.scaleLock->load() > 0.5f;
        const int root = (int) params.root->load(), scale = (int) params.scale->load();
        for (const auto meta : in)
        {
            auto msg = meta.getMessage();
            if (lock && msg.isNoteOnOrOff())
                msg.setNoteNumber (quantizeToScale (msg.getNoteNumber(), root, scale));
            out.addEvent (msg, meta.samplePosition);
        }
        numHeld = numLatched = 0;
        uiStep = -1;
        blockStart += numSamples;
        return;
    }

    bpm = juce::jlimit (20.0, 999.0, bpm);
    static const double rateBeats[] { 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };
    const double stepBeats = rateBeats[juce::jlimit (0, 5, (int) params.rate->load())]
                             / feelMultiplier ((int) params.feel->load());
    const double bps = bpm / 60.0 / sampleRate;
    const double stepSamples = stepBeats / bps;
    const int trig = juce::jlimit (0, 2, (int) params.trigger->load());
    const bool useHost = hostPlaying;
    const bool transportActive = trig == 2 && hostPlaying;

    if (wasTransportActive && ! transportActive)
        allNotesOff (out, 0);
    wasTransportActive = transportActive;

    auto it = in.begin();
    const auto endIt = in.end();
    auto applyUpTo = [&] (int s)
    {
        int reset = -1;
        while (it != endIt && (*it).samplePosition <= s)
        {
            const auto meta = *it;
            const int r = handleInput (meta.getMessage(), meta.samplePosition, out, useHost, bps);
            if (r >= 0) reset = r;
            ++it;
        }
        return reset;
    };

    int pos = 0;
    for (int guard = 0; guard < 64; ++guard)
    {
        const double ppq0 = useHost ? hostPpq : internalPpq;
        auto k = (juce::int64) std::ceil ((ppq0 + pos * bps) / stepBeats - 1.0e-9);
        bool restarted = false;

        for (;;)
        {
            int b = (int) std::llround (((double) k * stepBeats - ppq0) / bps);
            if (b < pos) b = pos;
            if (b >= numSamples) break;

            const int r = applyUpTo (b);
            if (r >= 0) { pos = r; restarted = true; break; }

            trigger (k, b, stepSamples, transportActive);
            ++k;
            pos = b + 1;
        }
        if (restarted) continue;

        const int r = applyUpTo (numSamples - 1);
        if (r >= 0) { pos = r; continue; }
        break;
    }

    emitDue (out, numSamples);
    if (! useHost)
        internalPpq += numSamples * bps;
    blockStart += numSamples;
}
} // namespace mm
