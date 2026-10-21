#include "ScoreTransforms.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <set>

namespace tabplayer
{

namespace
{
    constexpr int kMaxFret = 24;

    double measureLengthBeats (int numerator, int denominator) noexcept
    {
        return (double) juce::jmax (1, numerator) * 4.0 / (double) juce::jmax (1, denominator);
    }

    /** splitmix64: a fixed, platform-independent generator, so a seed always
        produces the same sequence. */
    class SeededRandom
    {
    public:
        explicit SeededRandom (juce::uint32 seed) noexcept
            : state ((juce::uint64) seed * 0x9E3779B97F4A7C15ull + 0x632BE59BD9B4E019ull) {}

        /** A value in [0, 1). */
        double next() noexcept
        {
            state += 0x9E3779B97F4A7C15ull;
            juce::uint64 z = state;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            z ^= z >> 31;
            return (double) (z >> 11) * (1.0 / 9007199254740992.0);
        }

    private:
        juce::uint64 state;
    };

    struct ChordShape
    {
        const char* suffix;
        std::vector<int> intervals;   // semitones above the root, root included
        bool fifthMayBeOmitted;
    };

    const std::vector<ChordShape>& getChordShapes()
    {
        static const std::vector<ChordShape> shapes {
            { "",      { 0, 4, 7 },        true  },
            { "m",     { 0, 3, 7 },        true  },
            { "dim",   { 0, 3, 6 },        false },
            { "aug",   { 0, 4, 8 },        false },
            { "sus2",  { 0, 2, 7 },        true  },
            { "sus4",  { 0, 5, 7 },        true  },
            { "5",     { 0, 7 },           false },
            { "6",     { 0, 4, 7, 9 },     false },
            { "m6",    { 0, 3, 7, 9 },     false },
            { "7",     { 0, 4, 7, 10 },    true  },
            { "maj7",  { 0, 4, 7, 11 },    true  },
            { "m7",    { 0, 3, 7, 10 },    true  },
            { "mMaj7", { 0, 3, 7, 11 },    true  },
            { "m7b5",  { 0, 3, 6, 10 },    false },
            { "dim7",  { 0, 3, 6, 9 },     false },
            { "7sus4", { 0, 5, 7, 10 },    true  },
            { "9",     { 0, 2, 4, 7, 10 }, false },
        };
        return shapes;
    }

    const char* const kRootNames[12] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

//==============================================================================
PerformanceScore transpose (const PerformanceScore& score, int semitones)
{
    PerformanceScore out (score);

    for (int t = 0; t < out.getNumTracks(); ++t)
    {
        for (auto& measure : out.getTrack (t).measures)
        {
            for (auto& voice : measure.voices)
            {
                for (auto& note : voice.notes)
                {
                    const int newMidi = juce::jlimit (0, 127, note.midiNote + semitones);
                    const int delta = newMidi - note.midiNote;

                    const int newFret = note.fret + delta;
                    if (newFret >= 0 && newFret <= kMaxFret)
                        note.fret = newFret;

                    note.midiNote = newMidi;
                    note.pitchHz *= std::pow (2.0, (double) delta / 12.0);
                }
            }
        }
    }

    return out;
}

PerformanceScore humanize (const PerformanceScore& score, double timingBeats,
                           double velocityAmount, juce::uint32 seed)
{
    PerformanceScore out (score);
    SeededRandom random (seed);

    const double timing = juce::jmax (0.0, timingBeats);
    const double velocityJitter = juce::jmax (0.0, velocityAmount);

    for (int t = 0; t < out.getNumTracks(); ++t)
    {
        for (auto& measure : out.getTrack (t).measures)
        {
            for (auto& voice : measure.voices)
            {
                for (auto& note : voice.notes)
                {
                    // Both draws happen for every note, so the sequence does not
                    // depend on which amounts are zero.
                    const double timingOffset = (random.next() * 2.0 - 1.0) * timing;
                    const double velocityOffset = (random.next() * 2.0 - 1.0) * velocityJitter;

                    note.startBeat = juce::jmax (0.0, note.startBeat + timingOffset);
                    note.velocity = juce::jlimit (0.05, 1.0, note.velocity + velocityOffset);
                }
            }
        }
    }

    return out;
}

PerformanceScore quantize (const PerformanceScore& score, double gridBeats)
{
    PerformanceScore out (score);

    if (! (gridBeats > 0.0))
        return out;

    for (int t = 0; t < out.getNumTracks(); ++t)
    {
        double measureBase = 0.0;

        for (auto& measure : out.getTrack (t).measures)
        {
            for (auto& voice : measure.voices)
            {
                for (auto& note : voice.notes)
                {
                    const double absolute = measureBase + note.startBeat;
                    const double snapped = std::round (absolute / gridBeats) * gridBeats;
                    note.startBeat = juce::jmax (0.0, snapped - measureBase);
                }
            }

            measureBase += measureLengthBeats (measure.timeSignatureNumerator, measure.timeSignatureDenominator);
        }
    }

    return out;
}

//==============================================================================
juce::String ChordDetector::detectChord (const std::vector<int>& midiNotes)
{
    std::array<bool, 12> present {};
    int lowest = 0;
    bool any = false;

    for (const int note : midiNotes)
    {
        const int pitchClass = ((note % 12) + 12) % 12;
        present[(size_t) pitchClass] = true;

        if (! any || note < lowest)
            lowest = note;
        any = true;
    }

    if (! any)
        return {};

    const int bass = ((lowest % 12) + 12) % 12;

    std::set<int> pitchClasses;
    for (int pc = 0; pc < 12; ++pc)
        if (present[(size_t) pc])
            pitchClasses.insert (pc);

    if (pitchClasses.size() < 2)
        return {};

    // Best candidate: exact shape beats a shape with the fifth omitted, and
    // among equal shapes a root in the bass wins. Ties keep the first found.
    int bestScore = 0;
    juce::String best;

    for (int root = 0; root < 12; ++root)
    {
        if (! present[(size_t) root])
            continue;

        std::set<int> relative;
        for (const int pc : pitchClasses)
            relative.insert ((pc - root + 12) % 12);

        for (const auto& shape : getChordShapes())
        {
            const std::set<int> target (shape.intervals.begin(), shape.intervals.end());

            int quality = 0;
            if (relative == target)
            {
                quality = 2;
            }
            else if (shape.fifthMayBeOmitted && target.count (7) != 0 && relative.size() >= 3)
            {
                std::set<int> withoutFifth = target;
                withoutFifth.erase (7);

                if (relative == withoutFifth)
                    quality = 1;
            }

            if (quality == 0)
                continue;

            const int score = quality * 2 + (root == bass ? 1 : 0);
            if (score > bestScore)
            {
                bestScore = score;
                best = juce::String (kRootNames[root]) + shape.suffix;
            }
        }
    }

    return best;
}

} // namespace tabplayer
