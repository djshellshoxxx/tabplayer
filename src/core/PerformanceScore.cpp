#include "PerformanceScore.h"

#include <algorithm>

namespace tabplayer
{

//==============================================================================
const char* getTechniqueName (ScoreTechnique::Type type) noexcept
{
    switch (type)
    {
        case ScoreTechnique::Type::bend:               return "Bend";
        case ScoreTechnique::Type::bendRelease:        return "Bend Release";
        case ScoreTechnique::Type::preBend:            return "Pre-Bend";
        case ScoreTechnique::Type::slideUp:            return "Slide Up";
        case ScoreTechnique::Type::slideDown:          return "Slide Down";
        case ScoreTechnique::Type::slideLegato:        return "Legato Slide";
        case ScoreTechnique::Type::slideShift:         return "Shift Slide";
        case ScoreTechnique::Type::slideIn:            return "Slide In";
        case ScoreTechnique::Type::slideOut:           return "Slide Out";
        case ScoreTechnique::Type::hammerOn:           return "Hammer-On";
        case ScoreTechnique::Type::pullOff:            return "Pull-Off";
        case ScoreTechnique::Type::palmMute:           return "Palm Mute";
        case ScoreTechnique::Type::deadNote:           return "Dead Note";
        case ScoreTechnique::Type::naturalHarmonic:    return "Natural Harmonic";
        case ScoreTechnique::Type::pinchHarmonic:      return "Pinch Harmonic";
        case ScoreTechnique::Type::artificialHarmonic: return "Artificial Harmonic";
        case ScoreTechnique::Type::tapHarmonic:        return "Tap Harmonic";
        case ScoreTechnique::Type::tap:                return "Tap";
        case ScoreTechnique::Type::vibrato:            return "Vibrato";
        case ScoreTechnique::Type::trill:              return "Trill";
        case ScoreTechnique::Type::whammy:             return "Whammy";
        case ScoreTechnique::Type::ghostNote:          return "Ghost Note";
        case ScoreTechnique::Type::accent:             return "Accent";
        case ScoreTechnique::Type::staccato:           return "Staccato";
        case ScoreTechnique::Type::letRing:            return "Let Ring";
        case ScoreTechnique::Type::pickStrokeUp:       return "Up Stroke";
        case ScoreTechnique::Type::pickStrokeDown:     return "Down Stroke";
        case ScoreTechnique::Type::slap:               return "Slap";
        case ScoreTechnique::Type::pop:                return "Pop";
        case ScoreTechnique::Type::numTypes:
        default:                                       return "Unknown";
    }
}

//==============================================================================
bool ScoreNote::hasTechnique (ScoreTechnique::Type type) const noexcept
{
    return findTechnique (type) != nullptr;
}

const ScoreTechnique* ScoreNote::findTechnique (ScoreTechnique::Type type) const noexcept
{
    for (const auto& technique : techniques)
        if (technique.type == type)
            return &technique;

    return nullptr;
}

//==============================================================================
std::vector<const ScoreNote*> ScoreMeasure::collectNotes() const
{
    std::vector<const ScoreNote*> all;

    for (const auto& voice : voices)
        for (const auto& note : voice.notes)
            all.push_back (&note);

    std::sort (all.begin(), all.end(),
               [] (const ScoreNote* a, const ScoreNote* b)
    {
        if (a->startBeat != b->startBeat)
            return a->startBeat < b->startBeat;

        // Within a chord, the lowest string first, so a strum reads downward the
        // way a guitarist plays one.
        return a->stringIndex > b->stringIndex;
    });

    return all;
}

//==============================================================================
PerformanceScore::PerformanceScore()
{
    soundingIndex.fill (-1);
    addTrack();
}

void PerformanceScore::clear()
{
    tracks.clear();
    captured.clear();
    capturedChords.clear();
    soundingIndex.fill (-1);
    capturing = false;

    addTrack();
}

ScoreTrack& PerformanceScore::addTrack (const juce::String& name)
{
    ScoreTrack track;
    track.name = name;
    tracks.push_back (track);

    return tracks.back();
}

ScoreTrack& PerformanceScore::getTrack (int index) noexcept
{
    if (tracks.empty())
        addTrack();

    return tracks[(size_t) juce::jlimit (0, (int) tracks.size() - 1, index)];
}

const ScoreTrack& PerformanceScore::getTrack (int index) const noexcept
{
    static const ScoreTrack empty;

    return tracks.empty() ? empty
                          : tracks[(size_t) juce::jlimit (0, (int) tracks.size() - 1, index)];
}

int PerformanceScore::getTotalNoteCount() const noexcept
{
    int count = 0;

    for (const auto& track : tracks)
        for (const auto& measure : track.measures)
            for (const auto& voice : measure.voices)
                count += (int) voice.notes.size();

    return count;
}

double PerformanceScore::getTotalBeats() const noexcept
{
    double total = 0.0;

    for (const auto& track : tracks)
    {
        double trackBeats = 0.0;

        for (const auto& measure : track.measures)
            trackBeats += (double) measure.timeSignatureNumerator * 4.0
                            / (double) juce::jmax (1, measure.timeSignatureDenominator);

        total = juce::jmax (total, trackBeats);
    }

    return total;
}

//==============================================================================
void PerformanceScore::beginCapture (double tempoBpm, int numerator, int denominator)
{
    captured.clear();
    capturedChords.clear();
    soundingIndex.fill (-1);

    captureTempo = juce::jlimit (20.0, 300.0, tempoBpm);

    meta.tempoBpm = captureTempo;
    meta.timeSignatureNumerator = juce::jlimit (1, 32, numerator);
    meta.timeSignatureDenominator = (denominator == 2 || denominator == 4
                                       || denominator == 8 || denominator == 16)
                                      ? denominator : 4;

    capturing = true;
}

void PerformanceScore::noteStarted (int stringIndex, int fret, int midiNote, double pitchHz,
                                    double velocity, double beat)
{
    if (! capturing || ! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return;

    // A string that was already sounding is stopped first: one string cannot
    // play two notes at once, and leaving the old one open would give it an
    // unbounded duration.
    noteEnded (stringIndex, beat);

    CapturedNote entry;

    entry.note.startBeat = juce::jmax (0.0, beat);
    entry.note.durationBeats = 0.0;
    entry.note.stringIndex = stringIndex;
    entry.note.fret = juce::jmax (0, fret);
    entry.note.midiNote = juce::jlimit (0, 127, midiNote);
    entry.note.pitchHz = pitchHz;
    entry.note.velocity = juce::jlimit (0.0, 1.0, velocity);
    entry.open = true;

    captured.push_back (entry);
    soundingIndex[(size_t) stringIndex] = (int) captured.size() - 1;
}

void PerformanceScore::noteEnded (int stringIndex, double beat)
{
    if (! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return;

    const int index = soundingIndex[(size_t) stringIndex];

    if (! juce::isPositiveAndBelow (index, (int) captured.size()))
        return;

    auto& entry = captured[(size_t) index];

    if (entry.open)
    {
        // A minimum of a sixty-fourth note, so a stab still has a duration a
        // notation program can express.
        entry.note.durationBeats = juce::jmax (0.0625, beat - entry.note.startBeat);
        entry.open = false;
    }

    soundingIndex[(size_t) stringIndex] = -1;
}

void PerformanceScore::addTechnique (int stringIndex, const ScoreTechnique& technique)
{
    if (! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return;

    const int index = soundingIndex[(size_t) stringIndex];

    if (juce::isPositiveAndBelow (index, (int) captured.size()))
        captured[(size_t) index].note.techniques.push_back (technique);
}

void PerformanceScore::setAutoRules (int stringIndex, juce::uint16 rules)
{
    if (! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return;

    const int index = soundingIndex[(size_t) stringIndex];

    if (juce::isPositiveAndBelow (index, (int) captured.size()))
        captured[(size_t) index].note.autoRules = rules;
}

void PerformanceScore::addChordSymbol (double beat, const juce::String& symbol)
{
    if (! capturing || symbol.isEmpty())
        return;

    // Only changes are recorded, which is what notation-export 4 asks for: a
    // symbol at every beat would be unreadable.
    if (! capturedChords.empty() && capturedChords.back().second == symbol)
        return;

    capturedChords.emplace_back (juce::jmax (0.0, beat), symbol);
}

//==============================================================================
void PerformanceScore::endCapture (double finalBeat)
{
    if (! capturing)
        return;

    capturing = false;

    // Anything still sounding ends here.
    for (int s = 0; s < kMaxStrings; ++s)
        noteEnded (s, finalBeat);

    auto& track = getTrack (0);
    track.measures.clear();

    const double beatsPerMeasure = (double) meta.timeSignatureNumerator * 4.0
                                     / (double) juce::jmax (1, meta.timeSignatureDenominator);

    if (beatsPerMeasure <= 0.0)
        return;

    const double lastBeat = juce::jmax (finalBeat, [this]
    {
        double latest = 0.0;

        for (const auto& entry : captured)
            latest = juce::jmax (latest, entry.note.startBeat + entry.note.durationBeats);

        return latest;
    }());

    const int numMeasures = juce::jmax (1, (int) std::ceil (lastBeat / beatsPerMeasure));

    track.measures.resize ((size_t) numMeasures);

    for (auto& measure : track.measures)
    {
        measure.timeSignatureNumerator = meta.timeSignatureNumerator;
        measure.timeSignatureDenominator = meta.timeSignatureDenominator;
        measure.voices.resize (1);
    }

    track.measures[0].tempoChange = meta.tempoBpm;

    /*  Notes go into the measure they start in, with their beat made relative to
        it. A note that runs over a bar line keeps its full duration rather than
        being split into tied notes: every format this exports to can express a
        note longer than the bar it starts in, and splitting would lose which
        string the tail was on. */
    for (const auto& entry : captured)
    {
        const int measureIndex = juce::jlimit (
            0, numMeasures - 1, (int) (entry.note.startBeat / beatsPerMeasure));

        auto note = entry.note;
        note.startBeat -= (double) measureIndex * beatsPerMeasure;

        if (note.durationBeats <= 0.0)
            note.durationBeats = 0.25;

        auto& measure = track.measures[(size_t) measureIndex];

        /*  Voices (notation-export 2.1).

            A note that overlaps another already in a voice goes into the next
            one, so that polyphony survives into the export instead of being
            flattened into a chord that was never played. */
        size_t voiceIndex = 0;

        for (;;)
        {
            if (voiceIndex >= measure.voices.size())
            {
                measure.voices.emplace_back();
                break;
            }

            bool clashes = false;

            for (const auto& existing : measure.voices[voiceIndex].notes)
            {
                const bool sameStart = std::abs (existing.startBeat - note.startBeat) < 1.0e-6;

                // Notes that start together are a chord, and belong in one voice.
                if (sameStart)
                    continue;

                const bool overlaps = note.startBeat < existing.startBeat + existing.durationBeats
                                        && existing.startBeat < note.startBeat + note.durationBeats;

                if (overlaps)
                {
                    clashes = true;
                    break;
                }
            }

            if (! clashes)
                break;

            ++voiceIndex;
        }

        measure.voices[voiceIndex].notes.push_back (note);
    }

    // Sort each voice, and drop the empty ones a clash search may have left.
    for (auto& measure : track.measures)
    {
        for (auto& voice : measure.voices)
            std::sort (voice.notes.begin(), voice.notes.end(),
                       [] (const ScoreNote& a, const ScoreNote& b)
        {
            if (a.startBeat != b.startBeat)
                return a.startBeat < b.startBeat;

            return a.stringIndex > b.stringIndex;
        });

        measure.voices.erase (
            std::remove_if (measure.voices.begin() + 1, measure.voices.end(),
                            [] (const ScoreVoice& v) { return v.notes.empty(); }),
            measure.voices.end());
    }

    // ---- chord symbols -------------------------------------------------------------
    for (const auto& [beat, symbol] : capturedChords)
    {
        const int measureIndex = juce::jlimit (
            0, numMeasures - 1, (int) (beat / beatsPerMeasure));

        track.measures[(size_t) measureIndex].chordSymbols.emplace_back (
            beat - (double) measureIndex * beatsPerMeasure, symbol);
    }
}

//==============================================================================
void PerformanceScore::beatToMeasure (double beat, int& measureIndex,
                                      double& beatInMeasure) const noexcept
{
    const double beatsPerMeasure = (double) meta.timeSignatureNumerator * 4.0
                                     / (double) juce::jmax (1, meta.timeSignatureDenominator);

    if (beatsPerMeasure <= 0.0)
    {
        measureIndex = 0;
        beatInMeasure = beat;
        return;
    }

    measureIndex = (int) (juce::jmax (0.0, beat) / beatsPerMeasure);
    beatInMeasure = beat - (double) measureIndex * beatsPerMeasure;
}

juce::String PerformanceScore::getNoteName (int midiNote)
{
    static const char* const names[12] =
        { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    const int clamped = juce::jlimit (0, 127, midiNote);

    return juce::String (names[clamped % 12]) + juce::String (clamped / 12 - 1);
}

void PerformanceScore::getMusicXmlPitch (int midiNote, juce::String& step, int& alter, int& octave)
{
    // MusicXML wants a natural letter plus an alteration, not a chromatic index.
    static const char* const steps[12] =
        { "C", "C", "D", "D", "E", "F", "F", "G", "G", "A", "A", "B" };

    static const int alters[12] =
        {  0,   1,   0,   1,   0,   0,   1,   0,   1,   0,   1,   0 };

    const int clamped = juce::jlimit (0, 127, midiNote);

    step = steps[clamped % 12];
    alter = alters[clamped % 12];
    octave = clamped / 12 - 1;
}

} // namespace tabplayer
