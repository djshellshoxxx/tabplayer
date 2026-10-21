#include "InstrumentProfile.h"

#include <cmath>
#include <limits>

namespace tabplayer
{
namespace
{
constexpr int maxMappings = 64;

const char* const articulationNames[] = {
    "palmMute", "deadNote", "harmonic", "tap", "slap", "pop", "legato",
    "staccato", "accent", "ghost", "letRing"
};
static_assert (sizeof (articulationNames) / sizeof (articulationNames[0])
                   == static_cast<size_t> (Articulation::numArticulations),
               "articulationNames must cover every Articulation");

const char* channelModeName (ChannelMode m)
{
    switch (m)
    {
        case ChannelMode::single:       return "single";
        case ChannelMode::perString:    return "perString";
        case ChannelMode::mpeLowerZone: return "mpeLowerZone";
    }
    return "perString";
}

bool parseChannelMode (const juce::var& v, ChannelMode& out)
{
    if (v.isString())
    {
        const auto s = v.toString();
        if (s == "single")       { out = ChannelMode::single; return true; }
        if (s == "perString")    { out = ChannelMode::perString; return true; }
        if (s == "mpeLowerZone") { out = ChannelMode::mpeLowerZone; return true; }
        return false;
    }

    if (v.isInt() || v.isInt64() || v.isDouble())
    {
        const double d = v;
        if (std::isfinite (d) && d >= 0.0 && d <= 2.0)
        {
            out = static_cast<ChannelMode> (static_cast<int> (std::lround (d)));
            return true;
        }
    }

    return false;
}

bool parseArticulation (const juce::var& v, Articulation& out)
{
    if (v.isString())
    {
        const auto s = v.toString();
        for (int i = 0; i < static_cast<int> (Articulation::numArticulations); ++i)
            if (s == articulationNames[i])
            {
                out = static_cast<Articulation> (i);
                return true;
            }
        return false;
    }

    if (v.isInt() || v.isInt64() || v.isDouble())
    {
        const double d = v;
        if (std::isfinite (d) && d >= 0.0 && d < static_cast<double> (Articulation::numArticulations))
        {
            out = static_cast<Articulation> (static_cast<int> (d));
            return true;
        }
    }

    return false;
}

/** Numeric read: non-numeric or non-finite input yields fallback; finite input is clamped. */
double readDouble (const juce::var& obj, const char* key, double fallback, double lo, double hi)
{
    const auto v = obj.getProperty (juce::Identifier (key), juce::var());
    if (! (v.isInt() || v.isInt64() || v.isDouble()))
        return fallback;

    const double d = v;
    if (! std::isfinite (d))
        return fallback;

    return juce::jlimit (lo, hi, d);
}

int readInt (const juce::var& obj, const char* key, int fallback, int lo, int hi)
{
    const auto v = obj.getProperty (juce::Identifier (key), juce::var());
    if (! (v.isInt() || v.isInt64() || v.isDouble()))
        return fallback;

    const double d = v;
    if (! std::isfinite (d))
        return fallback;

    return juce::jlimit (lo, hi, static_cast<int> (std::lround (juce::jlimit (-1.0e9, 1.0e9, d))));
}

bool readBool (const juce::var& obj, const char* key, bool fallback)
{
    const auto v = obj.getProperty (juce::Identifier (key), juce::var());
    if (v.isBool())
        return static_cast<bool> (v);
    if (v.isInt() || v.isInt64())
        return static_cast<int64_t> (v) != 0;
    return fallback;
}

juce::String readString (const juce::var& obj, const char* key, const juce::String& fallback, int maxChars)
{
    const auto v = obj.getProperty (juce::Identifier (key), juce::var());
    if (! v.isString())
        return fallback;

    auto s = v.toString();
    if (s.length() > maxChars)
        s = s.substring (0, maxChars);
    return s;
}

ArticulationMapping mappingFromVar (const juce::var& v, bool& ok)
{
    ArticulationMapping m;
    ok = false;
    if (! v.isObject())
        return m;

    Articulation a;
    if (! parseArticulation (v.getProperty (juce::Identifier ("articulation"), juce::var()), a))
        return m;

    m.articulation = a;
    m.keyswitchNote = readInt (v, "keyswitchNote", -1, -1, 127);
    m.ccNumber      = readInt (v, "ccNumber", -1, -1, 127);
    m.ccValue       = readInt (v, "ccValue", 127, 0, 127);
    m.ccOffValue    = readInt (v, "ccOffValue", 0, 0, 127);
    ok = true;
    return m;
}
}

const ArticulationMapping* InstrumentProfile::find (Articulation a) const noexcept
{
    for (const auto& m : mappings)
        if (m.articulation == a)
            return &m;
    return nullptr;
}

juce::var InstrumentProfile::toVar() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("name", name);
    obj->setProperty ("channelMode", channelModeName (channelMode));
    obj->setProperty ("singleChannel", singleChannel);
    obj->setProperty ("pitchBendRangeSemitones", pitchBendRangeSemitones);
    obj->setProperty ("keyswitchChannel", keyswitchChannel);

    juce::Array<juce::var> mappingVars;
    for (const auto& m : mappings)
    {
        auto* mo = new juce::DynamicObject();
        mo->setProperty ("articulation", articulationNames[static_cast<int> (m.articulation)]);
        mo->setProperty ("keyswitchNote", m.keyswitchNote);
        mo->setProperty ("ccNumber", m.ccNumber);
        mo->setProperty ("ccValue", m.ccValue);
        mo->setProperty ("ccOffValue", m.ccOffValue);
        mappingVars.add (juce::var (mo));
    }
    obj->setProperty ("mappings", mappingVars);

