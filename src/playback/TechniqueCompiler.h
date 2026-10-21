#pragma once

#include "../core/PerformanceScore.h"
#include "../profile/InstrumentProfile.h"

#include <cstdint>
#include <vector>

namespace tabplayer
{
//==============================================================================
/** Tempo map built from the score (per-measure tempoChange). Beats are quarter notes.
    Lookups do not allocate and are safe to call on the audio thread. */
struct TempoMap
{
    struct Segment
    {
        double beat = 0.0;     ///< where the segment starts
        double bpm = 120.0;
        double seconds = 0.0;  ///< score time at the start of the segment
    };

    std::vector<Segment> segments;  ///< sorted, first segment at beat 0

    double initialBpm() const noexcept;
    double bpmAt (double beat) const noexcept;
    double beatToSeconds (double beat) const noexcept;
    double secondsToBeat (double seconds) const noexcept;
};

//==============================================================================
/** One compiled MIDI-level event. Ordering at equal beats follows the spec (section 6). */
struct PlaybackEvent
{
    enum class Type : std::uint8_t { noteOn = 0, noteOff, pitchBend, cc, programChange, keyswitch };

    double beat = 0.0;
    Type type = Type::noteOn;
    int channel = 1;        ///< 1..16
    int data1 = 0;          ///< note number, CC number or program number
    int data2 = 0;          ///< velocity or CC value
    int pitchBend14 = 8192; ///< pitch-bend position (0..16383), centre 8192
    int stringIndex = -1;   ///< -1 when not from a string
    int fret = 0;
    double bendSemitones = 0.0;
    bool reset = false;     ///< CC/pitch-bend reset (ranks before ordinary pitch bends)
    int sequence = 0;       ///< stable tie-break after the ordering rank
};

/** A note span, for UI queries (active notes). */
struct NoteSpan
{
    double startBeat = 0.0;
    double endBeat = 0.0;
    int midiNote = 60;
    int channel = 1;
    int stringIndex = -1;
    int fret = 0;
    int velocity = 1;
};

/** Immutable compiled result. */
struct CompiledTimeline
{
    std::vector<PlaybackEvent> events;  ///< sorted by beat, then ordering rank
    std::vector<NoteSpan> notes;
    TempoMap tempo;
    double totalBeats = 0.0;
    double beatsPerBar = 4.0;           ///< quarter-note beats in the first measure
    std::uint32_t channelMask = 0;      ///< bit (channel) set for every channel used (1..16)
    int programChange = -1;             ///< from the profile, -1 for none
    std::vector<std::pair<int, int>> resetCcs; ///< (cc number, off value) from the profile
};

//==============================================================================
class TechniqueCompiler
{
public:
    /** Compiles track 0 of the score with the given instrument profile. Not for the audio thread. */
    static CompiledTimeline compile (const PerformanceScore& score, const InstrumentProfile& profile);

    /** Builds the tempo map from the score's meta tempo and per-measure tempo changes. */
    static TempoMap buildTempoMap (const PerformanceScore& score);

    /** Semitones (clamped to +/- range) to a 14-bit pitch-bend position. */
    static int semitonesToPitchBend14 (double semitones, double rangeSemitones) noexcept;

    /** Ordering rank at an identical beat: 0 noteOff, 1 CC/reset, 2 pitchBend,
        3 programChange/keyswitch, 4 noteOn, 5 expression. */
    static int rank (PlaybackEvent::Type type, bool isReset) noexcept;

    /** Sorts events by beat, then rank, channel, data1 and sequence (deterministic). */
    static void sortEvents (std::vector<PlaybackEvent>& events);
};
}
