#include "TabPlaybackEngine.h"

#include <algorithm>
#include <cmath>

namespace tabplayer
{
std::shared_ptr<TabPlaybackEngine::Timeline> TabPlaybackEngine::compile (const PerformanceScore& score)
{
    auto out = std::make_shared<Timeline>();
    double trackBase = 0.0;

    if (score.getNumTracks() <= 0)
        return out;

    const auto& track = score.getTrack (0);
    for (const auto& measure : track.measures)
    {
        for (const auto& voice : measure.voices)
        {
            for (const auto& note : voice.notes)
            {
                const int channel = juce::jlimit (1, 16, note.stringIndex + 1);
                const int velocity = juce::jlimit (1, 127, (int) std::lround (note.velocity * 127.0));
                const double start = trackBase + juce::jmax (0.0, note.startBeat);
                const double end = start + juce::jmax (0.01, note.durationBeats);

                out->events.push_back ({ start, note.midiNote, velocity, channel, true,
                                         note.stringIndex, note.fret });
                out->events.push_back ({ end, note.midiNote, 0, channel, false,
                                         note.stringIndex, note.fret });
            }
        }

        trackBase += (double) measure.timeSignatureNumerator * 4.0
                   / (double) juce::jmax (1, measure.timeSignatureDenominator);
    }

    std::sort (out->events.begin(), out->events.end(), [] (const Event& a, const Event& b)
    {
        if (a.beat != b.beat)
            return a.beat < b.beat;
        if (a.noteOn != b.noteOn)
            return ! a.noteOn; // note-off before note-on at the same boundary
        return a.channel < b.channel;
    });

    out->totalBeats = juce::jmax (score.getTotalBeats(), trackBase);
    return out;
}

void TabPlaybackEngine::setScore (const PerformanceScore& score)
{
    auto compiled = compile (score);
    std::atomic_store_explicit (&timeline,
                                std::static_pointer_cast<const Timeline> (compiled),
                                std::memory_order_release);
    beatPosition.store (0.0, std::memory_order_release);
    playing.store (false, std::memory_order_release);
}

void TabPlaybackEngine::clear()
{
    std::atomic_store_explicit (&timeline, std::shared_ptr<const Timeline> {},
                                std::memory_order_release);
    beatPosition.store (0.0, std::memory_order_release);
    playing.store (false, std::memory_order_release);
}

void TabPlaybackEngine::play() noexcept
{
    if (getTimeline() != nullptr)
        playing.store (true, std::memory_order_release);
}

void TabPlaybackEngine::stop() noexcept
{
    playing.store (false, std::memory_order_release);
}

void TabPlaybackEngine::rewind() noexcept
{
    beatPosition.store (0.0, std::memory_order_release);
}

void TabPlaybackEngine::seekBeat (double beat) noexcept
{
    beatPosition.store (juce::jlimit (0.0, getTotalBeats(), beat), std::memory_order_release);
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

void TabPlaybackEngine::process (juce::MidiBuffer& midi, int numSamples, double sampleRate,
                                 double bpm, double speed)
{
    if (! playing.load (std::memory_order_acquire) || numSamples <= 0 || sampleRate <= 0.0)
        return;

    const auto t = getTimeline();
    if (t == nullptr || t->events.empty())
        return;

    bpm = juce::jlimit (20.0, 400.0, bpm);
    speed = juce::jlimit (0.1, 4.0, speed);

    const double startBeat = beatPosition.load (std::memory_order_acquire);
    const double beatsPerSample = (bpm / 60.0) * speed / sampleRate;
    const double endBeat = startBeat + beatsPerSample * (double) numSamples;

    for (const auto& event : t->events)
    {
        if (event.beat < startBeat)
            continue;
        if (event.beat >= endBeat)
            break;

        const int sample = juce::jlimit (0, numSamples - 1,
            (int) std::floor ((event.beat - startBeat) / beatsPerSample));

        const auto message = event.noteOn
            ? juce::MidiMessage::noteOn (event.channel, event.midiNote, (juce::uint8) event.velocity)
            : juce::MidiMessage::noteOff (event.channel, event.midiNote);

        midi.addEvent (message, sample);
    }

    if (endBeat >= t->totalBeats)
    {
        beatPosition.store (t->totalBeats, std::memory_order_release);
        playing.store (false, std::memory_order_release);
    }
    else
    {
        beatPosition.store (endBeat, std::memory_order_release);
    }
}
}
