#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace tabplayer
{
/** How tab strings map onto MIDI channels. */
enum class ChannelMode { single = 0, perString, mpeLowerZone };

/** Instrument-neutral technique intents the playback compiler can emit. */
enum class Articulation
{
    palmMute = 0, deadNote, harmonic, tap, slap, pop, legato, staccato, accent, ghost, letRing,
    numArticulations
};

/** One articulation -> keyswitch note and/or CC mapping. -1 disables. */
struct ArticulationMapping
{
    Articulation articulation = Articulation::palmMute;
    int keyswitchNote = -1;  ///< sent as a short note on keyswitchChannel before the note
    int ccNumber = -1;       ///< CC set to ccValue for the note, reset to ccOffValue after
    int ccValue = 127;
    int ccOffValue = 0;
};

/** A user-editable, tab-independent description of how to drive an instrument. */
struct InstrumentProfile
{
    juce::String name { "Generic MIDI" };
    ChannelMode channelMode = ChannelMode::perString;
    int singleChannel = 1;             ///< used by ChannelMode::single
    double pitchBendRangeSemitones = 2.0;  ///< 48 for MPE
    int keyswitchChannel = 1;

    std::vector<ArticulationMapping> mappings;

    double palmMuteDurationScale = 0.5;   ///< note length multiplier
    double palmMuteVelocityScale = 0.8;
    double staccatoDurationScale = 0.4;
    double legatoOverlapBeats = 0.02;     ///< hammer-on/pull-off/legato slide overlap
    double ghostVelocityScale = 0.5;
    double accentVelocityScale = 1.25;
    bool sendVibrato = true;              ///< vibrato as pitch-bend LFO
    bool sendSlides = true;               ///< slides as pitch-bend ramps
    bool sendBends = true;

    int programChange = -1;               ///< sent on start/seek when >= 0

    const ArticulationMapping* find (Articulation a) const noexcept;

    juce::var toVar() const;
    static InstrumentProfile fromVar (const juce::var& v);   ///< tolerant; unknown fields ignored

    static std::vector<InstrumentProfile> factoryProfiles();
    static InstrumentProfile genericMidi();
};
}
