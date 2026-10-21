#include "../src/playback/TechniqueCompiler.h"

#include <cmath>
#include <iostream>
#include <vector>

using namespace tabplayer;

namespace
{
bool expect (bool condition, const char* message)
{
    if (! condition)
        std::cerr << "FAIL: " << message << "\n";
    return condition;
}

bool nearly (double a, double b, double tolerance = 1.0e-6)
{
    return std::abs (a - b) <= tolerance;
}

struct TestNote
{
    double start = 0.0;
    double dur = 1.0;
    int string = 0;
    int fret = 0;
    int midi = 60;
    double velocity = 0.8;
    std::vector<ScoreTechnique> techniques {};
};

ScoreTechnique technique (ScoreTechnique::Type type, double value = 0.0)
{
    ScoreTechnique t;
    t.type = type;
    t.value = value;
    return t;
}

/** Four-measure 4/4 score. Note start beats are absolute; measure-relative offsets are computed here. */
PerformanceScore makeScore (const std::vector<TestNote>& notes, double bpm = 120.0,
                            int measures = 4, int tempoChangeMeasure = -1, double tempoChange = 0.0)
{
    PerformanceScore score;
    score.getMeta().tempoBpm = bpm;

    auto& track = score.getTrack (0);
    track.measures.clear();

    for (int m = 0; m < measures; ++m)
    {
        ScoreMeasure measure;
        measure.voices.resize (1);
        if (m == tempoChangeMeasure)
            measure.tempoChange = tempoChange;
        track.measures.push_back (measure);
    }

    for (const auto& n : notes)
    {
        const int m = (int) std::floor (n.start / 4.0);

        ScoreNote note;
        note.startBeat = n.start - (double) m * 4.0;
        note.durationBeats = n.dur;
        note.stringIndex = n.string;
        note.fret = n.fret;
        note.midiNote = n.midi;
        note.velocity = n.velocity;
        note.techniques = n.techniques;

        track.measures[(size_t) m].voices[0].notes.push_back (note);
    }

    return score;
}

template <typename Predicate>
int findIndex (const CompiledTimeline& timeline, Predicate predicate)
{
    for (size_t i = 0; i < timeline.events.size(); ++i)
        if (predicate (timeline.events[i]))
            return (int) i;

    return -1;
}

int countIf (const CompiledTimeline& timeline, PlaybackEvent::Type type)
{
    int count = 0;
    for (const auto& e : timeline.events)
        if (e.type == type)
            ++count;

    return count;
}
} // namespace

