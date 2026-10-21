#include "ProjectFile.h"

#include <cmath>
#include <string>

namespace tabplayer
{
namespace
{
constexpr double maxBeat = 1.0e7;

/** Removes control characters except newline and tab (also DEL). Works on UTF-8 bytes, where
    every multi-byte sequence has its high bit set, so ASCII controls are never split. */
juce::String sanitize (const juce::String& s, bool& changed)
{
    const auto utf8 = s.toStdString();
    std::string out;
    out.reserve (utf8.size());

    for (const char ch : utf8)
    {
        const auto c = static_cast<unsigned char> (ch);
        if (c == '\n' || c == '\t' || (c >= 0x20 && c != 0x7f))
            out.push_back (ch);
    }

    changed = out.size() != utf8.size();
    return juce::String::fromUTF8 (out.data(), static_cast<int> (out.size()));
}

juce::String readString (const juce::var& obj, const char* key, int maxChars,
                         juce::StringArray& warnings)
{
    const auto v = obj.getProperty (juce::Identifier (key), juce::var());
    if (! v.isString())
        return {};

    bool changed = false;
    auto s = sanitize (v.toString(), changed);
    if (changed)
        warnings.add (juce::String (key) + ": control characters were removed");

    if (s.length() > maxChars)
    {
        s = s.substring (0, maxChars);
        warnings.add (juce::String (key) + ": truncated to " + juce::String (maxChars) + " characters");
    }
    return s;
}

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
        return static_cast<double> (v) != 0.0;
    return fallback;
}
}

juce::var TabPlayerProject::toVar() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("schemaVersion", currentSchemaVersion);
    obj->setProperty ("title", title);
    obj->setProperty ("sourceFileName", sourceFileName);
    obj->setProperty ("sourceText", sourceText);
    obj->setProperty ("speed", speed);
    obj->setProperty ("positionBeat", positionBeat);
    obj->setProperty ("loopEnabled", loopEnabled);
    obj->setProperty ("loopStart", loopStart);
    obj->setProperty ("loopEnd", loopEnd);
    obj->setProperty ("countInBars", countInBars);
    obj->setProperty ("metronome", metronome);
    obj->setProperty ("profile", profile.toVar());
    obj->setProperty ("embeddedInstrumentId", embeddedInstrumentId);
    obj->setProperty ("embeddedInstrumentStateBase64", embeddedInstrumentStateBase64);
    obj->setProperty ("zoom", zoom);
    return juce::var (obj);
}

juce::String TabPlayerProject::toJsonString() const
{
    return juce::JSON::toString (toVar(), false);
}

juce::var TabPlayerProject::migrate (const juce::var& input, int fromVersion)
{
    // Always work on a fresh object so the caller's var (which shares DynamicObjects) is untouched.
    auto* obj = new juce::DynamicObject();
    if (input.isObject())
        if (auto* src = input.getDynamicObject())
            for (const auto& p : src->getProperties())
                obj->setProperty (p.name, p.value);

    if (fromVersion < 1)
    {
        // v0 (legacy plugin state): {sourceText, beat, speed}. "beat" became "positionBeat".
        if (! obj->hasProperty ("positionBeat") && obj->hasProperty ("beat"))
            obj->setProperty ("positionBeat", obj->getProperty ("beat"));
        obj->removeProperty ("beat");
        obj->setProperty ("schemaVersion", 1);
    }

    // fromVersion 1 is current; newer versions are passed through and read tolerantly.
    return juce::var (obj);
}

