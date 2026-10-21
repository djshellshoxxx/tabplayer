#pragma once

/*  Standard MIDI File export of a PerformanceScore.

    Format 1: track 0 carries the tempo and time-signature meta events (taken
    from track 0 of the score, the same track playback follows), and every
    ScoreTrack gets its own track named after the track. All notes are written
    on channel 1 at quarter-note resolution given by ticksPerQuarter.

    Beats are quarter-note beats. ScoreNote.startBeat is relative to its
    ScoreMeasure, so the absolute position is the sum of the preceding measures'
    lengths (numerator * 4 / denominator quarter beats) plus startBeat.
*/

#include "../core/PerformanceScore.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace tabplayer
{

/** Builds the MIDI file in memory. */
juce::MidiFile buildMidiFile (const PerformanceScore& score, int ticksPerQuarter = 480);

/** Writes the score to a Standard MIDI File. On failure returns false and sets
    error to a human-readable message. */
bool exportMidiFile (const PerformanceScore& score, const juce::File& destination,
                     juce::String& error, int ticksPerQuarter = 480);

} // namespace tabplayer