    obj->setProperty ("palmMuteDurationScale", palmMuteDurationScale);
    obj->setProperty ("palmMuteVelocityScale", palmMuteVelocityScale);
    obj->setProperty ("staccatoDurationScale", staccatoDurationScale);
    obj->setProperty ("legatoOverlapBeats", legatoOverlapBeats);
    obj->setProperty ("ghostVelocityScale", ghostVelocityScale);
    obj->setProperty ("accentVelocityScale", accentVelocityScale);
    obj->setProperty ("sendVibrato", sendVibrato);
    obj->setProperty ("sendSlides", sendSlides);
    obj->setProperty ("sendBends", sendBends);
    obj->setProperty ("programChange", programChange);

    return juce::var (obj);
}

InstrumentProfile InstrumentProfile::fromVar (const juce::var& v)
{
    InstrumentProfile p;
    if (! v.isObject())
        return p;

    const InstrumentProfile defaults;

    p.name = readString (v, "name", defaults.name, 128);
    if (p.name.trim().isEmpty())
        p.name = defaults.name;

    if (auto cm = v.getProperty (juce::Identifier ("channelMode"), juce::var()); ! cm.isVoid())
        if (! parseChannelMode (cm, p.channelMode))
            p.channelMode = defaults.channelMode;

    p.singleChannel             = readInt (v, "singleChannel", defaults.singleChannel, 1, 16);
    p.pitchBendRangeSemitones   = readDouble (v, "pitchBendRangeSemitones", defaults.pitchBendRangeSemitones, 0.0, 96.0);
    p.keyswitchChannel          = readInt (v, "keyswitchChannel", defaults.keyswitchChannel, 1, 16);

    if (auto mappingsVar = v.getProperty (juce::Identifier ("mappings"), juce::var()); mappingsVar.isArray())
    {
        for (const auto& item : *mappingsVar.getArray())
        {
            if (static_cast<int> (p.mappings.size()) >= maxMappings)
                break;

            bool ok = false;
            auto m = mappingFromVar (item, ok);
            if (ok)
                p.mappings.push_back (m);
        }
    }

    p.palmMuteDurationScale   = readDouble (v, "palmMuteDurationScale", defaults.palmMuteDurationScale, 0.01, 4.0);
    p.palmMuteVelocityScale   = readDouble (v, "palmMuteVelocityScale", defaults.palmMuteVelocityScale, 0.0, 2.0);
    p.staccatoDurationScale   = readDouble (v, "staccatoDurationScale", defaults.staccatoDurationScale, 0.01, 4.0);
    p.legatoOverlapBeats      = readDouble (v, "legatoOverlapBeats", defaults.legatoOverlapBeats, 0.0, 1.0);
    p.ghostVelocityScale      = readDouble (v, "ghostVelocityScale", defaults.ghostVelocityScale, 0.0, 2.0);
    p.accentVelocityScale     = readDouble (v, "accentVelocityScale", defaults.accentVelocityScale, 0.0, 2.0);
    p.sendVibrato             = readBool (v, "sendVibrato", defaults.sendVibrato);
    p.sendSlides              = readBool (v, "sendSlides", defaults.sendSlides);
    p.sendBends               = readBool (v, "sendBends", defaults.sendBends);
    p.programChange           = readInt (v, "programChange", defaults.programChange, -1, 127);

    return p;
}

InstrumentProfile InstrumentProfile::genericMidi()
{
    InstrumentProfile p;
    p.name = "Generic MIDI";
    p.channelMode = ChannelMode::perString;
    return p;
}

std::vector<InstrumentProfile> InstrumentProfile::factoryProfiles()
{
    std::vector<InstrumentProfile> out;

    out.push_back (genericMidi());

    {
        InstrumentProfile p;
        p.name = "Generic MPE";
        p.channelMode = ChannelMode::mpeLowerZone;
        p.pitchBendRangeSemitones = 48.0;
        out.push_back (p);
    }

    {
        InstrumentProfile p;
        p.name = "Guitar Sampler";
        p.channelMode = ChannelMode::single;
        p.singleChannel = 1;
        p.mappings = {
            { Articulation::palmMute, 24, -1, 127, 0 },
            { Articulation::deadNote, 25, -1, 127, 0 },
            { Articulation::harmonic, 26, -1, 127, 0 },
        };
        out.push_back (p);
    }

    {
        InstrumentProfile p;
        p.name = "Bass Sampler";
        p.channelMode = ChannelMode::single;
        p.singleChannel = 1;
        p.palmMuteDurationScale = 0.45;
        p.mappings = {
            { Articulation::palmMute, 28, -1, 127, 0 },
            { Articulation::deadNote, 29, -1, 127, 0 },
            { Articulation::harmonic, 30, -1, 127, 0 },
            { Articulation::slap,     31, -1, 127, 0 },
            { Articulation::pop,      32, -1, 127, 0 },
        };
        out.push_back (p);
    }

    {
        InstrumentProfile p;
        p.name = "Monophonic Lead";
        p.channelMode = ChannelMode::single;
        p.singleChannel = 1;
        p.legatoOverlapBeats = 0.05;
        p.mappings = {
            { Articulation::legato, -1, 68, 127, 0 },   // legato footswitch CC
        };
        out.push_back (p);
    }

    {
        InstrumentProfile p;
        p.name = "Keyswitch Orchestral";
        p.channelMode = ChannelMode::single;
        p.singleChannel = 1;
        p.programChange = 0;
        p.mappings = {
            { Articulation::staccato, 24, -1, 127, 0 },
            { Articulation::legato,   25, -1, 127, 0 },
            { Articulation::accent,   26, -1, 127, 0 },
            { Articulation::ghost,    27, -1, 127, 0 },
            { Articulation::letRing,  28, -1, 127, 0 },
        };
        out.push_back (p);
    }

    return out;
}
}
