#include "../src/project/ProjectFile.h"

#include <cmath>
#include <iostream>
#include <limits>

using namespace tabplayer;

static bool expect (bool condition, const char* message)
{
    if (! condition)
        std::cerr << "FAIL: " << message << "\n";
    return condition;
}

static bool near (double a, double b)
{
    return std::abs (a - b) < 1.0e-9;
}

static bool sameProject (const TabPlayerProject& a, const TabPlayerProject& b)
{
    return a.schemaVersion == b.schemaVersion && a.title == b.title && a.sourceFileName == b.sourceFileName
        && a.sourceText == b.sourceText && near (a.speed, b.speed) && near (a.positionBeat, b.positionBeat)
        && a.loopEnabled == b.loopEnabled && near (a.loopStart, b.loopStart) && near (a.loopEnd, b.loopEnd)
        && a.countInBars == b.countInBars && a.metronome == b.metronome
        && a.profile.name == b.profile.name && a.profile.channelMode == b.profile.channelMode
        && a.profile.mappings.size() == b.profile.mappings.size()
        && a.embeddedInstrumentId == b.embeddedInstrumentId
        && a.embeddedInstrumentStateBase64 == b.embeddedInstrumentStateBase64 && near (a.zoom, b.zoom);
}

static juce::var makeObject (std::initializer_list<std::pair<juce::Identifier, juce::var>> items)
{
    auto* obj = new juce::DynamicObject();
    for (const auto& item : items)
        obj->setProperty (item.first, item.second);
    return juce::var (obj);
}

static juce::File testDirectory()
{
    static const juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
        .getChildFile ("tabplayer_project_tests_" + juce::String (juce::Random::getSystemRandom().nextInt()));
    return dir;
}

