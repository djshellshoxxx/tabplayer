#pragma once

#include "../core/PerformanceScore.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <memory>
#include <vector>

namespace tabplayer
{
class TabPlaybackEngine
{
public:
    struct Event
    {
        double beat = 0.0;
        int midiNote = 60;
        int velocity = 100;
        int channel = 1;
        bool noteOn = true;
        int stringIndex = 0;
        int fret = 0;
    };

    struct Timeline
    {
        std::vector<Event> events;
        double totalBeats = 0.0;
    };

    void setScore (const PerformanceScore& score);
    void clear();

    void play() noexcept;
    void stop() noexcept;
    void rewind() noexcept;
    void seekBeat (double beat) noexcept;

    bool isPlaying() const noexcept { return playing.load (std::memory_order_relaxed); }
    double getBeat() const noexcept { return beatPosition.load (std::memory_order_relaxed); }
    double getTotalBeats() const noexcept;
    std::shared_ptr<const Timeline> getTimeline() const noexcept;

    void process (juce::MidiBuffer& midi, int numSamples, double sampleRate,
                  double bpm, double speed = 1.0);

private:
    static std::shared_ptr<Timeline> compile (const PerformanceScore& score);

    std::shared_ptr<const Timeline> timeline;
    std::atomic<bool> playing { false };
    std::atomic<double> beatPosition { 0.0 };
};
}
