#include "TabPlaybackEngine.h"

#include <algorithm>
#include <cmath>

namespace tabplayer
{
namespace
{
constexpr int kMetronomeChannel = 10;
constexpr int kMetronomeNoteBeat = 76;
constexpr int kMetronomeNoteAccent = 77;
constexpr double kClickSeconds = 0.05;
constexpr int kSoundingBitCount = 16 * 128;
} // namespace

//==============================================================================
void TabPlaybackEngine::setScore (const PerformanceScore& score)
{
    scoreCopy = std::make_shared<const PerformanceScore> (score);
    rebuildTimeline();

    beatPosition.store (0.0, std::memory_order_release);
    playing.store (false, std::memory_order_release);
    countInRequest.store (0, std::memory_order_release);
    countInCancel.store (true, std::memory_order_release);
    pendingReset.store (true, std::memory_order_release);
}

void TabPlaybackEngine::setProfile (const InstrumentProfile& newProfile)
{
    profile = newProfile;

    if (scoreCopy != nullptr)
    {
        rebuildTimeline();
        pendingReset.store (true, std::memory_order_release);
    }
}

void TabPlaybackEngine::clear()
{
    scoreCopy.reset();
    std::atomic_store_explicit (&timeline, std::shared_ptr<const Timeline> {}, std::memory_order_release);
    beatPosition.store (0.0, std::memory_order_release);
    playing.store (false, std::memory_order_release);
    countInRequest.store (0, std::memory_order_release);
    countInCancel.store (true, std::memory_order_release);
    pendingReset.store (true, std::memory_order_release);
}

void TabPlaybackEngine::rebuildTimeline()
{
    std::shared_ptr<const Timeline> compiled;

    if (scoreCopy != nullptr)
        compiled = std::make_shared<const Timeline> (TechniqueCompiler::compile (*scoreCopy, profile));

    std::atomic_store_explicit (&timeline, compiled, std::memory_order_release);
}

//==============================================================================
void TabPlaybackEngine::play() noexcept
{
    if (getTimeline() == nullptr)
        return;

    if (! playing.load (std::memory_order_acquire))
        countInRequest.store (countInBars.load (std::memory_order_relaxed), std::memory_order_release);

    playing.store (true, std::memory_order_release);
}

void TabPlaybackEngine::stop() noexcept
{
    playing.store (false, std::memory_order_release);
    countInRequest.store (0, std::memory_order_release);
    countInCancel.store (true, std::memory_order_release);
    pendingReset.store (true, std::memory_order_release);
}

void TabPlaybackEngine::rewind() noexcept
{
    seekBeat (0.0);
}

void TabPlaybackEngine::seekBeat (double beat) noexcept
{
    beatPosition.store (juce::jlimit (0.0, getTotalBeats(), beat), std::memory_order_release);
    countInCancel.store (true, std::memory_order_release);
    pendingReset.store (true, std::memory_order_release);
}

void TabPlaybackEngine::setLoop (double startBeat, double endBeat, bool enabled) noexcept
{
    const double start = juce::jmax (0.0, startBeat);
    loopStart.store (start, std::memory_order_release);
    loopEnd.store (endBeat, std::memory_order_release);
    loopEnabled.store (enabled && endBeat > start, std::memory_order_release);
}

void TabPlaybackEngine::setCountInBars (int bars) noexcept
{
    countInBars.store (juce::jlimit (0, 16, bars), std::memory_order_release);
}

void TabPlaybackEngine::setMetronomeEnabled (bool enabled) noexcept
{
    metronomeEnabled.store (enabled, std::memory_order_release);
}

std::shared_ptr<const TabPlaybackEngine::Timeline> TabPlaybackEngine::getTimeline() const noexcept
{
    return std::atomic_load_explicit (&timeline, std::memory_order_acquire);
}

double TabPlaybackEngine::getTotalBeats() const noexcept
{
    const auto t = getTimeline();
    return t != nullptr ? t->totalBeats : 0.0;
}

double TabPlaybackEngine::beatToSeconds (double beat) const noexcept
{
    const auto t = getTimeline();
    return t != nullptr ? t->tempo.beatToSeconds (beat) : beat * 0.5;
}

TabPlaybackEngine::MessageSummary TabPlaybackEngine::getLastMessage() const noexcept
{
    MessageSummary s;
    s.note = lastNote.load (std::memory_order_acquire);
    s.string = lastString.load (std::memory_order_acquire);
    s.fret = lastFret.load (std::memory_order_acquire);
    s.velocity = lastVelocity.load (std::memory_order_acquire);
    s.channel = lastChannel.load (std::memory_order_acquire);
    s.bendSemitones = lastBend.load (std::memory_order_acquire);
    return s;
}

std::vector<TabPlaybackEngine::ActiveNote> TabPlaybackEngine::getActiveNotes() const
{
    std::vector<ActiveNote> out;
    const auto t = getTimeline();

    if (t == nullptr)
        return out;

    const double beat = getBeat();
    for (const auto& span : t->notes)
        if (span.startBeat <= beat && beat < span.endBeat)
            out.push_back ({ span.midiNote, span.channel, span.stringIndex, span.fret, span.velocity });

    return out;
}

//==============================================================================
void TabPlaybackEngine::publish (int note, int stringIndex, int fret, int velocity, int channel) noexcept
{
    lastNote.store (note, std::memory_order_release);
    lastString.store (stringIndex, std::memory_order_release);
    lastFret.store (fret, std::memory_order_release);
    lastVelocity.store (velocity, std::memory_order_release);
    lastChannel.store (channel, std::memory_order_release);
}

void TabPlaybackEngine::emitNoteOn (juce::MidiBuffer& midi, int sample, int channel, int note,
                                    int velocity, int stringIndex, int fret) noexcept
{
    const int index = (juce::jlimit (1, 16, channel) - 1) * 128 + juce::jlimit (0, 127, note);
    soundingBits[(size_t) (index >> 6)] |= (std::uint64_t) 1 << (index & 63);

    publish (note, stringIndex, fret, velocity, channel);
    midi.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) velocity), sample);
}

