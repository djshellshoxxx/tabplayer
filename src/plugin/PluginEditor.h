#pragma once

#include "PluginProcessor.h"
#include "TabView.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace tabplayer
{
class TabPlayerAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                            private juce::Button::Listener,
                                            private juce::Slider::Listener,
                                            private juce::ComboBox::Listener,
                                            private juce::Timer
{
public:
    explicit TabPlayerAudioProcessorEditor (TabPlayerAudioProcessor&);
    ~TabPlayerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void buttonClicked (juce::Button*) override;
    void sliderValueChanged (juce::Slider*) override;
    void comboBoxChanged (juce::ComboBox*) override;
    void timerCallback() override;

    void chooseTab();
    void chooseProject (bool save);
    void chooseMidiExport();
    void setStatus (const juce::String& text);
    void refreshStatusAfterLoad (const juce::String& name);

    TabPlayerAudioProcessor& processor;
    juce::TextButton loadButton { "Load Tab" }, openProjectButton { "Open" }, saveProjectButton { "Save" },
                     exportButton { "Export MIDI" }, playButton { "Play" }, stopButton { "Stop" },
                     transposeDown { "-" }, transposeUp { "+" };
    juce::ToggleButton loopToggle { "Loop" }, metronomeToggle { "Metronome" }, followToggle { "Follow host" };
    juce::ComboBox countInBox, profileBox;
    juce::Label speedLabel { {}, "Speed" }, zoomLabel { {}, "Zoom" }, transposeLabel { {}, "Transpose" },
                statusLabel, inspectorLabel;
    juce::Slider speedSlider, zoomSlider;
    TabView tabView;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TabPlayerAudioProcessorEditor)
};
}
