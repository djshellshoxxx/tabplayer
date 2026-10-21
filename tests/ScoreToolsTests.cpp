#include "../src/core/PerformanceScore.h"
#include "../src/import/TabImportPipeline.h"
#include "../src/tools/MidiExporter.h"
#include "../src/tools/ScoreTransforms.h"

#include <cmath>
#include <iostream>
#include <vector>

using namespace tabplayer;

static bool expect (bool condition, const char* message)
{
    if (! condition)
        std::cerr << "FAIL: " << message << "\n";
    return condition;
}

static std::vector<const ScoreNote*> allNotes (const PerformanceScore& score)
{
    std::vector<const ScoreNote*> notes;

    for (int t = 0; t < score.getNumTracks(); ++t)
        for (const auto& measure : score.getTrack (t).measures)
            for (const auto& voice : measure.voices)
                for (const auto& note : voice.notes)
                    notes.push_back (&note);

    return notes;
}

static ScoreNote makeNote (double startBeat, double durationBeats, int midi, int string, int fret)
{
    ScoreNote note;
    note.startBeat = startBeat;
    note.durationBeats = durationBeats;
    note.midiNote = midi;
    note.stringIndex = string;
    note.fret = fret;
    note.pitchHz = 440.0 * std::pow (2.0, (midi - 69) / 12.0);
    note.velocity = 0.8;
    return note;
}

static ScoreMeasure makeMeasure (std::vector<ScoreNote> notes)
{
    ScoreMeasure measure;
    measure.voices.resize (1);
    measure.voices[0].notes = std::move (notes);
    return measure;
}

static bool importTab (const juce::String& tab, PerformanceScore& score)
{
    TabImportPipeline importer;
    TabImportDiagnostics diagnostics;
    return importer.read (tab, score, &diagnostics);
}

//==============================================================================
static bool testExportRoundTrip (const PerformanceScore& score)
{
    bool ok = true;

    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("tabplayer_scoretools_test.mid");
    juce::String error;

    ok &= expect (exportMidiFile (score, file, error, 480), "export succeeds");
    ok &= expect (error.isEmpty(), "export reports no error");
    ok &= expect (file.existsAsFile(), "export writes a file");

    juce::MidiFile in;
    juce::FileInputStream stream (file);
    ok &= expect (stream.openedOk() && in.readFrom (stream), "exported file reads back");
    ok &= expect (in.getTimeFormat() == 480, "ticks per quarter round-trips");
    ok &= expect (in.getNumTracks() == 1 + score.getNumTracks(), "format 1: tempo track plus one per score track");

    int noteOns = 0;
    int noteOffs = 0;
    bool hasTempo = false;
    bool hasTimeSignature = false;

    if (in.getNumTracks() > 0)
    {
        const auto* tempoTrack = in.getTrack (0);
        for (int i = 0; i < tempoTrack->getNumEvents(); ++i)
        {
            const auto& m = tempoTrack->getEventPointer (i)->message;
            if (m.isTempoMetaEvent())
            {
                hasTempo = true;
                ok &= expect (std::abs (m.getTempoSecondsPerQuarterNote() - 0.5) < 1.0e-6,
                              "tempo meta matches 120 BPM");
            }
            if (m.isTimeSignatureMetaEvent())
                hasTimeSignature = true;
        }
    }

    ok &= expect (hasTempo, "tempo meta event present");
    ok &= expect (hasTimeSignature, "time signature meta event present");

    for (int t = 1; t < in.getNumTracks(); ++t)
    {
        const auto* seq = in.getTrack (t);
        for (int i = 0; i < seq->getNumEvents(); ++i)
        {
            const auto& m = seq->getEventPointer (i)->message;
            if (m.isNoteOn())
                ++noteOns;
            else if (m.isNoteOff())
                ++noteOffs;
        }
    }

    ok &= expect (noteOns == score.getTotalNoteCount(), "note-on count equals score note count");
    ok &= expect (noteOffs == score.getTotalNoteCount(), "note-off count equals score note count");

    // At any one tick, every note-off comes before every note-on.
    for (int t = 1; t < in.getNumTracks(); ++t)
    {
        const auto* seq = in.getTrack (t);
        double lastTick = -1.0;
        bool sawOnAtTick = false;
        bool orderOk = true;

        for (int i = 0; i < seq->getNumEvents(); ++i)
        {
            const auto* holder = seq->getEventPointer (i);
            const double tick = holder->message.getTimeStamp();

            if (tick != lastTick)
            {
                lastTick = tick;
                sawOnAtTick = false;
            }

            if (holder->message.isNoteOn())
                sawOnAtTick = true;
            else if (holder->message.isNoteOff() && sawOnAtTick)
                orderOk = false;
        }

        ok &= expect (orderOk, "note-off sorted before note-on at the same tick");
    }

    // A failed write is reported, not silent: a directory cannot be written as a file.
    juce::String badError;
    ok &= expect (! exportMidiFile (score, juce::File::getSpecialLocation (juce::File::tempDirectory), badError),
                  "writing onto a directory fails");
    ok &= expect (badError.isNotEmpty(), "failed export sets an error message");

    file.deleteFile();
    return ok;
}

