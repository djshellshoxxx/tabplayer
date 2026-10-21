#pragma once

#include "../core/PerformanceScore.h"
#include "../profile/InstrumentProfile.h"
#include "TechniqueCompiler.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace tabplayer
{
class TabPlaybackEngine
{
public:
    using Event = PlaybackEvent;
    using Timeline = CompiledTimeline;

    /** The last MIDI message the engine sent (lock-free, for UI readouts). */
    struct MessageSummary
    {
        int note = -1;
        int string = -1;
        int fret = 0;
        int velocity = 0;
        int channel = 0;
        double bendSemitones = 0.0;
    };

    /** A note sounding at the current beat (UI query). */
    struct ActiveNote
    {
        int midiNote = 60;
        int channel = 1;
        int stringIndex = -1;
        int fret = 0;
        int velocity = 1;
    };

    //==========================================================================
    // Score and profile (UI thread). Changes emit a reset bundle on the next process() call.
    void setScore (const PerformanceScore& score);
    void setProfile (const InstrumentProfile& profile);
    const InstrumentProfile& getProfile() const noexcept { return profile; }
    void clear();

    //==========================================================================
    // Transport (UI thread). Stop, seek and rewind queue an all-notes-off/reset bundle.
    void play() noexcept;
    void stop() noexcept;
    void rewind() noexcept;
    void seekBeat (double beat) noexcept;

    /** Loop region in beats. Ignored when enabled is false or endBeat <= startBeat. */
    void setLoop (double startBeat, double endBeat, bool enabled) noexcept;
    /** Count-in bars played before playback starts on play(). 0 disables it (max 16). */
    void setCountInBars (int bars) noexcept;
    /** Metronome clicks on MIDI channel 10 during playback (76 on beats, 77 on bar downbeats). */
    void setMetronomeEnabled (bool enabled) noexcept;

    bool isPlaying() const noexcept { return playing.load (std::memory_order_relaxed); }
    double getBeat() const noexcept { return beatPosition.load (std::memory_order_relaxed); }
    double getTotalBeats() const noexcept;
    std::shared_ptr<const Timeline> getTimeline() const noexcept;

    /** Score-time seconds for a beat, using the score's tempo map (120 BPM if no score). */
    double beatToSeconds (double beat) const noexcept;

    //==========================================================================
    // UI getters (lock-free or non-audio).
    MessageSummary getLastMessage() const noexcept;
    std::vector<ActiveNote> getActiveNotes() const;

    //==========================================================================
    /** Renders MIDI for one buffer. Audio-thread safe: no allocation, no parsing.

        bpm is the playback tempo; the score's tempo map scales relative to its initial tempo.
        speed is clamped to 0.1..4.0. When hostPpq is >= 0 the host position is authoritative: a
        jump of more than 1/16 beat re-seeks to it and sends a reset bundle. */
    void process (juce::MidiBuffer& midi, int numSamples, double sampleRate,
                  double bpm, double speed = 1.0, double hostPpq = -1.0);

private:
    void rebuildTimeline();

    // Audio-thread helpers. Only process() calls these.
    void emitEvent (juce::MidiBuffer& midi, int sample, const PlaybackEvent& e) noexcept;
    void emitNoteOn (juce::MidiBuffer& midi, int sample, int channel, int note, int velocity,
                     int stringIndex, int fret) noexcept;
    void emitNoteOff (juce::MidiBuffer& midi, int sample, int channel, int note,
                      int stringIndex, int fret) noexcept;
    void emitReset (juce::MidiBuffer& midi, int sample, const Timeline* t, bool withProgram) noexcept;
    void emitClick (juce::MidiBuffer& midi, int sample, int offSample, bool accent) noexcept;
    void publish (int note, int stringIndex, int fret, int velocity, int channel) noexcept;

    //==========================================================================
    // Shared state (written by UI, read by audio thread).
    std::shared_ptr<const PerformanceScore> scoreCopy;   // UI thread only
    InstrumentProfile profile;                           // UI thread only
    std::shared_ptr<const Timeline> timeline;            // atomic_load/store

    std::atomic<bool> playing { false };
    std::atomic<bool> pendingReset { false };
    std::atomic<bool> countInCancel { false };
    std::atomic<int> countInRequest { 0 };
    std::atomic<int> countInBars { 0 };
    std::atomic<bool> metronomeEnabled { false };
    std::atomic<bool> loopEnabled { false };
    std::atomic<double> loopStart { 0.0 };
    std::atomic<double> loopEnd { 0.0 };
    std::atomic<double> beatPosition { 0.0 };

    std::atomic<int> lastNote { -1 };
    std::atomic<int> lastString { -1 };
    std::atomic<int> lastFret { 0 };
    std::atomic<int> lastVelocity { 0 };
    std::atomic<int> lastChannel { 0 };
    std::atomic<double> lastBend { 0.0 };

    //==========================================================================
    // Audio-thread state.
    std::array<std::uint64_t, 32> soundingBits {};      // (channel-1)*128 + note, 16 * 128 bits
    bool countInActive = false;
    double countInElapsed = 0.0;   ///< quarter-note beats of count-in already played
    double countInTotal = 0.0;
};
}
