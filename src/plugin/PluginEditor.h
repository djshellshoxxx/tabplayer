#pragma once

#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace tabplayer
{
class TabView final : public juce::Component, private juce::Timer
{
public:
    explicit TabView (TabPlayerAudioProcessor& p) : processor (p) { startTimerHz (30); }
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    TabPlayerAudioProcessor& processor;
};

class TabPlayerAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                            private juce::Button::Listener,
                                            private juce::Slider::Listener
{
public:
    explicit TabPlayerAudioProcessorEditor (TabPlayerAudioProcessor&);
    ~TabPlayerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void buttonClicked (juce::Button*) override;
    void sliderValueChanged (juce::Slider*) override;
    void chooseTab();

    TabPlayerAudioProcessor& processor;
    juce::TextButton loadButton { "Load Tab" };
    juce::TextButton playButton { "Play" };
    juce::TextButton stopButton { "Stop" };
    juce::Slider speedSlider;
    juce::Label statusLabel;
    TabView tabView;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TabPlayerAudioProcessorEditor)
};
}
