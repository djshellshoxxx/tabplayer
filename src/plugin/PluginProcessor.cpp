#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../tools/MidiExporter.h"
#include "../tools/ScoreTransforms.h"

namespace tabplayer
{
TabPlayerAudioProcessor::TabPlayerAudioProcessor()
    : juce::AudioProcessor (BusesProperties())
{
}

void TabPlayerAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
}

template <typename Sample>
void TabPlayerAudioProcessor::processTyped (juce::AudioBuffer<Sample>& audio, juce::MidiBuffer& midi)
{
    audio.clear();

    double bpm = score.getMeta().tempoBpm;
    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
            if (const auto hostBpm = position->getBpm())
                bpm = *hostBpm;
    }

    double hostPpq = -1.0;
    if (followHost.load())
    {
        bool hostPlaying = false;
        if (auto* playHead = getPlayHead())
            if (const auto position = playHead->getPosition())
            {
                hostPlaying = position->getIsPlaying();
                if (const auto ppq = position->getPpqPosition())
                    hostPpq = *ppq;
            }

        if (hostPlaying && ! wasHostPlaying)
            playback.play();
        else if (! hostPlaying && wasHostPlaying)
            playback.stop();
        wasHostPlaying = hostPlaying;
        if (! hostPlaying)
            hostPpq = -1.0;
    }

    playback.process (midi, audio.getNumSamples(), currentSampleRate, bpm, speed.load(), hostPpq);
}

void TabPlayerAudioProcessor::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    processTyped (audio, midi);
}

void TabPlayerAudioProcessor::processBlock (juce::AudioBuffer<double>& audio, juce::MidiBuffer& midi)
{
    processTyped (audio, midi);
}

bool TabPlayerAudioProcessor::loadTabText (const juce::String& text, juce::String* error)
{
    PerformanceScore imported;
    TabImportPipeline pipeline;
    TabImportDiagnostics importedDiagnostics;

    if (! pipeline.read (text, imported, &importedDiagnostics))
    {
        if (error != nullptr)
            *error = pipeline.getLastError();
        return false;
    }

    score = std::move (imported);
    diagnostics = std::move (importedDiagnostics);
    sourceText = text;
    playback.setScore (score);
    playback.setProfile (profiles[(size_t) profileIndex]);

    if (error != nullptr)
        error->clear();
    return true;
}

bool TabPlayerAudioProcessor::loadTabFile (const juce::File& file, juce::String* error)
{
    if (! file.existsAsFile())
    {
        if (error != nullptr)
            *error = "File does not exist.";
        return false;
    }
    sourceFileName = file.getFileName();
    return loadTabText (file.loadFileAsString(), error);
}

TabPlayerProject TabPlayerAudioProcessor::makeProject() const
{
    TabPlayerProject project;
    project.title = score.getMeta().title;
    project.sourceFileName = sourceFileName;
    project.sourceText = sourceText;
    project.speed = speed.load();
    project.positionBeat = playback.getBeat();
    project.loopEnabled = loopEnabled;
    project.loopStart = loopStart;
    project.loopEnd = loopEnd;
    project.countInBars = countInBars;
    project.metronome = metronome;
    project.profile = playback.getProfile();
    project.zoom = zoom;
    return project;
}

void TabPlayerAudioProcessor::setProfileFromProject (const InstrumentProfile& profile)
{
    profileIndex = 0;
    for (size_t i = 0; i < profiles.size(); ++i)
        if (profiles[i].name == profile.name)
            profileIndex = (int) i;
    playback.setProfile (profile);
}

bool TabPlayerAudioProcessor::applyProject (const TabPlayerProject& project, juce::String* error)
{
    if (project.sourceText.isNotEmpty())
    {
        if (! loadTabText (project.sourceText, error))
            return false;
        sourceFileName = project.sourceFileName;
    }

    setSpeed (project.speed);
    setProfileFromProject (project.profile);
    setCountInBars (project.countInBars);
    setMetronome (project.metronome);
    setLoop (project.loopStart, project.loopEnd, project.loopEnabled);
    setZoom (project.zoom);
    playback.seekBeat (project.positionBeat);
    return true;
}

bool TabPlayerAudioProcessor::saveProjectFile (const juce::File& file, juce::String* error) const
{
    juce::String message;
    const bool ok = TabPlayerProject::saveToFile (file, makeProject(), message);
    if (error != nullptr)
        *error = message;
    return ok;
}

bool TabPlayerAudioProcessor::loadProjectFile (const juce::File& file, juce::String* error)
{
    TabPlayerProject project;
    juce::String message;
    juce::StringArray warnings;
    if (! TabPlayerProject::loadFromFile (file, project, message, warnings))
    {
        if (error != nullptr)
            *error = message;
        return false;
    }
    return applyProject (project, error);
}

bool TabPlayerAudioProcessor::exportMidiFile (const juce::File& file, juce::String* error) const
{
    juce::String message;
    const bool ok = tabplayer::exportMidiFile (score, file, message);
    if (error != nullptr)
        *error = message;
    return ok;
}

void TabPlayerAudioProcessor::setLoop (double startBeat, double endBeat, bool enabled)
{
    loopStart = startBeat;
    loopEnd = endBeat;
    loopEnabled = enabled && endBeat > startBeat;
    playback.setLoop (loopStart, loopEnd, loopEnabled);
}

void TabPlayerAudioProcessor::setCountInBars (int bars)
{
    countInBars = juce::jlimit (0, 4, bars);
    playback.setCountInBars (countInBars);
}

void TabPlayerAudioProcessor::setMetronome (bool enabled)
{
    metronome = enabled;
    playback.setMetronomeEnabled (enabled);
}

void TabPlayerAudioProcessor::selectProfile (int index)
{
    if (index < 0 || index >= (int) profiles.size())
        return;
    profileIndex = index;
    playback.setProfile (profiles[(size_t) index]);
}

void TabPlayerAudioProcessor::transposeScore (int semitones)
{
    if (semitones == 0 || score.getNumTracks() == 0)
        return;
    score = tabplayer::transpose (score, semitones);
    playback.setScore (score);
}

void TabPlayerAudioProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    const auto json = makeProject().toJsonString();
    destination.append (json.toRawUTF8(), (size_t) json.getNumBytesAsUTF8());
}

void TabPlayerAudioProcessor::setStateInformation (const void* data, int size)
{
    // Accepts both the current project JSON and the legacy bootstrap state (migrated as v0).
    TabPlayerProject project;
    juce::String error;
    juce::StringArray warnings;
    if (TabPlayerProject::fromJsonString (juce::String::fromUTF8 (static_cast<const char*> (data), size),
                                          project, error, warnings))
        applyProject (project, &error);
}

juce::AudioProcessorEditor* TabPlayerAudioProcessor::createEditor()
{
    return new TabPlayerAudioProcessorEditor (*this);
}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new tabplayer::TabPlayerAudioProcessor();
}
