#pragma once

/*  Pure transforms on a PerformanceScore. Each function takes the score by
    const reference and returns a modified copy; the input is never changed.
*/

#include "../core/PerformanceScore.h"

#include <vector>

namespace tabplayer
{

/** Moves every note by semitones. The fret moves with the pitch when the new
    fret is still 0..24 on the same string. When it would fall outside that
    range the fret is left as it was and only the pitch moves, so such a note
    no longer matches its string's tuning. midiNote is clamped to 0..127 and
    pitchHz follows the actual change. */
PerformanceScore transpose (const PerformanceScore& score, int semitones);

/** Deterministic jitter: the same seed and score always give the same result.
    Each note's startBeat moves by up to +/- timingBeats and is clamped to be
    at least 0 (relative to its measure). Each velocity moves by up to
    +/- velocityAmount (velocity is 0..1) and is clamped to 0.05..1.
    Durations are unchanged. */
PerformanceScore humanize (const PerformanceScore& score, double timingBeats,
                           double velocityAmount, juce::uint32 seed);

/** Snaps each note's start to the nearest multiple of gridBeats on the track's
    absolute beat timeline, which is the measure grid when gridBeats divides the
    measure length. Durations are unchanged. A non-positive grid returns an
    unchanged copy. */
PerformanceScore quantize (const PerformanceScore& score, double gridBeats);

//==============================================================================
/** Names the chord a set of MIDI notes forms, from the pitch classes present
    regardless of octave or order. Inversions are recognised because every
    pitch class is tried as the root. Returns names such as "Am", "G7", "Cmaj7",
    "Dsus4", "E5" (power chord, two pitch classes a fifth apart), or "" when
    nothing matches. Recognised shapes: major, minor, diminished, augmented,
    sus2, sus4, 5, 6, m6, 7, maj7, m7, mMaj7, m7b5, dim7, 7sus4 and 9. A
    seventh-chord or triad with its fifth left out is still named. Slash bass
    names are not produced. */
class ChordDetector
{
public:
    static juce::String detectChord (const std::vector<int>& midiNotes);
};

} // namespace tabplayer
