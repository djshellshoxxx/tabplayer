#pragma once

#include "../import/TabImportPipeline.h"
#include "../playback/TabPlaybackEngine.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace tabplayer
{
class TabPlayerAudioProcessor final : public juce::AudioProcessor
{
public:
    TabPlayerAudioProcessor();
    ~TabPlayerAudioProcessor() override = default;

    const juce::String getName() const override { return "TabPlayer"; }
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    bool loadTabText (const juce::String& text, juce::String* error = nullptr);
    bool loadTabFile (const juce::File& file, juce::String* error = nullptr);

    const PerformanceScore& getScore() const noexcept { return score; }
    const TabImportDiagnostics& getDiagnostics() const noexcept { return diagnostics; }
    const juce::String& getSourceText() const noexcept { return sourceText; }
    TabPlaybackEngine& getPlayback() noexcept { return playback; }

    void setSpeed (double value) noexcept { speed.store (juce::jlimit (0.1, 4.0, value)); }
    double getSpeed() const noexcept { return speed.load(); }

private:
    template <typename Sample>
    void processTyped (juce::AudioBuffer<Sample>&, juce::MidiBuffer&);

    PerformanceScore score;
    TabImportDiagnostics diagnostics;
    juce::String sourceText;
    TabPlaybackEngine playback;
    std::atomic<double> speed { 1.0 };
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TabPlayerAudioProcessor)
};
}