void TabPlaybackEngine::emitNoteOff (juce::MidiBuffer& midi, int sample, int channel, int note,
                                     int stringIndex, int fret) noexcept
{
    const int index = (juce::jlimit (1, 16, channel) - 1) * 128 + juce::jlimit (0, 127, note);
    soundingBits[(size_t) (index >> 6)] &= ~((std::uint64_t) 1 << (index & 63));

    publish (note, stringIndex, fret, 0, channel);
    midi.addEvent (juce::MidiMessage::noteOff (channel, note), sample);
}

void TabPlaybackEngine::emitEvent (juce::MidiBuffer& midi, int sample, const PlaybackEvent& e) noexcept
{
    switch (e.type)
    {
        case PlaybackEvent::Type::noteOn:
        case PlaybackEvent::Type::keyswitch:
            emitNoteOn (midi, sample, e.channel, e.data1, e.data2, e.stringIndex, e.fret);
            break;

        case PlaybackEvent::Type::noteOff:
            emitNoteOff (midi, sample, e.channel, e.data1, e.stringIndex, e.fret);
            break;

        case PlaybackEvent::Type::pitchBend:
            midi.addEvent (juce::MidiMessage::pitchWheel (e.channel, e.pitchBend14), sample);
            lastBend.store (e.bendSemitones, std::memory_order_release);
            lastChannel.store (e.channel, std::memory_order_release);
            break;

        case PlaybackEvent::Type::cc:
            midi.addEvent (juce::MidiMessage::controllerEvent (e.channel, e.data1, e.data2), sample);
            lastChannel.store (e.channel, std::memory_order_release);
            break;

        case PlaybackEvent::Type::programChange:
            midi.addEvent (juce::MidiMessage::programChange (e.channel, e.data1), sample);
            lastChannel.store (e.channel, std::memory_order_release);
            break;
    }
}