static bool testTiesAndTempoChange()
{
    bool ok = true;

    PerformanceScore score;
    score.getMeta().tempoBpm = 120.0;

    auto& track = score.getNumTracks() > 0 ? score.getTrack (0) : score.addTrack ("Lead");
    track.measures.clear();

    // Measure 1: A (midi 64, beat 0, 1 beat), B tied to A (beat 1, 1 beat), C (midi 67, beat 2).
    ScoreNote b = makeNote (1.0, 1.0, 64, 0, 0);
    b.tiedFromPrevious = true;
    track.measures.push_back (makeMeasure ({ makeNote (0.0, 1.0, 64, 0, 0), b, makeNote (2.0, 1.0, 67, 0, 3) }));

    // Measure 2 changes tempo to 90 BPM and the time signature to 3/4.
    ScoreMeasure second = makeMeasure ({ makeNote (0.0, 1.0, 67, 0, 3) });
    second.tempoChange = 90.0;
    second.timeSignatureNumerator = 3;
    track.measures.push_back (second);

    const auto midi = buildMidiFile (score, 480);
    ok &= expect (midi.getNumTracks() == 2, "one tempo track plus one note track");

    // Tied notes merge: A+B is one 2-beat note, so three source notes give two note-ons.
    int noteOns = 0;
    double mergedOffTick = -1.0;
    const auto* notes = midi.getTrack (1);
    for (int i = 0; i < notes->getNumEvents(); ++i)
    {
        const auto* holder = notes->getEventPointer (i);
        if (holder->message.isNoteOn())
            ++noteOns;
        else if (holder->message.isNoteOff() && holder->message.getNoteNumber() == 64)
            mergedOffTick = holder->message.getTimeStamp();
    }

    ok &= expect (noteOns == 2, "tied notes merge into one note");
    ok &= expect (std::abs (mergedOffTick - 960.0) < 1.0e-6, "merged note ends two beats after it starts");

    // Tempo track: 120 at the start, 90 at the start of measure 2 (beat 4 = tick 1920).
    const auto* tempoTrack = midi.getTrack (0);
    bool found90AtMeasure2 = false;
    bool found120AtStart = false;
    bool found34 = false;
    for (int i = 0; i < tempoTrack->getNumEvents(); ++i)
    {
        const auto& m = tempoTrack->getEventPointer (i)->message;
        if (m.isTempoMetaEvent())
        {
            const double bpm = 60.0 / m.getTempoSecondsPerQuarterNote();
            if (std::abs (bpm - 120.0) < 0.01 && m.getTimeStamp() == 0.0)
                found120AtStart = true;
            if (std::abs (bpm - 90.0) < 0.01 && std::abs (m.getTimeStamp() - 1920.0) < 1.0e-6)
                found90AtMeasure2 = true;
        }
        if (m.isTimeSignatureMetaEvent() && m.getTimeStamp() == 1920.0)
        {
            int numerator = 0;
            int denominator = 0;
            m.getTimeSignatureInfo (numerator, denominator);
            found34 = (numerator == 3 && denominator == 4);
        }
    }

    ok &= expect (found120AtStart, "120 BPM tempo meta at tick 0");
    ok &= expect (found90AtMeasure2, "90 BPM tempo meta at the start of measure 2");
    ok &= expect (found34, "3/4 time signature meta at the start of measure 2");

    return ok;
}

