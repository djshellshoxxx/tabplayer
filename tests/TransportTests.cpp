#include "../src/playback/TabPlaybackEngine.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <vector>

using namespace tabplayer;

namespace
{
constexpr double kRate = 48000.0;
constexpr int kBlock = 512;

bool expect (bool condition, const char* message)
{
    if (! condition)
        std::cerr << "FAIL: " << message << "\n";
    return condition;
}

struct Msg
{
    long long sample = 0;
    juce::MidiMessage message;
};

/** Renders totalSamples in blocks, returning every message with its absolute sample position. */
std::vector<Msg> render (TabPlaybackEngine& engine, long long totalSamples, int block, double bpm,
                         double speed = 1.0, double hostPpq = -1.0)
{
    std::vector<Msg> out;
    long long done = 0;

    while (done < totalSamples)
    {
        const int n = (int) juce::jmin ((long long) block, totalSamples - done);
        juce::MidiBuffer midi;
        engine.process (midi, n, kRate, bpm, speed, hostPpq);

        for (const auto meta : midi)
            out.push_back ({ done + meta.samplePosition, meta.getMessage() });

        done += n;
    }

    return out;
}

int countNoteOn (const std::vector<Msg>& msgs, int channel, int note = -1)
{
    int count = 0;
    for (const auto& m : msgs)
        if (m.message.isNoteOn() && m.message.getChannel() == channel
            && (note < 0 || m.message.getNoteNumber() == note))
            ++count;

    return count;
}

int countController (const std::vector<Msg>& msgs, int number)
{
    int count = 0;
    for (const auto& m : msgs)
        if (m.message.isController() && m.message.getControllerNumber() == number)
            ++count;

    return count;
}

/** Replays the messages as a sounding-note model. Returns the number of note-ons that retrigger a
    note still sounding; sets stillSounding to the number of notes left on at the end. */
int replay (const std::vector<Msg>& msgs, int& stillSounding)
{
    std::set<std::pair<int, int>> sounding;
    int retriggers = 0;

    for (const auto& m : msgs)
    {
        const auto key = std::make_pair (m.message.getChannel(), m.message.getNoteNumber());

        if (m.message.isNoteOn())
        {
            if (sounding.count (key) != 0)
                ++retriggers;
            sounding.insert (key);
        }
        else if (m.message.isNoteOff())
        {
            sounding.erase (key);
        }
    }

    stillSounding = (int) sounding.size();
    return retriggers;
}

PerformanceScore makeScore (const std::vector<std::pair<double, double>>& startsAndDurations, int string = 0)
{
    PerformanceScore score;
    auto& track = score.getTrack (0);
    track.measures.clear();

    for (int m = 0; m < 4; ++m)
    {
        ScoreMeasure measure;
        measure.voices.resize (1);
        track.measures.push_back (measure);
    }

    for (const auto& [start, dur] : startsAndDurations)
    {
        const int m = (int) std::floor (start / 4.0);
        ScoreNote note;
        note.startBeat = start - (double) m * 4.0;
        note.durationBeats = dur;
        note.stringIndex = string;
        note.midiNote = 60 + string;
        note.velocity = 0.8;
        track.measures[(size_t) m].voices[0].notes.push_back (note);
    }

    return score;
}
} // namespace