void TabPlaybackEngine::emitReset (juce::MidiBuffer& midi, int sample, const Timeline* t, bool withProgram) noexcept
{
    // Note-offs for everything this engine has sounding, in channel/note order.
    for (int index = 0; index < kSoundingBitCount; ++index)
    {
        const auto word = soundingBits[(size_t) (index >> 6)];
        if (((word >> (index & 63)) & 1u) != 0)
            midi.addEvent (juce::MidiMessage::noteOff (index / 128 + 1, index % 128), sample);
    }

    soundingBits.fill (0);
    lastBend.store (0.0, std::memory_order_release);

    if (t == nullptr)
        return;

    for (int channel = 1; channel <= 16; ++channel)
    {
        if ((t->channelMask & (1u << (unsigned) channel)) == 0)
            continue;

        midi.addEvent (juce::MidiMessage::controllerEvent (channel, 123, 0), sample); // all notes off
        midi.addEvent (juce::MidiMessage::controllerEvent (channel, 64, 0), sample);  // sustain off
        midi.addEvent (juce::MidiMessage::pitchWheel (channel, 8192), sample);        // bend centre

        for (const auto& [cc, offValue] : t->resetCcs)
            midi.addEvent (juce::MidiMessage::controllerEvent (channel, cc, offValue), sample);

        if (withProgram && t->programChange >= 0)
            midi.addEvent (juce::MidiMessage::programChange (channel, t->programChange), sample);
    }
}

void TabPlaybackEngine::emitClick (juce::MidiBuffer& midi, int sample, int offSample, bool accent) noexcept
{
    const int note = accent ? kMetronomeNoteAccent : kMetronomeNoteBeat;
    const int velocity = accent ? 127 : 100;

    emitNoteOn (midi, sample, kMetronomeChannel, note, velocity, -1, 0);
    emitNoteOff (midi, offSample, kMetronomeChannel, note, -1, 0);
}

