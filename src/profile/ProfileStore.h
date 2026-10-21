#pragma once

// File persistence for InstrumentProfile. Header-only on purpose: CMakeLists.txt compiles
// only InstrumentProfile.cpp from this directory, and the build file is outside this work package.

#include "InstrumentProfile.h"

namespace tabplayer::ProfileStore
{
inline constexpr int64_t maxProfileFileBytes = 1024 * 1024;

/** Writes the profile as JSON. On failure returns false and sets error; the target is left untouched. */
inline bool saveToFile (const juce::File& file, const InstrumentProfile& profile, juce::String& error)
{
    const auto text = juce::JSON::toString (profile.toVar(), false);
    if (text.getNumBytesAsUTF8() > maxProfileFileBytes)
    {
        error = "Instrument profile is larger than 1 MB";
        return false;
    }

    if (file == juce::File() || file.isDirectory())
    {
        error = "Invalid profile file path";
        return false;
    }

    if (! file.getParentDirectory().createDirectory())
    {
        error = "Could not create folder for profile: " + file.getParentDirectory().getFullPathName();
        return false;
    }

    if (! file.replaceWithText (text))
    {
        error = "Could not write profile file: " + file.getFullPathName();
        return false;
    }

    error.clear();
    return true;
}

/** Reads a profile. Output is only assigned on success. */
inline bool loadFromFile (const juce::File& file, InstrumentProfile& out, juce::String& error)
{
    if (! file.existsAsFile())
    {
        error = "Profile file not found: " + file.getFullPathName();
        return false;
    }

    if (file.getSize() > maxProfileFileBytes)
    {
        error = "Profile file is larger than 1 MB";
        return false;
    }

    const auto text = file.loadFileAsString();
    juce::var parsed;
    const auto result = juce::JSON::parse (text, parsed);
    if (! result.wasOk())
    {
        error = "Profile is not valid JSON: " + result.getErrorMessage();
        return false;
    }

    if (! parsed.isObject())
    {
        error = "Profile JSON must be an object";
        return false;
    }

    out = InstrumentProfile::fromVar (parsed);
    error.clear();
    return true;
}
}