int main()
{
    bool ok = true;

    // No MIDI when stopped, and a fresh engine is silent.
    {
        TabPlaybackEngine engine;
        ok &= expect (render (engine, kBlock, kBlock, 120.0).empty(), "fresh engine emits nothing");

        engine.setScore (makeScore ({ { 0.0, 2.0 } }));
        render (engine, kBlock, kBlock, 120.0); // the post-setScore reset goes out here
        ok &= expect (render (engine, kBlock, kBlock, 120.0).empty(), "unplayed engine emits no notes");

        engine.play();
        ok &= expect (! render (engine, kBlock, kBlock, 120.0).empty(), "playing emits MIDI");
        engine.stop();
        render (engine, kBlock, kBlock, 120.0); // stop reset
        ok &= expect (render (engine, 4 * kBlock, kBlock, 120.0).empty(), "stopped engine emits no MIDI");
    }

    // Stop sends all-notes-off, sustain off and bend centre for the used channel, and note-offs for held notes.
    {
        TabPlaybackEngine engine;
        engine.setScore (makeScore ({ { 0.0, 4.0 } }));
        engine.play();
        render (engine, kBlock, kBlock, 120.0);

        engine.stop();
        const auto msgs = render (engine, kBlock, kBlock, 120.0);
        ok &= expect (countController (msgs, 123) >= 1, "stop sends CC123 on the used channel");
        ok &= expect (countController (msgs, 64) >= 1, "stop sends sustain off");
        bool centre = false;
        for (const auto& m : msgs)
            centre = centre || (m.message.isPitchWheel() && m.message.getPitchWheelValue() == 8192);
        ok &= expect (centre, "stop sends pitch-bend centre");

        bool heldOff = false;
        for (const auto& m : msgs)
            heldOff = heldOff || (m.message.isNoteOff() && m.message.getNoteNumber() == 60);
        ok &= expect (heldOff, "stop note-offs the held note");
    }

    // Seek sends a reset before the new position's events, and the held note is released first.
    {
        TabPlaybackEngine engine;
        engine.setScore (makeScore ({ { 0.0, 4.0 }, { 2.0, 1.0 } }));
        engine.play();
        render (engine, kBlock, kBlock, 120.0);

        engine.seekBeat (0.5);
        const auto msgs = render (engine, kBlock, kBlock, 120.0);
        ok &= expect (! msgs.empty() && msgs.front().sample == 0, "seek reset goes out at the buffer start");
        ok &= expect (countController (msgs, 123) >= 1, "seek sends CC123");
        ok &= expect (! msgs.empty() && msgs.front().message.isNoteOff(), "seek note-offs precede other messages");
        ok &= expect (engine.isPlaying(), "seek keeps playing");
    }

    // A loop wraps without stuck or retriggered notes.
    {
        TabPlaybackEngine engine;
        engine.setScore (makeScore ({ { 0.0, 1.0 }, { 1.0, 1.0 }, { 2.5, 0.5 } }));
        engine.setLoop (0.0, 3.0, true);
        engine.play();

        const auto msgs = render (engine, 6LL * (long long) kRate, kBlock, 120.0); // 12 beats = 4 passes
        int left = 0;
        const int retriggers = replay (msgs, left);

        ok &= expect (countController (msgs, 123) >= 3, "each loop wrap resets");
        ok &= expect (retriggers == 0, "loop wrap does not retrigger a sounding note");
        ok &= expect (countNoteOn (msgs, 1, 60) >= 4, "looped note plays on every pass");

        engine.stop();
        const auto tail = render (engine, kBlock, kBlock, 120.0);
        std::vector<Msg> everything = msgs;
        everything.insert (everything.end(), tail.begin(), tail.end());
        int stuck = 0;
        replay (everything, stuck);
        ok &= expect (stuck == 0, "no notes left sounding after stop");
    }

    // Count-in bars and metronome clicks.
    {
        TabPlaybackEngine engine;
        engine.setScore (makeScore ({ { 0.0, 1.0 } }));
        engine.setCountInBars (2);
        engine.setMetronomeEnabled (false);
        engine.play();

        // 2 bars of 4 beats at 120 BPM = 4 s.
        const auto countIn = render (engine, 4LL * (long long) kRate, kBlock, 120.0);
        ok &= expect (countNoteOn (countIn, 10) == 8, "count-in plays 8 clicks for 2 bars");
        ok &= expect (countNoteOn (countIn, 10, 77) == 2, "count-in downbeats are accented");
        ok &= expect (countNoteOn (countIn, 1) == 0, "playhead does not move during count-in");
        ok &= expect (engine.getBeat() == 0.0, "beat holds at 0 during count-in");

        engine.setMetronomeEnabled (true);
        // Score is 4 measures (16 beats) = 8 s at 120 BPM.
        const auto playback = render (engine, 8LL * (long long) kRate, kBlock, 120.0);
        ok &= expect (countNoteOn (playback, 10) == 16, "metronome clicks on every beat");
        ok &= expect (countNoteOn (playback, 10, 77) == 4, "metronome accents bar downbeats");
        ok &= expect (countNoteOn (playback, 1, 60) == 1, "score note plays during playback");
    }

    // Host sync: a jump re-seeks and resets; a small drift does not.
    {
        TabPlaybackEngine engine;
        engine.setScore (makeScore ({ { 0.0, 1.0 }, { 8.0, 1.0 } }));
        engine.play();
        render (engine, kBlock, kBlock, 120.0, 1.0, 0.0);

        const auto jump = render (engine, kBlock, kBlock, 120.0, 1.0, 8.0);
        ok &= expect (countController (jump, 123) >= 1, "host discontinuity sends a reset");
        ok &= expect (engine.getBeat() >= 8.0 && engine.getBeat() < 8.1, "host discontinuity re-seeks");

        const double before = engine.getBeat();
        const auto drift = render (engine, kBlock, kBlock, 120.0, 1.0, before + 1.0 / 64.0);
        ok &= expect (countController (drift, 123) == 0, "small drift within 1/16 beat does not reset");
    }

    // Deterministic output at a fixed tempo, and speed is clamped to 0.1..4.0.
    {
        auto score = makeScore ({ { 0.0, 1.0 }, { 1.0, 0.5 }, { 2.0, 2.0 } });
        TabPlaybackEngine a, b;
        a.setScore (score);
        b.setScore (score);
        a.play();
        b.play();

        const auto ma = render (a, 3LL * (long long) kRate, kBlock, 120.0, 1.0);
        const auto mb = render (b, 3LL * (long long) kRate, kBlock, 120.0, 1.0);

        bool same = ma.size() == mb.size() && ! ma.empty();
        for (size_t i = 0; same && i < ma.size(); ++i)
            same = ma[i].sample == mb[i].sample
                   && ma[i].message.getRawDataSize() == mb[i].message.getRawDataSize()
                   && std::equal (ma[i].message.getRawData(), ma[i].message.getRawData() + ma[i].message.getRawDataSize(),
                                  mb[i].message.getRawData());
        ok &= expect (same, "same inputs give identical MIDI");

        TabPlaybackEngine fast, capped;
        fast.setScore (score);
        capped.setScore (score);
        fast.play();
        capped.play();
        const auto mf = render (fast, 2LL * (long long) kRate, kBlock, 120.0, 40.0);
        const auto mc = render (capped, 2LL * (long long) kRate, kBlock, 120.0, 4.0);
        ok &= expect (mf.size() == mc.size(), "speed above 4.0 is clamped to 4.0");
    }

    // Setting a profile while playing resets the instrument.
    {
        TabPlaybackEngine engine;
        engine.setScore (makeScore ({ { 0.0, 4.0 } }));
        engine.play();
        render (engine, kBlock, kBlock, 120.0);

        InstrumentProfile profile;
        profile.name = "Changed";
        engine.setProfile (profile);
        ok &= expect (countController (render (engine, kBlock, kBlock, 120.0), 123) >= 1,
                      "setProfile sends a reset");
    }

    // UI getters: last message and active notes.
    {
        TabPlaybackEngine engine;
        engine.setScore (makeScore ({ { 0.0, 2.0 } }, 1));
        engine.play();
        render (engine, kBlock, kBlock, 120.0);

        const auto last = engine.getLastMessage();
        ok &= expect (last.note == 61 && last.string == 1 && last.channel == 2 && last.velocity > 0,
                      "last message summary reports the sounding note");

        const auto active = engine.getActiveNotes();
        ok &= expect (active.size() == 1 && active.front().midiNote == 61, "active notes reflect the current beat");
    }

    std::cout << (ok ? "TransportTests passed\n" : "TransportTests FAILED\n");
    return ok ? 0 : 1;
}