//==============================================================================
void TabPlaybackEngine::process (juce::MidiBuffer& midi, int numSamples, double sampleRate,
                                 double bpm, double speed, double hostPpq)
{
    if (numSamples <= 0 || sampleRate <= 0.0)
        return;

    const auto t = getTimeline();
    const Timeline* tl = t.get();

    // Host sync: the host position is authoritative. A jump re-seeks and resets.
    if (hostPpq >= 0.0 && tl != nullptr && std::abs (hostPpq - beatPosition.load (std::memory_order_acquire)) > 1.0 / 16.0)
    {
        beatPosition.store (juce::jlimit (0.0, tl->totalBeats, hostPpq), std::memory_order_release);
        countInCancel.store (true, std::memory_order_release);
        pendingReset.store (true, std::memory_order_release);
    }

    if (countInCancel.exchange (false, std::memory_order_acq_rel))
        countInActive = false;

    // Pending reset (stop, seek, setScore, setProfile, host jump) goes out first, even when stopped.
    if (pendingReset.exchange (false, std::memory_order_acq_rel))
        emitReset (midi, 0, tl, playing.load (std::memory_order_acquire));

    if (! playing.load (std::memory_order_acquire) || tl == nullptr)
        return;
    if (tl->events.empty() && tl->totalBeats <= 0.0)
        return;

    bpm = juce::jlimit (20.0, 400.0, bpm);
    speed = juce::jlimit (0.1, 4.0, speed);

    // Score seconds per output sample. The tempo map is relative to its initial tempo.
    const double secPerSample = (speed * bpm / juce::jmax (1.0, tl->tempo.initialBpm())) / sampleRate;
    const double countRate = speed * bpm / 60.0 / sampleRate; // quarter-note beats per sample
    const int clickOffset = juce::jmax (1, (int) std::lround (kClickSeconds * sampleRate));
    const int barBeats = juce::jmax (1, (int) std::lround (tl->beatsPerBar));

    int s = 0;

    // Count-in: clicks only, playhead does not move.
    if (const int bars = countInRequest.exchange (0, std::memory_order_acq_rel); bars > 0)
    {
        countInActive = true;
        countInElapsed = 0.0;
        countInTotal = (double) bars * tl->beatsPerBar;
    }

    while (countInActive && s < numSamples)
    {
        const double remain = countInTotal - countInElapsed;
        const int needed = juce::jmax (1, (int) std::ceil (remain / countRate - 1.0e-9));
        const int chunk = juce::jmin (numSamples - s, needed);
        const double chunkEnd = chunk == needed ? countInTotal : countInElapsed + (double) chunk * countRate;

        for (int i = (int) std::ceil (countInElapsed); (double) i < chunkEnd && (double) i < countInTotal; ++i)
        {
            const double offset = ((double) i - countInElapsed) / countRate;
            const int sample = juce::jlimit (s, s + chunk - 1, s + (int) std::floor (juce::jmax (0.0, offset) + 1.0e-9));
            emitClick (midi, sample, juce::jmin (numSamples - 1, sample + clickOffset), i % barBeats == 0);
        }

        countInElapsed = chunkEnd;
        s += chunk;

        if (countInElapsed >= countInTotal - 1.0e-9)
            countInActive = false;
    }

    // Playback.
    double beat = beatPosition.load (std::memory_order_acquire);
    const double total = tl->totalBeats;
    const double loopStartBeat = loopStart.load (std::memory_order_acquire);
    const double loopEndBeat = juce::jmin (loopEnd.load (std::memory_order_acquire), total);
    const bool useLoop = loopEnabled.load (std::memory_order_acquire)
                         && loopEndBeat > loopStartBeat && loopStartBeat < total;
    const bool metronome = metronomeEnabled.load (std::memory_order_acquire);

    while (s < numSamples)
    {
        if (useLoop && beat >= loopEndBeat)
        {
            // Loop wrap: all notes off and reset before the region starts again.
            emitReset (midi, s, tl, false);
            beat = loopStartBeat;
            continue;
        }

        if (! useLoop && beat >= total)
        {
            emitReset (midi, s, tl, false);
            playing.store (false, std::memory_order_release);
            beat = total;
            break;
        }

        const double limit = useLoop ? loopEndBeat : total;
        const double sec0 = tl->tempo.beatToSeconds (beat);
        const double samplesToLimit = (tl->tempo.beatToSeconds (limit) - sec0) / secPerSample;
        const int needed = juce::jmax (1, (int) std::ceil (samplesToLimit - 1.0e-9));
        const int chunk = juce::jmin (numSamples - s, needed);
        const double endBeat = chunk == needed ? limit : tl->tempo.secondsToBeat (sec0 + (double) chunk * secPerSample);
        const bool inclusiveEnd = ! useLoop && endBeat >= total; // notes ending exactly at the song end

        auto sampleFor = [&] (double eventBeat)
        {
            const double offset = (tl->tempo.beatToSeconds (eventBeat) - sec0) / secPerSample;
            return juce::jlimit (s, s + chunk - 1, s + (int) std::floor (juce::jmax (0.0, offset) + 1.0e-9));
        };

        const auto& events = tl->events;
        auto it = std::lower_bound (events.begin(), events.end(), beat,
                                    [] (const PlaybackEvent& e, double b) { return e.beat < b; });

        for (; it != events.end() && (it->beat < endBeat || (inclusiveEnd && it->beat <= endBeat)); ++it)
            emitEvent (midi, sampleFor (it->beat), *it);

        if (metronome)
        {
            // Half-open [beat, endBeat) with no epsilon, so a click on a chunk boundary is emitted exactly once.
            for (int k = (int) std::ceil (beat); (double) k < endBeat; ++k)
            {
                const int sample = sampleFor ((double) k);
                emitClick (midi, sample, juce::jmin (numSamples - 1, sample + clickOffset), k % barBeats == 0);
            }
        }

        s += chunk;
        beat = endBeat;
    }

    beatPosition.store (beat, std::memory_order_release);
}
} // namespace tabplayer
