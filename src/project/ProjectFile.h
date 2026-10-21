#pragma once

#include "../profile/InstrumentProfile.h"
#include <juce_core/juce_core.h>

namespace tabplayer
{
/**
    A .tabplayer project document (ENGINEERING_SPEC sections 14 and 15).

    Load rules: unknown fields are ignored, missing fields take defaults, numeric values are
    clamped, strings are stripped of control characters (except newline and tab). A file with
    no schemaVersion is treated as legacy v0 and migrated explicitly. A schemaVersion newer than
    currentSchemaVersion loads the known fields and adds a warning.
    Loading never partially mutates the output object: it is written only on success.
*/
struct TabPlayerProject
{
    static constexpr int currentSchemaVersion = 1;
    static constexpr int64_t maxFileBytes = 16 * 1024 * 1024;

    int schemaVersion = currentSchemaVersion;
    juce::String title;
    juce::String sourceFileName;
    juce::String sourceText;
    double speed = 1.0;
    double positionBeat = 0.0;
    bool loopEnabled = false;
    double loopStart = 0.0;
    double loopEnd = 0.0;
    int countInBars = 0;
    bool metronome = false;
    InstrumentProfile profile;
    juce::String embeddedInstrumentId;
    juce::String embeddedInstrumentStateBase64;
    double zoom = 1.0;

    juce::var toVar() const;
    juce::String toJsonString() const;

    /** Explicit schema migration. Returns a new var; the input is not modified.
        fromVersion 0 (legacy plugin state {sourceText, beat, speed}) becomes version 1. */
    static juce::var migrate (const juce::var& input, int fromVersion);

    static bool fromVar (const juce::var& v, TabPlayerProject& out,
                         juce::String& error, juce::StringArray& warnings);
    static bool fromJsonString (const juce::String& json, TabPlayerProject& out,
                                juce::String& error, juce::StringArray& warnings);

    static bool saveToFile (const juce::File& file, const TabPlayerProject& project, juce::String& error);
    static bool loadFromFile (const juce::File& file, TabPlayerProject& out,
                              juce::String& error, juce::StringArray& warnings);
};
}
