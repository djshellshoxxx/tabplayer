#include "../src/profile/InstrumentProfile.h"
#include "../src/profile/ProfileStore.h"

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

static bool sameMapping (const ArticulationMapping& a, const ArticulationMapping& b)
{
    return a.articulation == b.articulation && a.keyswitchNote == b.keyswitchNote
        && a.ccNumber == b.ccNumber && a.ccValue == b.ccValue && a.ccOffValue == b.ccOffValue;
}

static bool sameProfile (const InstrumentProfile& a, const InstrumentProfile& b)
{
    if (a.name != b.name || a.channelMode != b.channelMode || a.singleChannel != b.singleChannel
        || a.keyswitchChannel != b.keyswitchChannel || a.programChange != b.programChange
        || a.sendVibrato != b.sendVibrato || a.sendSlides != b.sendSlides || a.sendBends != b.sendBends)
        return false;

    if (! near (a.pitchBendRangeSemitones, b.pitchBendRangeSemitones)
        || ! near (a.palmMuteDurationScale, b.palmMuteDurationScale)
        || ! near (a.palmMuteVelocityScale, b.palmMuteVelocityScale)
        || ! near (a.staccatoDurationScale, b.staccatoDurationScale)
        || ! near (a.legatoOverlapBeats, b.legatoOverlapBeats)
        || ! near (a.ghostVelocityScale, b.ghostVelocityScale)
        || ! near (a.accentVelocityScale, b.accentVelocityScale))
        return false;

    if (a.mappings.size() != b.mappings.size())
        return false;

    for (size_t i = 0; i < a.mappings.size(); ++i)
        if (! sameMapping (a.mappings[i], b.mappings[i]))
            return false;

    return true;
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
        .getChildFile ("tabplayer_profile_tests_" + juce::String (juce::Random::getSystemRandom().nextInt()));
    return dir;
}

static juce::File scratchFile (const char* name)
{
    return testDirectory().getChildFile (name);
}

