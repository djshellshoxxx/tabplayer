#pragma once

/*  The performance score (notation-export.md section 1).

    The internal representation of what was played, from which every export
    format is written. The thing that makes it a guitar score rather than a list
    of pitches is rule 2 of notation-export 0: a note knows which string it was
    played on and at which fret, and those are not recoverable from the pitch. E
    on string 3 fret 9 and E on string 4 fret 14 are the same note and a
    different piece of music.

    The score is built on a worker thread from the engine's own event stream, and
    every exporter reads it without modifying it. Nothing here runs on the audio
    thread, which is rule 1.
*/

#include "TabPlayerTypes.h"

#include <vector>

namespace tabplayer
{

//==============================================================================
/** A technique attached to a note (notation-export 1). */
struct ScoreTechnique
{
    enum class Type
    {
        bend = 0, bendRelease, preBend,
        slideUp, slideDown, slideLegato, slideShift, slideIn, slideOut,
        hammerOn, pullOff,
        palmMute, deadNote,
        naturalHarmonic, pinchHarmonic, artificialHarmonic, tapHarmonic,
        tap, vibrato, trill, whammy, ghostNote, accent, staccato, letRing,
        pickStrokeUp, pickStrokeDown,   // auto-articulation.md 9 (FEAT-ASSIST)
        slap, pop,                      // tab-import-export 7: bass slap (thumb) and pop, append-only
        numTypes
    };

    Type type = Type::bend;

    /** What the technique needs: semitones for a bend, a fret for a slide, a
        depth for a palm mute, a rate for vibrato. */
    double value = 0.0;
    double secondValue = 0.0;

    /** Bend and whammy curves: points of (position in the note 0..1, semitones). */
    std::vector<std::pair<double, double>> curve;
};

const char* getTechniqueName (ScoreTechnique::Type type) noexcept;

//==============================================================================
/** One note (notation-export 1). */
struct ScoreNote
{
    double startBeat = 0.0;
    double durationBeats = 1.0;

    int stringIndex = 0;       ///< 0 is the highest string.
    int fret = 0;

    int midiNote = 60;
    double pitchHz = 261.63;
    double velocity = 0.8;

    std::vector<ScoreTechnique> techniques;

    /** auto-articulation.md 9 (FEAT-ASSIST): the aa_rules bits Performance
        Assist applied to this note; 0 when it was played as written. */
    juce::uint16 autoRules = 0;

    bool hasTechnique (ScoreTechnique::Type type) const noexcept;
    const ScoreTechnique* findTechnique (ScoreTechnique::Type type) const noexcept;

    /** True when the note is tied to the one before it. */
    bool tiedFromPrevious = false;
};

//==============================================================================
struct ScoreVoice
{
    std::vector<ScoreNote> notes;
};

//==============================================================================
struct ScoreMeasure
{
    int timeSignatureNumerator = 4;
    int timeSignatureDenominator = 4;

    /** A tempo change at the start of this measure, or 0 for none. */
    double tempoChange = 0.0;

    /** Chord symbols that start in this measure, with the beat they land on. */
    std::vector<std::pair<double, juce::String>> chordSymbols;

    juce::String sectionName;

    std::vector<ScoreVoice> voices;

    /** Every note in the measure, across all voices, sorted by start beat. */
    std::vector<const ScoreNote*> collectNotes() const;
};

//==============================================================================
struct ScoreTrack
{
    juce::String name { "Guitar" };
    juce::String guitarId;

    int capoFret = 0;
    int numStrings = 6;

    /** The MIDI note of each string played open, highest string first. */
    std::array<int, kMaxStrings> tuning { { 64, 59, 55, 50, 45, 40, 0, 0, 0, 0, 0, 0 } };

    std::vector<ScoreMeasure> measures;
};

//==============================================================================
class PerformanceScore
{
public:
    struct Meta
    {
        juce::String title { "TabPlayer Performance" };
        juce::String artist;
        double tempoBpm = 120.0;
        int timeSignatureNumerator = 4;
        int timeSignatureDenominator = 4;
        juce::String key;
        juce::String tuningName { "Standard" };
    };

    PerformanceScore();

    void clear();

    Meta& getMeta() noexcept { return meta; }
    const Meta& getMeta() const noexcept { return meta; }

    int getNumTracks() const noexcept { return (int) tracks.size(); }
    ScoreTrack& getTrack (int index) noexcept;
    const ScoreTrack& getTrack (int index) const noexcept;

    ScoreTrack& addTrack (const juce::String& name = "Guitar");

    /** Total notes across every track, for reporting and for tests. */
    int getTotalNoteCount() const noexcept;

    double getTotalBeats() const noexcept;

    //==========================================================================
    /*  Building a score as it is played (notation-export 1).

        The plugin records notes as they happen and turns them into measures at
        the end, because a note's duration is not known until it is released and
        a measure cannot be closed until the notes that fall in it have all
        arrived. */

    /** Starts a capture. Anything previously captured is discarded. */
    void beginCapture (double tempoBpm, int numerator, int denominator);

    /** Records a note starting. `beat` is from the start of the capture. */
    void noteStarted (int stringIndex, int fret, int midiNote, double pitchHz,
                      double velocity, double beat);

    /** Records a note ending, which is what gives it a duration. */
    void noteEnded (int stringIndex, double beat);

    /** Attaches a technique to whichever note is sounding on a string. */
    void addTechnique (int stringIndex, const ScoreTechnique& technique);

    /** FEAT-ASSIST: the sounding note's Performance Assist bits (9). */
    void setAutoRules (int stringIndex, juce::uint16 rules);

    /** Records a chord symbol at a beat. */
    void addChordSymbol (double beat, const juce::String& symbol);

    /** Turns the captured notes into measures. Call before exporting. */
    void endCapture (double finalBeat);

    bool isCapturing() const noexcept { return capturing; }

    int getNumCapturedNotes() const noexcept { return (int) captured.size(); }

    //==========================================================================
    /** Which measure and beat a position in beats falls in. */
    void beatToMeasure (double beat, int& measureIndex, double& beatInMeasure) const noexcept;

    /** The note name of a MIDI note, for the notation formats that want one. */
    static juce::String getNoteName (int midiNote);

    /** The step, alter and octave MusicXML wants. */
    static void getMusicXmlPitch (int midiNote, juce::String& step, int& alter, int& octave);

private:
    struct CapturedNote
    {
        ScoreNote note;
        bool open = true;
    };

    Meta meta;
    std::vector<ScoreTrack> tracks;

    std::vector<CapturedNote> captured;
    std::array<int, kMaxStrings> soundingIndex {};
    std::vector<std::pair<double, juce::String>> capturedChords;

    bool capturing = false;
    double captureTempo = 120.0;
};

} // namespace tabplayer