//==============================================================================
static bool testTranspose()
{
    bool ok = true;

    PerformanceScore score;
    ok &= expect (importTab (
        "Tempo: 120\n"
        "e|--0---2---3---2--|\n"
        "B|------------------|\n"
        "G|------------------|\n"
        "D|------------------|\n"
        "A|------------------|\n"
        "E|------------------|\n", score), "tab for transpose imports");

    const auto before = allNotes (score);
    const auto up = transpose (score, 2);
    const auto after = allNotes (up);

    ok &= expect (before.size() == after.size() && ! before.empty(), "transpose keeps note count");
    for (size_t i = 0; i < before.size() && i < after.size(); ++i)
    {
        ok &= expect (after[i]->midiNote == before[i]->midiNote + 2, "transpose moves midi note");
        ok &= expect (after[i]->fret == before[i]->fret + 2, "fret follows pitch when in range");
        ok &= expect (after[i]->stringIndex == before[i]->stringIndex, "string is kept");
        ok &= expect (std::abs (after[i]->pitchHz / before[i]->pitchHz - std::pow (2.0, 2.0 / 12.0)) < 1.0e-9,
                      "pitchHz scales by the semitone ratio");
    }

    // Out of range: fret 23 + 2 = 25 is not playable, so the fret stays and only the pitch moves.
    PerformanceScore high;
    high.getMeta().tempoBpm = 120.0;
    auto& track = high.getNumTracks() > 0 ? high.getTrack (0) : high.addTrack ("Lead");
    track.measures.clear();
    track.measures.push_back (makeMeasure ({ makeNote (0.0, 1.0, 80, 1, 23) }));

    const auto shifted = transpose (high, 2);
    const auto shiftedNotes = allNotes (shifted);
    ok &= expect (shiftedNotes.size() == 1, "out-of-range transpose keeps the note");
    if (! shiftedNotes.empty())
    {
        ok &= expect (shiftedNotes[0]->midiNote == 82, "out-of-range note still moves in pitch");
        ok &= expect (shiftedNotes[0]->fret == 23, "out-of-range fret is left unchanged");
    }

    // The input is never modified.
    ok &= expect (allNotes (high)[0]->midiNote == 80, "transpose does not modify its input");
    ok &= expect (allNotes (score)[0]->midiNote == before[0]->midiNote, "transpose does not modify the original score");

    return ok;
}

//==============================================================================
static bool testHumanize()
{
    bool ok = true;

    PerformanceScore score;
    ok &= expect (importTab (
        "Tempo: 120\n"
        "e|--0---2---3---2--|\n"
        "B|------------------|\n"
        "G|------------------|\n"
        "D|------------------|\n"
        "A|------------------|\n"
        "E|------------------|\n", score), "tab for humanize imports");

    const double timing = 0.05;
    const auto first = humanize (score, timing, 0.1, 1234u);
    const auto again = humanize (score, timing, 0.1, 1234u);
    const auto other = humanize (score, timing, 0.1, 99u);

    const auto original = allNotes (score);
    const auto a = allNotes (first);
    const auto b = allNotes (again);
    const auto c = allNotes (other);

    bool deterministic = a.size() == b.size();
    bool differentSeedDiffers = false;
    bool timingInBounds = true;
    bool startsNonNegative = true;
    bool velocityInBounds = true;

    for (size_t i = 0; i < a.size() && i < b.size() && i < original.size(); ++i)
    {
        deterministic &= a[i]->startBeat == b[i]->startBeat && a[i]->velocity == b[i]->velocity;

        if (i < c.size() && (c[i]->startBeat != a[i]->startBeat || c[i]->velocity != a[i]->velocity))
            differentSeedDiffers = true;

        timingInBounds &= std::abs (a[i]->startBeat - original[i]->startBeat) <= timing + 1.0e-12;
        startsNonNegative &= a[i]->startBeat >= 0.0;
        velocityInBounds &= a[i]->velocity >= 0.05 && a[i]->velocity <= 1.0;
    }

    ok &= expect (deterministic, "same seed gives identical output");
    ok &= expect (differentSeedDiffers, "different seeds give different output");
    ok &= expect (timingInBounds, "humanized start stays within +/- timingBeats");
    ok &= expect (startsNonNegative, "humanized start is never negative");
    ok &= expect (velocityInBounds, "humanized velocity stays within 0.05..1");

    // Zero amounts change nothing; out-of-range velocities are clamped in.
    const auto still = humanize (score, 0.0, 0.0, 7u);
    bool unchanged = true;
    const auto stillNotes = allNotes (still);
    for (size_t i = 0; i < stillNotes.size(); ++i)
        unchanged &= stillNotes[i]->startBeat == original[i]->startBeat;
    ok &= expect (unchanged, "zero timing jitter leaves starts unchanged");

    return ok;
}

