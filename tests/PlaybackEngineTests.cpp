#include "../src/playback/TabPlaybackEngine.h"
#include "../src/import/TabImportPipeline.h"

#include <iostream>

using namespace tabplayer;

static bool expect (bool condition, const char* message)
{
    if (! condition)
        std::cerr << "FAIL: " << message << "\n";
    return condition;
}

int main()
{
    PerformanceScore score;
    TabImportPipeline importer;
    TabImportDiagnostics diagnostics;

    const juce::String tab =
        "Tempo: 120\n"
        "e|--0---2---3---2--|\n"
        "B|------------------|\n"
        "G|------------------|\n"
        "D|------------------|\n"
        "A|------------------|\n"
        "E|------------------|\n";

    bool ok = true;
    ok &= expect (importer.read (tab, score, &diagnostics), "ASCII tab imports");
    ok &= expect (score.getTotalNoteCount() == 4, "four notes parsed");

    TabPlaybackEngine engine;
    engine.setScore (score);
    ok &= expect (engine.getTotalBeats() > 0.0, "timeline has duration");

    juce::MidiBuffer midi;
    engine.play();
    engine.process (midi, 48000, 48000.0, 120.0, 1.0);
    ok &= expect (! midi.isEmpty(), "playback emits MIDI");

    // Stop queues one reset bundle; after it is delivered the engine is silent.
    engine.stop();
    engine.rewind();
    midi.clear();
    engine.process (midi, 1024, 48000.0, 120.0, 1.0);
    bool sentReset = false;
    for (const auto meta : midi)
        sentReset = sentReset || (meta.getMessage().isController() && meta.getMessage().getControllerNumber() == 123);
    ok &= expect (sentReset, "stop/rewind delivers an all-notes-off reset");

    midi.clear();
    engine.process (midi, 1024, 48000.0, 120.0, 1.0);
    ok &= expect (midi.isEmpty(), "stopped playback emits no MIDI");

    // Note-off before note-on at the same timestamp (single channel, back-to-back notes).
    {
        PerformanceScore back;
        auto& track = back.getTrack (0);
        track.measures.clear();
        for (int m = 0; m < 2; ++m)
        {
            ScoreMeasure measure;
            measure.voices.resize (1);
            track.measures.push_back (measure);
        }

        ScoreNote a;
        a.startBeat = 0.0;
        a.durationBeats = 1.0;
        a.midiNote = 60;
        a.stringIndex = 0;
        a.velocity = 0.8;
        track.measures[0].voices[0].notes.push_back (a);

        ScoreNote b = a;
        b.startBeat = 1.0;
        b.midiNote = 62;
        track.measures[0].voices[0].notes.push_back (b);

        InstrumentProfile single;
        single.channelMode = ChannelMode::single;

        TabPlaybackEngine seq;
        seq.setProfile (single);
        seq.setScore (back);
        seq.play();

        juce::MidiBuffer out;
        seq.process (out, 48000, 48000.0, 120.0, 1.0); // beats 0..2 at 120 BPM

        int offIndex = -1;
        int onIndex = -1;
        int position = 0;
        for (const auto meta : out)
        {
            const auto msg = meta.getMessage();
            if (msg.isNoteOff() && msg.getNoteNumber() == 60 && meta.samplePosition == 24000)
                offIndex = position;
            if (msg.isNoteOn() && msg.getNoteNumber() == 62 && meta.samplePosition == 24000)
                onIndex = position;
            ++position;
        }

        ok &= expect (offIndex >= 0 && onIndex > offIndex, "note-off precedes note-on at the same sample");
    }

    std::cout << (ok ? "PlaybackEngineTests passed\n" : "PlaybackEngineTests FAILED\n");
    return ok ? 0 : 1;
}
