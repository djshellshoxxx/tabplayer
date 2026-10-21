#include "MidiExporter.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace tabplayer
{

namespace
{
    constexpr int kDefaultTicksPerQuarter = 480;

    /** Quarter-note beats in a measure, the same arithmetic playback uses. */
    double measureLengthBeats (int numerator, int denominator) noexcept
    {
        return (double) juce::jmax (1, numerator) * 4.0 / (double) juce::jmax (1, denominator);
    }

    /** MIDI time signatures store the denominator as a power of two. */
    int sanitiseDenominator (int denominator) noexcept
    {
        if (denominator < 1 || (denominator & (denominator - 1)) != 0)
            return 4;

        return denominator;
    }

    struct TimedEvent
    {
        int tick = 0;
        int order = 0;   // 0 meta, 1 note off, 2 note on: note-off first at the same tick
        juce::MidiMessage message;
    };

    struct AbsoluteNote
    {
        double start = 0.0;   // quarter-note beats from the start of the track
        double end = 0.0;
        int midi = 60;
        int string = 0;
        int velocity = 100;
        bool tiedFromPrevious = false;
    };

    /** Turns a track's measures into absolute notes, with ties merged into the
        note they continue. */
    std::vector<AbsoluteNote> collectTrackNotes (const ScoreTrack& track)
    {
        std::vector<AbsoluteNote> notes;

        double measureBase = 0.0;
        for (const auto& measure : track.measures)
        {
            for (const auto& voice : measure.voices)
            {
                for (const auto& note : voice.notes)
                {
                    AbsoluteNote n;
                    n.start = measureBase + juce::jmax (0.0, note.startBeat);
                    n.end = n.start + juce::jmax (0.0, note.durationBeats);
                    n.midi = juce::jlimit (0, 127, note.midiNote);
                    n.string = note.stringIndex;
                    n.velocity = juce::jlimit (1, 127, (int) std::lround (note.velocity * 127.0));
                    n.tiedFromPrevious = note.tiedFromPrevious;
                    notes.push_back (n);
                }
            }

            measureBase += measureLengthBeats (measure.timeSignatureNumerator, measure.timeSignatureDenominator);
        }

        std::stable_sort (notes.begin(), notes.end(), [] (const AbsoluteNote& a, const AbsoluteNote& b)
        {
            if (a.start != b.start)
                return a.start < b.start;
            return a.midi < b.midi;
        });

        // A tied note continues the last note on the same string and pitch that
        // ends where it starts: that note is lengthened and the tie is dropped.
        std::vector<AbsoluteNote> merged;
        std::map<std::pair<int, int>, size_t> lastByKey;

        for (const auto& n : notes)
        {
            const auto key = std::make_pair (n.midi, n.string);

            if (n.tiedFromPrevious)
            {
                auto it = lastByKey.find (key);
                if (it != lastByKey.end() && std::abs (merged[it->second].end - n.start) < 1.0e-4)
                {
                    merged[it->second].end = juce::jmax (merged[it->second].end, n.end);
                    continue;
                }
            }

            merged.push_back (n);
            lastByKey[key] = merged.size() - 1;
        }

        return merged;
    }

    juce::MidiMessageSequence sequenceFromEvents (std::vector<TimedEvent>& events)
    {
        std::stable_sort (events.begin(), events.end(), [] (const TimedEvent& a, const TimedEvent& b)
        {
            if (a.tick != b.tick)
                return a.tick < b.tick;
            return a.order < b.order;
        });

        // MidiMessageSequence::addEvent keeps insertion order for equal
        // timestamps, so the sort above survives into the file.
        juce::MidiMessageSequence sequence;
        for (const auto& e : events)
            sequence.addEvent (e.message);

        return sequence;
    }

    int toTick (double beats, int ticksPerQuarter) noexcept
    {
        return juce::roundToInt (beats * (double) ticksPerQuarter);
    }

    juce::MidiMessageSequence buildTempoTrack (const PerformanceScore& score, int ticksPerQuarter)
    {
        std::vector<TimedEvent> events;

        auto add = [&events] (int tick, juce::MidiMessage message)
        {
            message.setTimeStamp ((double) tick);
            events.push_back ({ tick, 0, message });
        };

        const auto& meta = score.getMeta();
        const ScoreTrack* first = score.getNumTracks() > 0 ? &score.getTrack (0) : nullptr;

        double bpm = meta.tempoBpm > 0.0 ? meta.tempoBpm : 120.0;
        int numerator = meta.timeSignatureNumerator;
        int denominator = sanitiseDenominator (meta.timeSignatureDenominator);

        if (first != nullptr && ! first->measures.empty())
        {
            const auto& m0 = first->measures.front();
            if (m0.tempoChange > 0.0)
                bpm = m0.tempoChange;
            numerator = m0.timeSignatureNumerator;
            denominator = sanitiseDenominator (m0.timeSignatureDenominator);
        }

        auto usPerQuarter = [] (double tempo)
        {
            return juce::jlimit (1, 0xFFFFFF, juce::roundToInt (60000000.0 / tempo));
        };

        add (0, juce::MidiMessage::textMetaEvent (3, meta.title.isEmpty() ? juce::String ("Tempo") : meta.title));
        add (0, juce::MidiMessage::tempoMetaEvent (usPerQuarter (bpm)));
        add (0, juce::MidiMessage::timeSignatureMetaEvent (numerator, denominator));

        if (first != nullptr)
        {
            double currentBpm = bpm;
            int curNumerator = numerator;
            int curDenominator = denominator;
            double measureBase = 0.0;

            for (size_t i = 0; i < first->measures.size(); ++i)
            {
                const auto& m = first->measures[i];
                const int tick = toTick (measureBase, ticksPerQuarter);

                if (i > 0)
                {
                    if (m.tempoChange > 0.0 && m.tempoChange != currentBpm)
                    {
                        currentBpm = m.tempoChange;
                        add (tick, juce::MidiMessage::tempoMetaEvent (usPerQuarter (currentBpm)));
                    }

                    const int mDenominator = sanitiseDenominator (m.timeSignatureDenominator);
                    if (m.timeSignatureNumerator != curNumerator || mDenominator != curDenominator)
                    {
                        curNumerator = m.timeSignatureNumerator;
                        curDenominator = mDenominator;
                        add (tick, juce::MidiMessage::timeSignatureMetaEvent (curNumerator, curDenominator));
                    }
                }

                measureBase += measureLengthBeats (m.timeSignatureNumerator, m.timeSignatureDenominator);
            }
        }

        return sequenceFromEvents (events);
    }

    juce::MidiMessageSequence buildNoteTrack (const ScoreTrack& track, int ticksPerQuarter)
    {
        std::vector<TimedEvent> events;

        auto name = juce::MidiMessage::textMetaEvent (3, track.name.isEmpty() ? juce::String ("Guitar") : track.name);
        name.setTimeStamp (0.0);
        events.push_back ({ 0, 0, name });

        const auto notes = collectTrackNotes (track);
        for (const auto& n : notes)
        {
            const int onTick = toTick (n.start, ticksPerQuarter);
            const int offTick = juce::jmax (onTick + 1, toTick (n.end, ticksPerQuarter));

            auto on = juce::MidiMessage::noteOn (1, n.midi, (juce::uint8) n.velocity);
            on.setTimeStamp ((double) onTick);
            events.push_back ({ onTick, 2, on });

            auto off = juce::MidiMessage::noteOff (1, n.midi, (juce::uint8) 0);
            off.setTimeStamp ((double) offTick);
            events.push_back ({ offTick, 1, off });
        }

        return sequenceFromEvents (events);
    }
}

//==============================================================================
juce::MidiFile buildMidiFile (const PerformanceScore& score, int ticksPerQuarter)
{
    const int tpq = juce::jlimit (1, 32767, ticksPerQuarter > 0 ? ticksPerQuarter : kDefaultTicksPerQuarter);

    juce::MidiFile file;
    file.setTicksPerQuarterNote (tpq);

    file.addTrack (buildTempoTrack (score, tpq));

    for (int t = 0; t < score.getNumTracks(); ++t)
        file.addTrack (buildNoteTrack (score.getTrack (t), tpq));

    return file;
}

bool exportMidiFile (const PerformanceScore& score, const juce::File& destination,
                     juce::String& error, int ticksPerQuarter)
{
    error.clear();

    if (ticksPerQuarter < 1 || ticksPerQuarter > 32767)
    {
        error = "ticksPerQuarter must be between 1 and 32767";
        return false;
    }

    if (destination == juce::File())
    {
        error = "no destination file given";
        return false;
    }

    const auto folder = destination.getParentDirectory().createDirectory();
    if (! folder.wasOk())
    {
        error = "could not create folder " + destination.getParentDirectory().getFullPathName()
                + ": " + folder.getErrorMessage();
        return false;
    }

    const auto midi = buildMidiFile (score, ticksPerQuarter);

    juce::FileOutputStream output (destination);
    if (! output.openedOk())
    {
        error = "could not open " + destination.getFullPathName() + ": " + output.getStatus().getErrorMessage();
        return false;
    }

    output.setPosition (0);
    output.truncate();

    if (! midi.writeTo (output, 1))
    {
        error = "failed to write MIDI data to " + destination.getFullPathName();
        return false;
    }

    output.flush();
    return true;
}

} // namespace tabplayer
