#include "TechniqueCompiler.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace tabplayer
{
namespace
{
constexpr double kMinNoteBeats = 0.02;
constexpr double kBendGridBeats = 1.0 / 16.0;
constexpr double kKeyswitchGapBeats = 1.0 / 32.0;
constexpr double kVibratoRateHz = 5.5;
constexpr double kVibratoDefaultDepth = 0.35;
constexpr double kDeadNoteBeats = 0.1;
constexpr double kDeadNoteVelocityScale = 0.4;
constexpr int kKeyswitchVelocity = 100;
constexpr int kMaxBendSamplesPerNote = 2048;

double quantizeBeat (double beat) noexcept
{
    return std::round (beat * 1.0e6) / 1.0e6;
}

int clampChannel (int channel) noexcept
{
    return juce::jlimit (1, 16, channel);
}

const ArticulationMapping* findMapping (const InstrumentProfile& profile, Articulation a) noexcept
{
    // Local lookup: InstrumentProfile::find is not required here (unknown mappings degrade to a plain note).
    for (const auto& mapping : profile.mappings)
        if (mapping.articulation == a)
            return &mapping;

    return nullptr;
}

bool articulationFor (ScoreTechnique::Type type, Articulation& out) noexcept
{
    switch (type)
    {
        case ScoreTechnique::Type::palmMute:          out = Articulation::palmMute; return true;
        case ScoreTechnique::Type::deadNote:          out = Articulation::deadNote; return true;
        case ScoreTechnique::Type::naturalHarmonic:
        case ScoreTechnique::Type::pinchHarmonic:
        case ScoreTechnique::Type::artificialHarmonic:
        case ScoreTechnique::Type::tapHarmonic:       out = Articulation::harmonic; return true;
        case ScoreTechnique::Type::tap:               out = Articulation::tap; return true;
        case ScoreTechnique::Type::slap:              out = Articulation::slap; return true;
        case ScoreTechnique::Type::pop:               out = Articulation::pop; return true;
        case ScoreTechnique::Type::hammerOn:
        case ScoreTechnique::Type::pullOff:
        case ScoreTechnique::Type::slideLegato:       out = Articulation::legato; return true;
        case ScoreTechnique::Type::staccato:          out = Articulation::staccato; return true;
        case ScoreTechnique::Type::accent:            out = Articulation::accent; return true;
        case ScoreTechnique::Type::ghostNote:         out = Articulation::ghost; return true;
        case ScoreTechnique::Type::letRing:           out = Articulation::letRing; return true;
        default: break;
    }

    return false;
}

using Curve = std::vector<std::pair<double, double>>;

double interpolateCurve (const Curve& curve, double pos) noexcept
{
    if (curve.empty())
        return 0.0;
    if (pos <= curve.front().first)
        return curve.front().second;
    if (pos >= curve.back().first)
        return curve.back().second;

    for (size_t i = 1; i < curve.size(); ++i)
    {
        if (pos <= curve[i].first)
        {
            const double span = curve[i].first - curve[i - 1].first;
            const double t = span > 0.0 ? (pos - curve[i - 1].first) / span : 0.0;
            return curve[i - 1].second + t * (curve[i].second - curve[i - 1].second);
        }
    }

    return curve.back().second;
}

Curve defaultBendCurve (ScoreTechnique::Type type, double v)
{
    switch (type)
    {
        case ScoreTechnique::Type::bend:        return { { 0.0, 0.0 }, { 0.25, v }, { 1.0, v } };
        case ScoreTechnique::Type::bendRelease: return { { 0.0, v }, { 0.75, v }, { 1.0, 0.0 } };
        case ScoreTechnique::Type::preBend:     return { { 0.0, v }, { 1.0, v } };
        case ScoreTechnique::Type::whammy:      return { { 0.0, 0.0 }, { 0.5, v }, { 1.0, 0.0 } };
        default: break;
    }

    return {};
}

struct NoteRec
{
    double start = 0.0;
    double end = 0.0;
    const ScoreNote* note = nullptr;
    int channel = 1;
    int velocity = 1;
    bool legatoIn = false;
};

struct BendContributor
{
    Curve curve;
    bool vibrato = false;
    double depth = 0.0;
};
} // namespace

//==============================================================================
double TempoMap::initialBpm() const noexcept
{
    return segments.empty() ? 120.0 : segments.front().bpm;
}

double TempoMap::bpmAt (double beat) const noexcept
{
    if (segments.empty())
        return 120.0;

    auto it = std::upper_bound (segments.begin(), segments.end(), beat,
                                [] (double b, const Segment& s) { return b < s.beat; });
    return (it == segments.begin() ? segments.front() : *(it - 1)).bpm;
}

double TempoMap::beatToSeconds (double beat) const noexcept
{
    const double b = juce::jmax (0.0, beat);

    if (segments.empty())
        return b * 60.0 / 120.0;

    auto it = std::upper_bound (segments.begin(), segments.end(), b,
                                [] (double x, const Segment& s) { return x < s.beat; });
    const auto& seg = it == segments.begin() ? segments.front() : *(it - 1);
    return seg.seconds + (b - seg.beat) * 60.0 / seg.bpm;
}

double TempoMap::secondsToBeat (double seconds) const noexcept
{
    const double s = juce::jmax (0.0, seconds);

    if (segments.empty())
        return s * 2.0;

    auto it = std::upper_bound (segments.begin(), segments.end(), s,
                                [] (double x, const Segment& seg) { return x < seg.seconds; });
    const auto& seg = it == segments.begin() ? segments.front() : *(it - 1);
    return seg.beat + (s - seg.seconds) * seg.bpm / 60.0;
}

//==============================================================================
TempoMap TechniqueCompiler::buildTempoMap (const PerformanceScore& score)
{
    TempoMap map;
    const auto& meta = score.getMeta();
    double bpm = juce::jlimit (20.0, 400.0, meta.tempoBpm);

    const bool hasTrack = score.getNumTracks() > 0;
    const auto* measures = hasTrack ? &score.getTrack (0).measures : nullptr;

    if (measures != nullptr && ! measures->empty() && (*measures)[0].tempoChange > 0.0)
        bpm = juce::jlimit (20.0, 400.0, (*measures)[0].tempoChange);

    map.segments.push_back ({ 0.0, bpm, 0.0 });

    if (measures == nullptr)
        return map;

    double beat = 0.0;
    for (const auto& measure : *measures)
    {
        if (measure.tempoChange > 0.0)
        {
            const double newBpm = juce::jlimit (20.0, 400.0, measure.tempoChange);
            auto& last = map.segments.back();

            if (beat <= last.beat)
                last.bpm = newBpm;
            else if (std::abs (newBpm - last.bpm) > 1.0e-9)
                map.segments.push_back ({ beat, newBpm, map.beatToSeconds (beat) });
        }

        beat += (double) measure.timeSignatureNumerator * 4.0
              / (double) juce::jmax (1, measure.timeSignatureDenominator);
    }

    // Segment starts may have been recorded before a same-beat replacement; recompute seconds in order.
    for (size_t i = 1; i < map.segments.size(); ++i)
        map.segments[i].seconds = map.segments[i - 1].seconds
                                + (map.segments[i].beat - map.segments[i - 1].beat)
                                  * 60.0 / map.segments[i - 1].bpm;

    return map;
}

int TechniqueCompiler::semitonesToPitchBend14 (double semitones, double rangeSemitones) noexcept
{
    const double range = juce::jmax (0.5, rangeSemitones);
    const double x = juce::jlimit (-range, range, semitones) / range;
    return juce::jlimit (0, 16383, 8192 + (int) std::lround (x * 8191.0));
}

int TechniqueCompiler::rank (PlaybackEvent::Type type, bool isReset) noexcept
{
    switch (type)
    {
        case PlaybackEvent::Type::noteOff:       return 0;
        case PlaybackEvent::Type::cc:            return isReset ? 1 : 5;
        case PlaybackEvent::Type::pitchBend:     return isReset ? 1 : 2;
        case PlaybackEvent::Type::programChange: return 3;
        case PlaybackEvent::Type::keyswitch:     return 3;
        case PlaybackEvent::Type::noteOn:        return 4;
    }

    return 4;
}

void TechniqueCompiler::sortEvents (std::vector<PlaybackEvent>& events)
{
    std::stable_sort (events.begin(), events.end(), [] (const PlaybackEvent& a, const PlaybackEvent& b)
    {
        if (a.beat != b.beat)
            return a.beat < b.beat;

        const int ra = rank (a.type, a.reset);
        const int rb = rank (b.type, b.reset);
        if (ra != rb)
            return ra < rb;
        if (a.channel != b.channel)
            return a.channel < b.channel;
        if (a.data1 != b.data1)
            return a.data1 < b.data1;
        return a.sequence < b.sequence;
    });
}

//==============================================================================
CompiledTimeline TechniqueCompiler::compile (const PerformanceScore& score, const InstrumentProfile& profile)
{
    CompiledTimeline out;
    out.tempo = buildTempoMap (score);
    out.programChange = profile.programChange >= 0 ? juce::jlimit (0, 127, profile.programChange) : -1;

    for (const auto& mapping : profile.mappings)
        if (mapping.ccNumber >= 0)
            out.resetCcs.emplace_back (juce::jlimit (0, 127, mapping.ccNumber),
                                       juce::jlimit (0, 127, mapping.ccOffValue));

    const double bendRange = juce::jlimit (0.5, 96.0, profile.pitchBendRangeSemitones);
    const double overlap = juce::jmax (0.0, profile.legatoOverlapBeats);

    if (score.getNumTracks() <= 0)
    {
        out.totalBeats = score.getTotalBeats();
        return out;
    }

    const auto& track = score.getTrack (0);
    out.beatsPerBar = track.measures.empty() ? 4.0
                    : (double) track.measures.front().timeSignatureNumerator * 4.0
                      / (double) juce::jmax (1, track.measures.front().timeSignatureDenominator);

    // 1. Gather note records in absolute beats.
    std::vector<NoteRec> recs;
    double trackBase = 0.0;

    for (const auto& measure : track.measures)
    {
        for (const auto& voice : measure.voices)
        {
            for (const auto& note : voice.notes)
            {
                NoteRec rec;
                rec.note = &note;
                rec.start = quantizeBeat (trackBase + juce::jmax (0.0, note.startBeat));

                const int string = juce::jmax (0, note.stringIndex);
                switch (profile.channelMode)
                {
                    case ChannelMode::single:
                        rec.channel = clampChannel (profile.singleChannel);
                        break;
                    case ChannelMode::perString:
                        rec.channel = clampChannel (string + 1);
                        break;
                    case ChannelMode::mpeLowerZone:
                        rec.channel = 2 + juce::jmin (string, 14);
                        break;
                }

                double v = std::round (note.velocity * 127.0);
                double dur = juce::jmax (kMinNoteBeats, note.durationBeats);

                if (note.hasTechnique (ScoreTechnique::Type::palmMute))
                {
                    dur *= profile.palmMuteDurationScale;
                    v *= profile.palmMuteVelocityScale;
                }
                if (note.hasTechnique (ScoreTechnique::Type::staccato))
                    dur *= profile.staccatoDurationScale;
                if (note.hasTechnique (ScoreTechnique::Type::deadNote))
                {
                    v *= kDeadNoteVelocityScale;
                    dur = juce::jmin (dur, kDeadNoteBeats);
                }
                if (note.hasTechnique (ScoreTechnique::Type::ghostNote))
                    v *= profile.ghostVelocityScale;
                if (note.hasTechnique (ScoreTechnique::Type::accent))
                    v *= profile.accentVelocityScale;

                rec.velocity = juce::jlimit (1, 127, (int) std::lround (v));
                rec.end = quantizeBeat (rec.start + juce::jmax (kMinNoteBeats, dur));
                rec.legatoIn = note.hasTechnique (ScoreTechnique::Type::hammerOn)
                            || note.hasTechnique (ScoreTechnique::Type::pullOff)
                            || note.hasTechnique (ScoreTechnique::Type::slideLegato);

                recs.push_back (rec);
            }
        }

        trackBase += (double) measure.timeSignatureNumerator * 4.0
                   / (double) juce::jmax (1, measure.timeSignatureDenominator);
    }

    out.totalBeats = juce::jmax (score.getTotalBeats(), trackBase);

    std::stable_sort (recs.begin(), recs.end(), [] (const NoteRec& a, const NoteRec& b)
    {
        if (a.start != b.start)
            return a.start < b.start;
        return a.note->stringIndex < b.note->stringIndex;
    });

    // 2. Legato: a hammer-on, pull-off or legato slide lets the previous note on the same string ring
    //    into it by the profile's overlap.
    std::array<int, kMaxStrings> lastOnString {};
    lastOnString.fill (-1);

    for (size_t i = 0; i < recs.size(); ++i)
    {
        const int string = juce::jlimit (0, kMaxStrings - 1, recs[i].note->stringIndex);

        if (recs[i].legatoIn && lastOnString[(size_t) string] >= 0)
        {
            auto& previous = recs[(size_t) lastOnString[(size_t) string]];
            previous.end = quantizeBeat (juce::jmax (previous.end, recs[i].start + overlap));
        }

        lastOnString[(size_t) string] = (int) i;
    }

    // 3. Emit events.
    int sequence = 0;
    auto add = [&] (PlaybackEvent e)
    {
        e.sequence = sequence++;
        e.beat = quantizeBeat (e.beat);
        out.channelMask |= (1u << (unsigned) juce::jlimit (1, 16, e.channel));
        out.events.push_back (e);
    };

    for (const auto& rec : recs)
    {
        const auto& note = *rec.note;
        const int string = note.stringIndex;

        // Articulation keyswitches and CCs, each mapping applied once per note.
        std::uint32_t appliedMask = 0;
        for (const auto& tech : note.techniques)
        {
            Articulation articulation;
            if (! articulationFor (tech.type, articulation))
                continue;

            const auto bit = 1u << (unsigned) articulation;
            if ((appliedMask & bit) != 0)
                continue;
            appliedMask |= bit;

            const auto* mapping = findMapping (profile, articulation);
            if (mapping == nullptr)
                continue; // unknown mapping: plain note

            if (mapping->keyswitchNote >= 0)
            {
                const int ksChannel = clampChannel (profile.keyswitchChannel);
                PlaybackEvent on;
                on.beat = rec.start;
                on.type = PlaybackEvent::Type::keyswitch;
                on.channel = ksChannel;
                on.data1 = juce::jlimit (0, 127, mapping->keyswitchNote);
                on.data2 = kKeyswitchVelocity;
                add (on);

                PlaybackEvent off = on;
                off.beat = rec.start + kKeyswitchGapBeats;
                off.type = PlaybackEvent::Type::noteOff;
                off.data2 = 0;
                add (off);
            }

            if (mapping->ccNumber >= 0)
            {
                PlaybackEvent on;
                on.beat = rec.start;
                on.type = PlaybackEvent::Type::cc;
                on.channel = rec.channel;
                on.data1 = juce::jlimit (0, 127, mapping->ccNumber);
                on.data2 = juce::jlimit (0, 127, mapping->ccValue);
                add (on);

                PlaybackEvent off = on;
                off.beat = rec.end;
                off.data2 = juce::jlimit (0, 127, mapping->ccOffValue);
                off.reset = true;
                add (off);
            }
        }

        // Note on / off.
        PlaybackEvent on;
        on.beat = rec.start;
        on.type = PlaybackEvent::Type::noteOn;
        on.channel = rec.channel;
        on.data1 = juce::jlimit (0, 127, note.midiNote);
        on.data2 = rec.velocity;
        on.stringIndex = string;
        on.fret = note.fret;
        add (on);

        PlaybackEvent off = on;
        off.beat = rec.end;
        off.type = PlaybackEvent::Type::noteOff;
        off.data2 = 0;
        add (off);

        out.notes.push_back ({ rec.start, rec.end, on.data1, rec.channel, string, note.fret, rec.velocity });

        // Pitch-bend contributors: bends, slides and vibrato.
        std::vector<BendContributor> contributors;
        for (const auto& tech : note.techniques)
        {
            switch (tech.type)
            {
                case ScoreTechnique::Type::bend:
                case ScoreTechnique::Type::bendRelease:
                case ScoreTechnique::Type::preBend:
                case ScoreTechnique::Type::whammy:
                {
                    if (! profile.sendBends)
                        break;

                    Curve curve = tech.curve.empty() ? defaultBendCurve (tech.type, tech.value) : tech.curve;
                    if (! curve.empty())
                    {
                        std::sort (curve.begin(), curve.end());
                        contributors.push_back ({ curve, false, 0.0 });
                    }
                    break;
                }
                case ScoreTechnique::Type::slideUp:
                case ScoreTechnique::Type::slideIn:
                case ScoreTechnique::Type::slideShift:
                case ScoreTechnique::Type::slideDown:
                case ScoreTechnique::Type::slideOut:
                {
                    const double amount = std::abs (tech.value);
                    if (! profile.sendSlides || amount <= 0.0)
                        break;

                    Curve curve;
                    if (tech.type == ScoreTechnique::Type::slideOut)
                        curve = { { 0.0, 0.0 }, { 1.0, amount } };
                    else if (tech.type == ScoreTechnique::Type::slideDown)
                        curve = { { 0.0, amount }, { 1.0, 0.0 } };
                    else
                        curve = { { 0.0, -amount }, { 1.0, 0.0 } };

                    contributors.push_back ({ curve, false, 0.0 });
                    break;
                }
                case ScoreTechnique::Type::vibrato:
                {
                    if (! profile.sendVibrato)
                        break;

                    const double depth = tech.value > 0.0
                                       ? juce::jmin (tech.value, bendRange * 0.5)
                                       : kVibratoDefaultDepth;
                    contributors.push_back ({ {}, true, depth });
                    break;
                }
                default:
                    break;
            }
        }

        if (contributors.empty())
            continue;

        const double start = rec.start;
        const double dur = juce::jmax (kMinNoteBeats, rec.end - rec.start);

        bool hasVibrato = false;
        for (const auto& c : contributors)
            hasVibrato = hasVibrato || c.vibrato;

        // Step size: a fixed grid, or finer than one vibrato cycle (tempo-aware) when vibrato is present.
        double step = kBendGridBeats;
        if (hasVibrato)
        {
            const double cycleBeats = (out.tempo.bpmAt (start) / 60.0) / kVibratoRateHz;
            step = juce::jmin (step, cycleBeats / 8.0);
        }
        step = juce::jmax (step, dur / (double) kMaxBendSamplesPerNote);

        const double startSeconds = out.tempo.beatToSeconds (start);
        const int numSteps = (int) std::ceil (dur / step);
        int lastBend14 = -1;

        for (int k = 0; k < numSteps; ++k)
        {
            const double t = start + (double) k * step;
            const double pos = juce::jlimit (0.0, 1.0, (t - start) / dur);

            double semitones = 0.0;
            for (const auto& c : contributors)
            {
                if (c.vibrato)
                {
                    const double secs = out.tempo.beatToSeconds (t) - startSeconds;
                    semitones += c.depth * std::sin (juce::MathConstants<double>::twoPi * kVibratoRateHz * secs);
                }
                else
                {
                    semitones += interpolateCurve (c.curve, pos);
                }
            }

            semitones = juce::jlimit (-bendRange, bendRange, semitones);
            const int bend14 = semitonesToPitchBend14 (semitones, bendRange);

            if (bend14 != lastBend14)
            {
                PlaybackEvent bend;
                bend.beat = t;
                bend.type = PlaybackEvent::Type::pitchBend;
                bend.channel = rec.channel;
                bend.pitchBend14 = bend14;
                bend.bendSemitones = semitones;
                bend.stringIndex = string;
                bend.fret = note.fret;
                add (bend);
                lastBend14 = bend14;
            }
        }

        // Release the bend at the end of the note (a reset: sorts before ordinary bends at the same beat).
        PlaybackEvent centre;
        centre.beat = rec.end;
        centre.type = PlaybackEvent::Type::pitchBend;
        centre.channel = rec.channel;
        centre.pitchBend14 = 8192;
        centre.bendSemitones = 0.0;
        centre.reset = true;
        add (centre);
    }

    // 4. Program change on every used channel at the start.
    if (out.programChange >= 0)
    {
        for (int ch = 1; ch <= 16; ++ch)
        {
            if ((out.channelMask & (1u << (unsigned) ch)) == 0)
                continue;

            PlaybackEvent pc;
            pc.beat = 0.0;
            pc.type = PlaybackEvent::Type::programChange;
            pc.channel = ch;
            pc.data1 = out.programChange;
            pc.sequence = sequence++;
            out.events.push_back (pc);
        }
    }

    TechniqueCompiler::sortEvents (out.events);
    return out;
}
} // namespace tabplayer
