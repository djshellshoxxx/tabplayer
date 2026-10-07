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

    return ok ? 0 : 1;
}