int main()
{
    bool ok = true;
    InstrumentProfile profile; // default: perString channels

    // Same-beat ordering: noteOff, reset pitch-bend, then noteOn.
    {
        InstrumentProfile single;
        single.channelMode = ChannelMode::single;
        single.singleChannel = 1;

        auto score = makeScore ({ { 0.0, 1.0, 0, 0, 60, 0.8, { technique (ScoreTechnique::Type::bend, 1.0) } },
                                  { 1.0, 1.0, 0, 0, 62 } });
        const auto timeline = TechniqueCompiler::compile (score, single);

        const int off = findIndex (timeline, [] (const PlaybackEvent& e) {
            return e.type == PlaybackEvent::Type::noteOff && e.data1 == 60 && nearly (e.beat, 1.0); });
        const int reset = findIndex (timeline, [] (const PlaybackEvent& e) {
            return e.type == PlaybackEvent::Type::pitchBend && e.reset && nearly (e.beat, 1.0); });
        const int on = findIndex (timeline, [] (const PlaybackEvent& e) {
            return e.type == PlaybackEvent::Type::noteOn && e.data1 == 62; });

        ok &= expect (off >= 0 && reset >= 0 && on >= 0, "same-beat events exist");
        ok &= expect (off < reset && reset < on, "noteOff < reset < noteOn at identical beat");

        bool sorted = true;
        for (size_t i = 1; i < timeline.events.size(); ++i)
            sorted = sorted && timeline.events[i - 1].beat <= timeline.events[i].beat;
        ok &= expect (sorted, "timeline is sorted by beat");
    }

    // Keyswitch before note-on at the same beat.
    {
        InstrumentProfile withKs;
        ArticulationMapping mapping;
        mapping.articulation = Articulation::palmMute;
        mapping.keyswitchNote = 24;
        withKs.mappings.push_back (mapping);

        auto score = makeScore ({ { 0.0, 1.0, 0, 0, 40, 0.8, { technique (ScoreTechnique::Type::palmMute) } } });
        const auto timeline = TechniqueCompiler::compile (score, withKs);

        const int ks = findIndex (timeline, [] (const PlaybackEvent& e) {
            return e.type == PlaybackEvent::Type::keyswitch && e.data1 == 24; });
        const int on = findIndex (timeline, [] (const PlaybackEvent& e) {
            return e.type == PlaybackEvent::Type::noteOn && e.data1 == 40; });
        ok &= expect (ks >= 0 && on >= 0 && ks < on, "keyswitch precedes noteOn at the same beat");

        const auto plain = TechniqueCompiler::compile (score, InstrumentProfile {});
        ok &= expect (findIndex (plain, [] (const PlaybackEvent& e) {
            return e.type == PlaybackEvent::Type::keyswitch; }) < 0,
            "unknown (unmapped) articulation degrades to a plain note");
    }

    // Velocity bounds.
    {
        auto score = makeScore ({ { 0.0, 1.0, 0, 0, 40, 0.0 },
                                  { 1.0, 1.0, 1, 0, 41, 1.0 },
                                  { 2.0, 1.0, 2, 0, 42, 1.0, { technique (ScoreTechnique::Type::accent) } },
                                  { 3.0, 1.0, 3, 0, 43, 1.0, { technique (ScoreTechnique::Type::ghostNote) } } });
        const auto timeline = TechniqueCompiler::compile (score, profile);

        bool bounded = true;
        for (const auto& e : timeline.events)
            if (e.type == PlaybackEvent::Type::noteOn)
                bounded = bounded && e.data2 >= 1 && e.data2 <= 127;

        ok &= expect (bounded, "velocities stay within 1..127");

        const int lowest = findIndex (timeline, [] (const PlaybackEvent& e) {
            return e.type == PlaybackEvent::Type::noteOn && e.data1 == 40; });
        const int highest = findIndex (timeline, [] (const PlaybackEvent& e) {
            return e.type == PlaybackEvent::Type::noteOn && e.data1 == 41; });
        ok &= expect (lowest >= 0 && timeline.events[(size_t) lowest].data2 == 1, "zero velocity clamps to 1");
        ok &= expect (highest >= 0 && timeline.events[(size_t) highest].data2 == 127, "full velocity is 127");
    }

    // Per-string channels (perString) are distinct.
    {
        std::vector<TestNote> notes;
        for (int s = 0; s < 6; ++s)
            notes.push_back ({ 0.0, 1.0, s, 0, 40 + s });

        const auto timeline = TechniqueCompiler::compile (makeScore (notes), profile);
        std::uint32_t seen = 0;
        bool perString = true;
        for (const auto& e : timeline.events)
            if (e.type == PlaybackEvent::Type::noteOn)
            {
                perString = perString && e.channel == e.stringIndex + 1;
                seen |= 1u << (unsigned) e.channel;
            }

        ok &= expect (perString, "string N plays on channel N+1");
        ok &= expect (seen == 0x7Eu, "six distinct per-string channels (1..6)");
    }

    // MPE lower zone: channels 2..16, one per string.
    {
        InstrumentProfile mpe;
        mpe.channelMode = ChannelMode::mpeLowerZone;
        mpe.pitchBendRangeSemitones = 48.0;

        std::vector<TestNote> notes;
        for (int s = 0; s < 6; ++s)
            notes.push_back ({ 0.0, 1.0, s, 0, 40 + s });
        notes.push_back ({ 0.0, 1.0, 20, 0, 70 }); // beyond the zone: clamps to channel 16

        const auto timeline = TechniqueCompiler::compile (makeScore (notes), mpe);
        bool inZone = true;
        int channelsSeen = 0;
        for (const auto& e : timeline.events)
            if (e.type == PlaybackEvent::Type::noteOn)
            {
                inZone = inZone && e.channel >= 2 && e.channel <= 16;
                ++channelsSeen;
            }

        ok &= expect (inZone, "MPE notes stay in the lower zone (2..16)");
        ok &= expect (channelsSeen == 7, "all MPE notes emitted");
        ok &= expect (findIndex (timeline, [] (const PlaybackEvent& e) {
            return e.type == PlaybackEvent::Type::noteOn && e.data1 == 40 && e.channel == 2; }) >= 0,
            "string 0 maps to channel 2 in MPE");
    }

    // Single channel mode.
    {
        InstrumentProfile single;
        single.channelMode = ChannelMode::single;
        single.singleChannel = 3;

        std::vector<TestNote> notes = { { 0.0, 1.0, 0, 0, 40 }, { 1.0, 1.0, 4, 0, 44 } };
        const auto timeline = TechniqueCompiler::compile (makeScore (notes), single);
        bool allThree = true;
        for (const auto& e : timeline.events)
            allThree = allThree && (e.type != PlaybackEvent::Type::noteOn || e.channel == 3);
        ok &= expect (allThree, "single channel mode uses the configured channel");
    }

    // Pitch-bend normalisation.
    {
        ok &= expect (TechniqueCompiler::semitonesToPitchBend14 (2.0, 2.0) == 16383, "+range maps to 16383");
        ok &= expect (TechniqueCompiler::semitonesToPitchBend14 (0.0, 2.0) == 8192, "zero maps to centre");
        ok &= expect (TechniqueCompiler::semitonesToPitchBend14 (-2.0, 2.0) == 1, "-range maps to 1");
        ok &= expect (std::abs (TechniqueCompiler::semitonesToPitchBend14 (1.0, 2.0) - 12288) <= 1,
                      "half range maps to 3/4 of the 14-bit span");
        ok &= expect (TechniqueCompiler::semitonesToPitchBend14 (9.0, 2.0) == 16383, "bend clamps to range");
    }

    // A bend emits pitch-bend events, then a centre reset at the end of the note.
    {
        auto score = makeScore ({ { 0.0, 2.0, 0, 0, 64, 0.8, { technique (ScoreTechnique::Type::bend, 2.0) } } });
        const auto timeline = TechniqueCompiler::compile (score, profile);

        int bends = 0;
        double peak = 0.0;
        for (const auto& e : timeline.events)
            if (e.type == PlaybackEvent::Type::pitchBend && ! e.reset)
            {
                ++bends;
                peak = juce::jmax (peak, e.bendSemitones);
            }

        ok &= expect (bends > 4, "bend emits pitch-bend events");
        ok &= expect (nearly (peak, 2.0, 1.0e-6), "bend reaches its semitone value");
        ok &= expect (findIndex (timeline, [] (const PlaybackEvent& e) {
            return e.type == PlaybackEvent::Type::pitchBend && e.reset && e.pitchBend14 == 8192 && nearly (e.beat, 2.0); }) >= 0,
            "bend releases to centre at note end");

        InstrumentProfile noBends = profile;
        noBends.sendBends = false;
        const auto plain = TechniqueCompiler::compile (score, noBends);
        ok &= expect (countIf (plain, PlaybackEvent::Type::pitchBend) == 0, "sendBends=false suppresses bends");
    }

    // Vibrato produces a varying pitch-bend LFO.
    {
        auto score = makeScore ({ { 0.0, 2.0, 0, 0, 64, 0.8, { technique (ScoreTechnique::Type::vibrato) } } });
        const auto timeline = TechniqueCompiler::compile (score, profile);

        int count = 0;
        bool varying = false;
        int first = -2;
        for (const auto& e : timeline.events)
            if (e.type == PlaybackEvent::Type::pitchBend && ! e.reset)
            {
                ++count;
                if (first == -2)
                    first = e.pitchBend14;
                varying = varying || e.pitchBend14 != first;
            }

        ok &= expect (count >= 10, "vibrato emits a dense pitch-bend LFO");
        ok &= expect (varying, "vibrato values vary");
    }

    // Slide ramps into the note.
    {
        auto score = makeScore ({ { 0.0, 1.0, 0, 0, 64, 0.8, { technique (ScoreTechnique::Type::slideUp, 2.0) } } });
        const auto timeline = TechniqueCompiler::compile (score, profile);
        const int firstBend = findIndex (timeline, [] (const PlaybackEvent& e) {
            return e.type == PlaybackEvent::Type::pitchBend && ! e.reset; });
        ok &= expect (firstBend >= 0 && timeline.events[(size_t) firstBend].bendSemitones < -1.9,
                      "slide up starts about a fret-width below the note");
    }

    // Palm mute shortens the note.
    {
        auto score = makeScore ({ { 0.0, 4.0, 0, 0, 40, 0.8, { technique (ScoreTechnique::Type::palmMute) } },
                                  { 4.0, 4.0, 1, 0, 45 } });
        const auto timeline = TechniqueCompiler::compile (score, profile);

        const int muteOff = findIndex (timeline, [] (const PlaybackEvent& e) {
            return e.type == PlaybackEvent::Type::noteOff && e.data1 == 40; });
        const int plainOff = findIndex (timeline, [] (const PlaybackEvent& e) {
            return e.type == PlaybackEvent::Type::noteOff && e.data1 == 45; });

        ok &= expect (muteOff >= 0 && nearly (timeline.events[(size_t) muteOff].beat, 2.0),
                      "palm mute halves a four-beat note");
        ok &= expect (plainOff >= 0 && nearly (timeline.events[(size_t) plainOff].beat, 8.0),
                      "plain note keeps its duration");

        const int muteOn = findIndex (timeline, [] (const PlaybackEvent& e) {
            return e.type == PlaybackEvent::Type::noteOn && e.data1 == 40; });
        ok &= expect (timeline.events[(size_t) muteOn].data2 < 127 * 0.8 + 1,
                      "palm mute reduces velocity");
    }

    // Hammer-on: the previous note on the same string rings into the next by the profile overlap.
    {
        auto score = makeScore ({ { 0.0, 1.0, 0, 0, 60 },
                                  { 1.0, 1.0, 0, 2, 62, 0.8, { technique (ScoreTechnique::Type::hammerOn) } } });
        const auto timeline = TechniqueCompiler::compile (score, profile);

        const int off = findIndex (timeline, [] (const PlaybackEvent& e) {
            return e.type == PlaybackEvent::Type::noteOff && e.data1 == 60; });
        ok &= expect (off >= 0 && nearly (timeline.events[(size_t) off].beat, 1.0 + profile.legatoOverlapBeats),
                      "hammer-on overlaps the previous note");
    }

    // Tempo map from per-measure tempo changes.
    {
        auto score = makeScore ({}, 120.0, 4, 2, 60.0); // measure 2 (beat 8) drops to 60 BPM
        const auto tempo = TechniqueCompiler::buildTempoMap (score);

        ok &= expect (nearly (tempo.beatToSeconds (8.0), 4.0), "8 beats at 120 BPM is 4 s");
        ok &= expect (nearly (tempo.beatToSeconds (12.0), 8.0), "4 beats at 60 BPM after the change");
        ok &= expect (nearly (tempo.secondsToBeat (8.0), 12.0), "secondsToBeat inverts beatToSeconds");
        ok &= expect (nearly (tempo.bpmAt (9.0), 60.0) && nearly (tempo.bpmAt (3.0), 120.0), "bpmAt follows the map");
    }

    // Determinism: compiling the same inputs gives the same events.
    {
        auto score = makeScore ({ { 0.0, 1.0, 0, 0, 40, 0.8, { technique (ScoreTechnique::Type::vibrato) } },
                                  { 1.0, 1.0, 1, 3, 45, 0.5, { technique (ScoreTechnique::Type::bend, 1.0) } } });
        const auto a = TechniqueCompiler::compile (score, profile);
        const auto b = TechniqueCompiler::compile (score, profile);

        bool same = a.events.size() == b.events.size();
        for (size_t i = 0; same && i < a.events.size(); ++i)
            same = a.events[i].beat == b.events[i].beat && a.events[i].type == b.events[i].type
                   && a.events[i].channel == b.events[i].channel && a.events[i].data1 == b.events[i].data1
                   && a.events[i].data2 == b.events[i].data2 && a.events[i].pitchBend14 == b.events[i].pitchBend14;
        ok &= expect (same, "compile is deterministic");
    }

    std::cout << (ok ? "TechniqueCompilerTests passed\n" : "TechniqueCompilerTests FAILED\n");
    return ok ? 0 : 1;
}