int main()
{
    bool ok = true;

    // Factory profiles: presence, names, and key properties.
    const auto factory = InstrumentProfile::factoryProfiles();
    ok &= expect (factory.size() >= 6, "six factory profiles present");

    bool uniqueNames = true;
    for (size_t i = 0; i < factory.size(); ++i)
    {
        uniqueNames &= ! factory[i].name.isEmpty();
        for (size_t j = i + 1; j < factory.size(); ++j)
            uniqueNames &= factory[i].name != factory[j].name;
    }
    ok &= expect (uniqueNames, "factory profile names are unique and non-empty");

    const InstrumentProfile* generic = nullptr;
    const InstrumentProfile* mpe = nullptr;
    const InstrumentProfile* guitar = nullptr;
    const InstrumentProfile* lead = nullptr;
    for (const auto& p : factory)
    {
        if (p.name == "Generic MIDI")       generic = &p;
        if (p.name == "Generic MPE")        mpe = &p;
        if (p.name == "Guitar Sampler")     guitar = &p;
        if (p.name == "Monophonic Lead")    lead = &p;
    }
    ok &= expect (generic && generic->channelMode == ChannelMode::perString, "Generic MIDI is per-string");
    ok &= expect (mpe && mpe->channelMode == ChannelMode::mpeLowerZone && near (mpe->pitchBendRangeSemitones, 48.0),
                  "Generic MPE uses lower zone with 48 semitone bend");
    ok &= expect (guitar && guitar->channelMode == ChannelMode::single, "Guitar Sampler is single channel");
    ok &= expect (guitar && guitar->find (Articulation::palmMute) && guitar->find (Articulation::palmMute)->keyswitchNote >= 0,
                  "Guitar Sampler has palm-mute keyswitch");
    ok &= expect (guitar && guitar->find (Articulation::deadNote) && guitar->find (Articulation::harmonic),
                  "Guitar Sampler has dead-note and harmonic keyswitches");
    ok &= expect (lead && lead->channelMode == ChannelMode::single, "Monophonic Lead is single channel");
    ok &= expect (lead && lead->find (Articulation::legato) != nullptr, "Monophonic Lead maps legato");
    ok &= expect (generic && generic->find (Articulation::palmMute) == nullptr, "find returns nullptr when unmapped");

    // Factory round-trip through var and through a file.
    for (const auto& p : factory)
    {
        const auto back = InstrumentProfile::fromVar (p.toVar());
        ok &= expect (sameProfile (p, back), "factory profile survives toVar/fromVar");
    }

    const auto file = scratchFile ("profile.json");
    juce::String error;
    ok &= expect (ProfileStore::saveToFile (file, *guitar, error), "profile saves to file");
    InstrumentProfile loaded;
    ok &= expect (ProfileStore::loadFromFile (file, loaded, error), "profile loads from file");
    ok &= expect (sameProfile (*guitar, loaded), "file round-trip preserves profile");
    ok &= expect (error.isEmpty(), "successful load clears error");

    // Tolerance: junk, unknown fields, NaN, out-of-range values.
    {
        const auto p = InstrumentProfile::fromVar (juce::var (42));
        ok &= expect (sameProfile (p, InstrumentProfile {}), "non-object input yields defaults");
    }
    {
        const auto v = makeObject ({
            { "name", "Mine" },
            { "futureField", juce::var (true) },
            { "singleChannel", 99 },
            { "keyswitchChannel", -4 },
            { "pitchBendRangeSemitones", 1000.0 },
            { "palmMuteDurationScale", std::numeric_limits<double>::quiet_NaN() },
            { "palmMuteVelocityScale", std::numeric_limits<double>::infinity() },
            { "sendVibrato", "yes" },
            { "programChange", 500 },
            { "channelMode", "bogus" },
        });
        const auto p = InstrumentProfile::fromVar (v);
        const InstrumentProfile defaults;
        ok &= expect (p.name == "Mine", "custom name read");
        ok &= expect (p.singleChannel == 16, "singleChannel clamped to 16");
        ok &= expect (p.keyswitchChannel == 1, "keyswitchChannel clamped to 1");
        ok &= expect (near (p.pitchBendRangeSemitones, 96.0), "bend range clamped to 96");
        ok &= expect (near (p.palmMuteDurationScale, defaults.palmMuteDurationScale), "NaN falls back to default");
        ok &= expect (near (p.palmMuteVelocityScale, defaults.palmMuteVelocityScale), "infinite falls back to default");
        ok &= expect (p.sendVibrato == defaults.sendVibrato, "non-bool falls back to default");
        ok &= expect (p.programChange == 127, "programChange clamped to 127");
        ok &= expect (p.channelMode == ChannelMode::perString, "unknown channel mode falls back");
    }
    {
        // Mappings: unknown articulations skipped, values clamped, count capped.
        auto* root = new juce::DynamicObject();
        juce::Array<juce::var> maps;
        maps.add (makeObject ({ { "articulation", "palmMute" }, { "keyswitchNote", 500 } }));
        maps.add (makeObject ({ { "articulation", "noSuchThing" }, { "keyswitchNote", 3 } }));
        maps.add (makeObject ({ { "articulation", juce::var (6) }, { "ccNumber", -20 }, { "ccValue", 300 } }));
        maps.add (juce::var (7));
        root->setProperty ("mappings", maps);
        const auto p = InstrumentProfile::fromVar (juce::var (root));
        ok &= expect (p.mappings.size() == 2, "unknown and non-object mappings are skipped");
        if (p.mappings.size() == 2)
        {
            ok &= expect (p.mappings[0].keyswitchNote == 127, "keyswitchNote clamped to 127");
            ok &= expect (p.mappings[1].articulation == Articulation::legato, "numeric articulation accepted");
            ok &= expect (p.mappings[1].ccNumber == -1, "ccNumber -20 clamped to -1");
            ok &= expect (p.mappings[1].ccValue == 127, "ccValue 300 clamped to 127");
        }
    }
    {
        // Count cap: 200 valid entries stop at 64.
        juce::Array<juce::var> maps;
        for (int i = 0; i < 200; ++i)
            maps.add (makeObject ({ { "articulation", "tap" }, { "keyswitchNote", i % 128 } }));
        auto* root = new juce::DynamicObject();
        root->setProperty ("mappings", maps);
        ok &= expect (InstrumentProfile::fromVar (juce::var (root)).mappings.size() == 64, "mappings capped at 64");
    }

    // Store: corrupt JSON and oversized files leave the output untouched.
    {
        InstrumentProfile target;
        target.name = "Keep Me";
        const auto bad = scratchFile ("corrupt.json");
        bad.replaceWithText ("{\"name\": \"x\", ");
        juce::String e;
        ok &= expect (! ProfileStore::loadFromFile (bad, target, e), "corrupt JSON rejected");
        ok &= expect (! e.isEmpty(), "corrupt JSON reports error");
        ok &= expect (target.name == "Keep Me", "corrupt load leaves output untouched");

        const auto big = scratchFile ("big.json");
        big.replaceWithText (juce::String (std::string (static_cast<size_t> (ProfileStore::maxProfileFileBytes) + 1, ' ')));
        ok &= expect (! ProfileStore::loadFromFile (big, target, e), "oversized profile rejected");
        ok &= expect (target.name == "Keep Me", "oversized load leaves output untouched");

        const auto missing = scratchFile ("missing.json");
        ok &= expect (! ProfileStore::loadFromFile (missing, target, e), "missing file rejected");
        ok &= expect (! e.isEmpty(), "missing file reports error");
    }

    testDirectory().deleteRecursively();
    return ok ? 0 : 1;
}
