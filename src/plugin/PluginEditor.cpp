#include "PluginEditor.h"

namespace tabplayer
{
namespace
{
double beatsBeforeMeasure (const ScoreTrack& track, int measureIndex)
{
    double beats = 0.0;
    for (int i = 0; i < measureIndex && i < (int) track.measures.size(); ++i)
        beats += (double) track.measures[(size_t) i].timeSignatureNumerator * 4.0
               / (double) juce::jmax (1, track.measures[(size_t) i].timeSignatureDenominator);
    return beats;
}
}

void TabView::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.fillAll (juce::Colour::fromRGB (14, 16, 22));

    const auto& score = processor.getScore();
    if (score.getNumTracks() == 0 || score.getTotalNoteCount() == 0)
    {
        g.setColour (juce::Colours::lightgrey);
        g.setFont (18.0f);
        g.drawFittedText ("Load a .tab or .txt tablature file", getLocalBounds(),
                          juce::Justification::centred, 1);
        return;
    }

    const auto& track = score.getTrack (0);
    const int strings = juce::jlimit (1, kMaxStrings, track.numStrings);
    const double totalBeats = juce::jmax (1.0, processor.getPlayback().getTotalBeats());
    const double currentBeat = processor.getPlayback().getBeat();
    const double visibleBeats = juce::jlimit (4.0, 32.0, totalBeats);
    const double leftBeat = juce::jlimit (0.0, juce::jmax (0.0, totalBeats - visibleBeats),
                                          currentBeat - visibleBeats * 0.25);

    const float header = 28.0f;
    const float lineGap = (area.getHeight() - header - 8.0f) / (float) strings;
    const float left = 20.0f;
    const float right = area.getRight() - 20.0f;
    const float width = juce::jmax (1.0f, right - left);

    g.setColour (juce::Colours::white.withAlpha (0.72f));
    g.setFont (13.0f);
    g.drawText ("TAB  •  " + juce::String (currentBeat, 2) + " / "
                + juce::String (totalBeats, 2) + " beats",
                12, 4, getWidth() - 24, 20, juce::Justification::centredLeft);

    for (int s = 0; s < strings; ++s)
    {
        const float y = header + lineGap * ((float) s + 0.5f);
        g.setColour (juce::Colours::grey.withAlpha (0.65f));
        g.drawHorizontalLine ((int) y, left, right);
    }

    for (int m = 0; m < (int) track.measures.size(); ++m)
    {
        const double base = beatsBeforeMeasure (track, m);
        for (const auto& voice : track.measures[(size_t) m].voices)
        {
            for (const auto& note : voice.notes)
            {
                const double beat = base + note.startBeat;
                if (beat < leftBeat || beat > leftBeat + visibleBeats)
                    continue;

                const float x = left + (float) ((beat - leftBeat) / visibleBeats) * width;
                const float y = header + lineGap * ((float) note.stringIndex + 0.5f);
                const bool active = currentBeat >= beat
                                 && currentBeat < beat + note.durationBeats;

                g.setColour (active ? juce::Colours::cyan : juce::Colours::white);
                g.setFont (active ? 17.0f : 14.0f);
                g.fillRoundedRectangle (x - 9.0f, y - 10.0f, 18.0f, 20.0f, 5.0f);
                g.setColour (juce::Colours::black);
                g.drawFittedText (juce::String (note.fret),
                                  juce::Rectangle<int> ((int) x - 9, (int) y - 10, 18, 20),
                                  juce::Justification::centred, 1);
            }
        }
    }

    const float playX = left + (float) ((currentBeat - leftBeat) / visibleBeats) * width;
    if (playX >= left && playX <= right)
    {
        g.setColour (juce::Colours::orange.withAlpha (0.9f));
        g.drawVerticalLine ((int) playX, header, area.getBottom() - 4.0f);
    }
}

TabPlayerAudioProcessorEditor::TabPlayerAudioProcessorEditor (TabPlayerAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p), tabView (p)
{
    for (auto* button : { &loadButton, &playButton, &stopButton })
    {
        addAndMakeVisible (*button);
        button->addListener (this);
    }

    speedSlider.setRange (0.25, 2.0, 0.01);
    speedSlider.setValue (processor.getSpeed(), juce::dontSendNotification);
    speedSlider.setTextValueSuffix ("x");
    speedSlider.addListener (this);
    addAndMakeVisible (speedSlider);

    statusLabel.setText ("No tab loaded", juce::dontSendNotification);
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);
    addAndMakeVisible (tabView);

    setResizable (true, true);
    setResizeLimits (700, 420, 1600, 1000);
    setSize (900, 560);
}

TabPlayerAudioProcessorEditor::~TabPlayerAudioProcessorEditor()
{
    loadButton.removeListener (this);
    playButton.removeListener (this);
    stopButton.removeListener (this);
    speedSlider.removeListener (this);
}

void TabPlayerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (23, 26, 34));
}

void TabPlayerAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto top = r.removeFromTop (38);
    loadButton.setBounds (top.removeFromLeft (100));
    top.removeFromLeft (6);
    playButton.setBounds (top.removeFromLeft (70));
    top.removeFromLeft (6);
    stopButton.setBounds (top.removeFromLeft (70));
    top.removeFromLeft (10);
    speedSlider.setBounds (top.removeFromLeft (150));
    top.removeFromLeft (10);
    statusLabel.setBounds (top);

    r.removeFromTop (8);
    tabView.setBounds (r);
}

void TabPlayerAudioProcessorEditor::buttonClicked (juce::Button* button)
{
    if (button == &loadButton)
        chooseTab();
    else if (button == &playButton)
        processor.getPlayback().play();
    else if (button == &stopButton)
    {
        processor.getPlayback().stop();
        processor.getPlayback().rewind();
    }
}

void TabPlayerAudioProcessorEditor::sliderValueChanged (juce::Slider* slider)
{
    if (slider == &speedSlider)
        processor.setSpeed (speedSlider.getValue());
}

void TabPlayerAudioProcessorEditor::chooseTab()
{
    chooser = std::make_unique<juce::FileChooser> ("Load tablature", juce::File {}, "*.tab;*.txt");
    const auto flags = juce::FileBrowserComponent::openMode
                     | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (flags, [safe = juce::Component::SafePointer<TabPlayerAudioProcessorEditor> (this)]
    (const juce::FileChooser& fc)
    {
        if (safe == nullptr)
            return;

        const auto file = fc.getResult();
        if (file == juce::File {})
            return;

        juce::String error;
        if (safe->processor.loadTabFile (file, &error))
        {
            const auto& d = safe->processor.getDiagnostics();
            safe->statusLabel.setText (file.getFileName() + "  •  "
                                       + juce::String (d.notes) + " notes  •  "
                                       + juce::String (d.measures) + " bars",
                                       juce::dontSendNotification);
        }
        else
        {
            safe->statusLabel.setText ("Load failed: " + error, juce::dontSendNotification);
        }
    });
}
}