bool TabPlayerProject::fromVar (const juce::var& v, TabPlayerProject& out,
                                juce::String& error, juce::StringArray& warnings)
{
    if (! v.isObject())
    {
        error = "Project data must be a JSON object";
        return false;
    }

    int version = 0;   // a missing schemaVersion is legacy v0
    if (v.hasProperty ("schemaVersion"))
    {
        const auto sv = v.getProperty (juce::Identifier ("schemaVersion"), juce::var());
        if (! (sv.isInt() || sv.isInt64() || sv.isDouble()))
        {
            error = "schemaVersion must be a number";
            return false;
        }

        const double d = sv;
        if (! std::isfinite (d) || d < 0.0 || d > 1.0e6)
        {
            error = "schemaVersion is out of range";
            return false;
        }
        version = static_cast<int> (d);
    }

    juce::StringArray localWarnings;
    if (version > currentSchemaVersion)
        localWarnings.add ("schemaVersion " + juce::String (version) + " is newer than supported version "
                           + juce::String (currentSchemaVersion) + "; unknown fields were ignored");

    const auto m = migrate (v, version);
    TabPlayerProject p;

    p.title            = readString (m, "title", 256, localWarnings);
    p.sourceFileName   = readString (m, "sourceFileName", 1024, localWarnings);
    p.sourceText       = readString (m, "sourceText", static_cast<int> (maxFileBytes), localWarnings);
    p.speed            = readDouble (m, "speed", 1.0, 0.05, 4.0);
    p.positionBeat     = readDouble (m, "positionBeat", 0.0, 0.0, maxBeat);
    p.loopEnabled      = readBool (m, "loopEnabled", false);
    p.loopStart        = readDouble (m, "loopStart", 0.0, 0.0, maxBeat);
    p.loopEnd          = readDouble (m, "loopEnd", 0.0, 0.0, maxBeat);
    p.countInBars      = readInt (m, "countInBars", 0, 0, 8);
    p.metronome        = readBool (m, "metronome", false);

    if (p.loopEnabled && p.loopEnd <= p.loopStart)
    {
        p.loopEnabled = false;
        localWarnings.add ("loop region is empty; loop was disabled");
    }

    if (const auto profileVar = m.getProperty (juce::Identifier ("profile"), juce::var()); profileVar.isObject())
        p.profile = InstrumentProfile::fromVar (profileVar);

    p.embeddedInstrumentId          = readString (m, "embeddedInstrumentId", 256, localWarnings);
    p.embeddedInstrumentStateBase64 = readString (m, "embeddedInstrumentStateBase64",
                                                  static_cast<int> (maxFileBytes), localWarnings);
    p.zoom                          = readDouble (m, "zoom", 1.0, 0.25, 8.0);
    p.schemaVersion                 = currentSchemaVersion;

    // Commit only after everything above succeeded.
    out = std::move (p);
    warnings.addArray (localWarnings);
    error.clear();
    return true;
}

bool TabPlayerProject::fromJsonString (const juce::String& json, TabPlayerProject& out,
                                       juce::String& error, juce::StringArray& warnings)
{
    if (static_cast<int64_t> (json.getNumBytesAsUTF8()) > maxFileBytes)
    {
        error = "Project is larger than 16 MB";
        return false;
    }

    juce::var parsed;
    const auto result = juce::JSON::parse (json, parsed);
    if (! result.wasOk())
    {
        error = "Project is not valid JSON: " + result.getErrorMessage();
        return false;
    }

    return fromVar (parsed, out, error, warnings);
}

bool TabPlayerProject::saveToFile (const juce::File& file, const TabPlayerProject& project, juce::String& error)
{
    if (file == juce::File() || file.isDirectory())
    {
        error = "Invalid project file path";
        return false;
    }

    const auto text = project.toJsonString();
    if (static_cast<int64_t> (text.getNumBytesAsUTF8()) > maxFileBytes)
    {
        error = "Project is larger than 16 MB";
        return false;
    }

    if (! file.getParentDirectory().createDirectory())
    {
        error = "Could not create folder for project: " + file.getParentDirectory().getFullPathName();
        return false;
    }

    if (! file.replaceWithText (text))
    {
        error = "Could not write project file: " + file.getFullPathName();
        return false;
    }

    error.clear();
    return true;
}

bool TabPlayerProject::loadFromFile (const juce::File& file, TabPlayerProject& out,
                                     juce::String& error, juce::StringArray& warnings)
{
    if (! file.existsAsFile())
    {
        error = "Project file not found: " + file.getFullPathName();
        return false;
    }

    if (file.getSize() > maxFileBytes)
    {
        error = "Project file is larger than 16 MB";
        return false;
    }

    return fromJsonString (file.loadFileAsString(), out, error, warnings);
}
}
