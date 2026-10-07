#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    if (auto position = getPlayHead() != nullptr ? getPlayHead()->getPosition() : juce::Optional<juce::AudioPlayHead::PositionInfo>{})
        if (auto hostBpm = position->getBpm())
            bpm = *hostBpm;

    playback.process (midi, audio.getNumSamples(), currentSampleRate, bpm, speed.load());
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
    return loadTabText (file.loadFileAsString(), error);
}

void TabPlayerAudioProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty ("sourceText", sourceText);
    root->setProperty ("beat", playback.getBeat());
    root->setProperty ("speed", speed.load());

    const auto json = juce::JSON::toString (juce::var (root.get()));
    destination.append (json.toRawUTF8(), (size_t) json.getNumBytesAsUTF8());
}

void TabPlayerAudioProcessor::setStateInformation (const void* data, int size)
{
    const auto json = juce::String::fromUTF8 (static_cast<const char*> (data), size);
    const auto parsed = juce::JSON::parse (json);
    if (auto* object = parsed.getDynamicObject())
    {
        setSpeed ((double) object->getProperty ("speed"));
        juce::String error;
        if (loadTabText (object->getProperty ("sourceText").toString(), &error))
            playback.seekBeat ((double) object->getProperty ("beat"));
    }
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