int main()
{
    bool ok = true;

    // Full round-trip through JSON text.
    TabPlayerProject project;
    project.title = "Song";
    project.sourceFileName = "song.txt";
    project.sourceText = "e|--0---2--|\nB|----------|\n";
    project.speed = 0.75;
    project.positionBeat = 12.5;
    project.loopEnabled = true;
    project.loopStart = 4.0;
    project.loopEnd = 8.0;
    project.countInBars = 1;
    project.metronome = true;
    project.profile = InstrumentProfile::factoryProfiles()[2];   // Guitar Sampler
    project.embeddedInstrumentId = "com.example.guitar";
    project.embeddedInstrumentStateBase64 = "AAECAw==";
    project.zoom = 2.0;

    {
        TabPlayerProject out;
        juce::String error;
        juce::StringArray warnings;
        ok &= expect (TabPlayerProject::fromJsonString (project.toJsonString(), out, error, warnings),
                      "project JSON round-trip loads");
        ok &= expect (sameProject (project, out), "project round-trip preserves fields");
        ok &= expect (out.profile.name == "Guitar Sampler", "embedded profile round-trips");
        ok &= expect (warnings.isEmpty(), "current-version project has no warnings");
    }

    // File round-trip.
    const auto file = testDirectory().getChildFile ("project.tabplayer");
    {
        juce::String error;
        ok &= expect (TabPlayerProject::saveToFile (file, project, error), "project saves to file");

        TabPlayerProject out;
        juce::StringArray warnings;
        ok &= expect (TabPlayerProject::loadFromFile (file, out, error, warnings), "project loads from file");
        ok &= expect (sameProject (project, out), "file round-trip preserves project");
    }

    // Tolerance: unknown fields ignored, missing fields defaulted, NaN and out-of-range clamped.
    {
        const auto v = makeObject ({
            { "schemaVersion", 1 },
            { "futureBlob", juce::var ("ignored") },
            { "speed", 1.0e9 },
            { "positionBeat", -5.0 },
            { "zoom", std::numeric_limits<double>::quiet_NaN() },
            { "countInBars", 99 },
            { "title", juce::var ("ok") },
        });
        TabPlayerProject out;
        juce::String error;
        juce::StringArray warnings;
        ok &= expect (TabPlayerProject::fromVar (v, out, error, warnings), "tolerant project loads");
        ok &= expect (near (out.speed, 4.0), "speed clamped to 4");
        ok &= expect (near (out.positionBeat, 0.0), "negative position clamped to 0");
        ok &= expect (near (out.zoom, 1.0), "NaN zoom falls back to default");
        ok &= expect (out.countInBars == 8, "countInBars clamped to 8");
        ok &= expect (out.title == "ok", "known field read beside unknown fields");
        ok &= expect (out.sourceText.isEmpty(), "missing field takes default");
    }
    {
        TabPlayerProject out;
        juce::String error;
        juce::StringArray warnings;
        ok &= expect (! TabPlayerProject::fromVar (juce::var (3), out, error, warnings), "non-object project rejected");
        ok &= expect (! error.isEmpty(), "non-object project reports error");
    }

    // Empty loop region is disabled with a warning.
    {
        const auto v = makeObject ({ { "schemaVersion", 1 }, { "loopEnabled", true },
                                     { "loopStart", 8.0 }, { "loopEnd", 4.0 } });
        TabPlayerProject out;
        juce::String error;
        juce::StringArray warnings;
        ok &= expect (TabPlayerProject::fromVar (v, out, error, warnings), "inverted loop loads");
        ok &= expect (! out.loopEnabled, "inverted loop disabled");
        ok &= expect (! warnings.isEmpty(), "inverted loop warns");
    }

    // Legacy v0: plugin state {sourceText, beat, speed}.
    {
        const auto v = makeObject ({ { "sourceText", juce::var ("e|--0--|") },
                                     { "beat", 4.5 }, { "speed", 0.5 } });
        TabPlayerProject out;
        juce::String error;
        juce::StringArray warnings;
        ok &= expect (TabPlayerProject::fromVar (v, out, error, warnings), "legacy v0 loads");
        ok &= expect (out.sourceText == "e|--0--|", "legacy sourceText migrated");
        ok &= expect (near (out.positionBeat, 4.5), "legacy beat migrated to positionBeat");
        ok &= expect (near (out.speed, 0.5), "legacy speed kept");
        ok &= expect (out.schemaVersion == 1, "legacy migrated to schema 1");
        ok &= expect (out.profile.name == "Generic MIDI", "legacy profile defaults");
        ok &= expect (warnings.isEmpty(), "legacy load has no warnings");

        // migrate must not mutate its input.
        const auto migrated = TabPlayerProject::migrate (v, 0);
        ok &= expect (v.getProperty (juce::Identifier ("beat"), juce::var()).isDouble(),
                      "migrate leaves caller's legacy object untouched");
        ok &= expect (migrated.getProperty (juce::Identifier ("positionBeat"), juce::var()).isDouble(),
                      "migrate output has positionBeat");
    }

    // Future schema version: known fields load, warning is recorded.
    {
        const auto v = makeObject ({ { "schemaVersion", 7 }, { "title", juce::var ("Later") },
                                     { "speed", 2.0 }, { "newThing", juce::var (true) } });
        TabPlayerProject out;
        juce::String error;
        juce::StringArray warnings;
        ok &= expect (TabPlayerProject::fromVar (v, out, error, warnings), "future version loads");
        ok &= expect (out.title == "Later" && near (out.speed, 2.0), "future version known fields loaded");
        ok &= expect (warnings.size() == 1 && warnings[0].contains ("newer"), "future version warns");
    }

    // Control characters stripped; newline and tab kept.
    {
        TabPlayerProject dirty = project;
        dirty.title = juce::String::fromUTF8 ("a\x01" "b\x7f" "c\td\ne", 9);
        TabPlayerProject out;
        juce::String error;
        juce::StringArray warnings;
        ok &= expect (TabPlayerProject::fromJsonString (dirty.toJsonString(), out, error, warnings),
                      "control-character project loads");
        ok &= expect (out.title == "abc\td\ne", "control characters stripped, tab and newline kept");
        ok &= expect (! warnings.isEmpty(), "sanitization is reported");
    }

    // Corrupt JSON and bad schemaVersion leave the output untouched.
    {
        TabPlayerProject out;
        out.title = "Keep";
        juce::String error;
        juce::StringArray warnings;
        ok &= expect (! TabPlayerProject::fromJsonString ("{\"title\": \"x\", ", out, error, warnings),
                      "corrupt JSON rejected");
        ok &= expect (! error.isEmpty(), "corrupt JSON reports error");
        ok &= expect (out.title == "Keep", "corrupt JSON leaves output untouched");

        ok &= expect (! TabPlayerProject::fromJsonString ("{\"schemaVersion\": \"two\", \"title\": \"x\"}", out, error, warnings),
                      "non-numeric schemaVersion rejected");
        ok &= expect (out.title == "Keep", "bad schemaVersion leaves output untouched");
        ok &= expect (! TabPlayerProject::fromJsonString ("{\"schemaVersion\": -1}", out, error, warnings),
                      "negative schemaVersion rejected");
        ok &= expect (warnings.isEmpty(), "rejected loads add no warnings");
    }

    // Oversized input rejected before parsing, from a string and from a file.
    {
        const std::string big (static_cast<size_t> (TabPlayerProject::maxFileBytes) + 1, 'a');
        TabPlayerProject out;
        out.title = "Keep";
        juce::String error;
        juce::StringArray warnings;
        ok &= expect (! TabPlayerProject::fromJsonString (juce::String (big), out, error, warnings),
                      "oversized project string rejected");
        ok &= expect (out.title == "Keep", "oversized string leaves output untouched");

        const auto bigFile = testDirectory().getChildFile ("big.tabplayer");
        bigFile.replaceWithText (juce::String (big));
        ok &= expect (! TabPlayerProject::loadFromFile (bigFile, out, error, warnings),
                      "oversized project file rejected");
        ok &= expect (out.title == "Keep", "oversized file leaves output untouched");
    }

    testDirectory().deleteRecursively();
    return ok ? 0 : 1;
}