//==============================================================================
static bool testQuantize()
{
    bool ok = true;

    PerformanceScore score;
    score.getMeta().tempoBpm = 120.0;
    auto& track = score.getNumTracks() > 0 ? score.getTrack (0) : score.addTrack ("Lead");
    track.measures.clear();

    track.measures.push_back (makeMeasure ({ makeNote (0.1, 0.5, 60, 0, 0),
                                             makeNote (0.45, 0.5, 62, 0, 2),
                                             makeNote (1.24, 0.5, 64, 0, 4) }));
    track.measures.push_back (makeMeasure ({ makeNote (0.24, 0.5, 65, 0, 5) }));

    const auto grid = quantize (score, 0.5);
    const auto notes = allNotes (grid);

    ok &= expect (notes.size() == 4, "quantize keeps every note");
    if (notes.size() == 4)
    {
        ok &= expect (std::abs (notes[0]->startBeat - 0.0) < 1.0e-9, "0.1 snaps to 0");
        ok &= expect (std::abs (notes[1]->startBeat - 0.5) < 1.0e-9, "0.45 snaps to 0.5");
        ok &= expect (std::abs (notes[2]->startBeat - 1.0) < 1.0e-9, "1.24 snaps to 1.0");
        ok &= expect (std::abs (notes[3]->startBeat - 0.0) < 1.0e-9,
                      "0.24 in measure 2 snaps on the absolute grid (4.24 -> 4.0, measure-relative 0)");
        ok &= expect (std::abs (notes[0]->durationBeats - 0.5) < 1.0e-9, "quantize keeps durations");
    }

    const auto unchanged = quantize (score, 0.0);
    ok &= expect (allNotes (unchanged)[0]->startBeat == 0.1, "non-positive grid returns an unchanged copy");

    return ok;
}

//==============================================================================
static bool testChords()
{
    bool ok = true;

    ok &= expect (ChordDetector::detectChord ({ 57, 60, 64 }) == "Am", "A C E is Am");
    ok &= expect (ChordDetector::detectChord ({ 55, 59, 62, 65 }) == "G7", "G B D F is G7");
    ok &= expect (ChordDetector::detectChord ({ 48, 52, 55, 59 }) == "Cmaj7", "C E G B is Cmaj7");
    ok &= expect (ChordDetector::detectChord ({ 50, 55, 57 }) == "Dsus4", "D G A is Dsus4");
    ok &= expect (ChordDetector::detectChord ({ 40, 47 }) == "E5", "E and B is the power chord E5");
    ok &= expect (ChordDetector::detectChord ({ 52, 55, 60 }) == "C", "first inversion C/E is C");
    ok &= expect (ChordDetector::detectChord ({ 57, 60, 64, 67 }) == "Am7", "A C E G is Am7");
    ok &= expect (ChordDetector::detectChord ({ 52, 55, 59 }) == "Em", "E G B is Em");
    ok &= expect (ChordDetector::detectChord ({ 59, 62, 65 }) == "Bdim", "B D F is Bdim");
    ok &= expect (ChordDetector::detectChord ({ 60, 63, 66, 69 }) == "Cdim7", "C Eb F# A is Cdim7");
    ok &= expect (ChordDetector::detectChord ({ 60, 72 }) == "", "octaves of one pitch class name nothing");
    ok &= expect (ChordDetector::detectChord ({ 64 }) == "", "a single note names nothing");
    ok &= expect (ChordDetector::detectChord ({}) == "", "no notes names nothing");
    ok &= expect (ChordDetector::detectChord ({ 60, 61 }) == "", "a semitone pair names nothing");

    return ok;
}

//==============================================================================
int main()
{
    bool ok = true;

    PerformanceScore tabScore;
    ok &= expect (importTab (
        "Tempo: 120\n"
        "e|--0---2---3---2--|\n"
        "B|------------------|\n"
        "G|------------------|\n"
        "D|------------------|\n"
        "A|------------------|\n"
        "E|------------------|\n", tabScore), "ASCII tab imports");
    ok &= expect (tabScore.getTotalNoteCount() == 4, "four notes parsed");

    ok &= testExportRoundTrip (tabScore);
    ok &= testTiesAndTempoChange();
    ok &= testTranspose();
    ok &= testHumanize();
    ok &= testQuantize();
    ok &= testChords();

    return ok ? 0 : 1;
}
