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

    engine.stop();
    engine.rewind();
    midi.clear();
    engine.process (midi, 1024, 48000.0, 120.0, 1.0);
    ok &= expect (midi.isEmpty(), "stopped playback emits no MIDI");

    // Bass import regressions: assert sounding octave, string count and MIDI.
    struct BassCase
    {
        const char* header;
        const char* staff;
        int stringCount;
        int topMidi;
        int bottomMidi;
    };
    const BassCase bassCases[] = {
        { "Tuning: 4 string bass", "G|--0--|\nD|-----|\nA|-----|\nE|--0--|", 4, 43, 28 },
        { "Tuning: 5 string bass", "G|--0--|\nD|-----|\nA|-----|\nE|-----|\nB|--0--|", 5, 43, 23 },
        { "Tuning: 6 string bass", "C|--0--|\nG|-----|\nD|-----|\nA|-----|\nE|-----|\nB|--0--|", 6, 48, 23 },
        { "Tuning: drop d bass", "G|--0--|\nD|-----|\nA|-----|\nD|--0--|", 4, 43, 26 },
        { "Tuning: BEAD bass", "D|--0--|\nA|-----|\nE|-----|\nB|--0--|", 4, 38, 23 }
    };
    for (const auto& item : bassCases)
    {
        PerformanceScore bass;
        TabImportDiagnostics bassDiagnostics;
        const juce::String source = juce::String (item.header) + "\n" + item.staff + "\n";
        const bool parsed = importer.read (source, bass, &bassDiagnostics);
        ok &= expect (parsed, "bass dialect imports");
        if (! parsed || bass.getNumTracks() == 0)
            continue;
        const auto& track = bass.getTrack (0);
        ok &= expect (track.numStrings == item.stringCount, "bass string count");
        ok &= expect (track.tuning[0] == item.topMidi, "bass top sounding octave");
        ok &= expect (track.tuning[(size_t) (item.stringCount - 1)] == item.bottomMidi,
                      "bass lowest sounding octave");
        ok &= expect (bass.getTotalNoteCount() == 2, "bass open notes imported");
        for (const auto& measure : track.measures)
            for (const auto& voice : measure.voices)
                for (const auto& note : voice.notes)
                    ok &= expect (note.midiNote == track.tuning[(size_t) note.stringIndex],
                                  "bass notes generate correct MIDI pitch");
    }

    return ok ? 0 : 1;
}
