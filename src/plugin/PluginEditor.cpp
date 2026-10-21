#include "PluginEditor.h"

namespace tabplayer
{
TabPlayerAudioProcessorEditor::TabPlayerAudioProcessorEditor (TabPlayerAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p), tabView (p)
{
    for (auto* button : { &loadButton, &openProjectButton, &saveProjectButton, &exportButton,
                          &playButton, &stopButton, &transposeDown, &transposeUp })
    {
        addAndMakeVisible (*button);
        button->addListener (this);
    }

    for (auto* toggle : { &loopToggle, &metronomeToggle, &followToggle })
    {
        addAndMakeVisible (*toggle);
        toggle->addListener (this);
    }
    loopToggle.setToggleState (processor.isLoopEnabled(), juce::dontSendNotification);
    metronomeToggle.setToggleState (processor.isMetronomeEnabled(), juce::dontSendNotification);
    followToggle.setToggleState (processor.isFollowingHost(), juce::dontSendNotification);
    loopToggle.setTooltip ("Shift-drag on the tab to select a loop; double-click to clear");

    countInBox.addItem ("No count-in", 1);
    for (int bars = 1; bars <= 4; ++bars)
        countInBox.addItem (juce::String (bars) + (bars == 1 ? " bar count-in" : " bars count-in"), bars + 1);
    countInBox.setSelectedId (processor.getCountInBars() + 1, juce::dontSendNotification);
    countInBox.addListener (this);
    addAndMakeVisible (countInBox);

    const auto& profiles = processor.getProfiles();
    for (size_t i = 0; i < profiles.size(); ++i)
        profileBox.addItem (profiles[i].name, (int) i + 1);
    profileBox.setSelectedId (processor.getProfileIndex() + 1, juce::dontSendNotification);
    profileBox.addListener (this);
    addAndMakeVisible (profileBox);

    speedSlider.setRange (0.25, 2.0, 0.01);
    speedSlider.setValue (processor.getSpeed(), juce::dontSendNotification);
    speedSlider.setTextValueSuffix ("x");
    speedSlider.addListener (this);
    zoomSlider.setRange (0.25, 4.0, 0.01);
    zoomSlider.setSkewFactorFromMidPoint (1.0);
    zoomSlider.setValue (processor.getZoom(), juce::dontSendNotification);
    zoomSlider.addListener (this);
    addAndMakeVisible (speedSlider);
    addAndMakeVisible (zoomSlider);

    for (auto* label : { &speedLabel, &zoomLabel, &transposeLabel })
    {
        label->setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (*label);
    }

    statusLabel.setText ("No tab loaded", juce::dontSendNotification);
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    inspectorLabel.setJustificationType (juce::Justification::centredLeft);
    inspectorLabel.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, 0)));
    addAndMakeVisible (statusLabel);
    addAndMakeVisible (inspectorLabel);
    addAndMakeVisible (tabView);

    setResizable (true, true);
    setResizeLimits (860, 480, 1800, 1000);
    setSize (1000, 600);
    startTimerHz (15);
}

TabPlayerAudioProcessorEditor::~TabPlayerAudioProcessorEditor()
{
    stopTimer();
}

void TabPlayerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (23, 26, 34));
}

void TabPlayerAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto row1 = r.removeFromTop (30);
    for (auto* b : { &loadButton, &openProjectButton, &saveProjectButton, &exportButton })
    {
        b->setBounds (row1.removeFromLeft (b == &exportButton ? 100 : 80));
        row1.removeFromLeft (6);
    }
    row1.removeFromLeft (14);
    playButton.setBounds (row1.removeFromLeft (70));
    row1.removeFromLeft (6);
    stopButton.setBounds (row1.removeFromLeft (70));
    row1.removeFromLeft (14);
    statusLabel.setBounds (row1);

    r.removeFromTop (6);
    auto row2 = r.removeFromTop (28);
    speedLabel.setBounds (row2.removeFromLeft (46));
    speedSlider.setBounds (row2.removeFromLeft (130));
    row2.removeFromLeft (8);
    loopToggle.setBounds (row2.removeFromLeft (60));
    countInBox.setBounds (row2.removeFromLeft (130));
    row2.removeFromLeft (8);
    metronomeToggle.setBounds (row2.removeFromLeft (100));
    followToggle.setBounds (row2.removeFromLeft (100));
    transposeLabel.setBounds (row2.removeFromLeft (70));
    transposeDown.setBounds (row2.removeFromLeft (28));
    transposeUp.setBounds (row2.removeFromLeft (28));
    row2.removeFromLeft (8);
    profileBox.setBounds (row2.removeFromLeft (170));
    zoomLabel.setBounds (row2.removeFromLeft (46));
    zoomSlider.setBounds (row2);

    r.removeFromTop (6);
    inspectorLabel.setBounds (r.removeFromBottom (24));
    r.removeFromBottom (4);
    tabView.setBounds (r);
}

void TabPlayerAudioProcessorEditor::setStatus (const juce::String& text)
{
    statusLabel.setText (text, juce::dontSendNotification);
}

void TabPlayerAudioProcessorEditor::refreshStatusAfterLoad (const juce::String& name)
{
    const auto& d = processor.getDiagnostics();
    setStatus (name + "  |  " + juce::String (d.notes) + " notes  |  " + juce::String (d.measures) + " bars");
    loopToggle.setToggleState (processor.isLoopEnabled(), juce::dontSendNotification);
    metronomeToggle.setToggleState (processor.isMetronomeEnabled(), juce::dontSendNotification);
    countInBox.setSelectedId (processor.getCountInBars() + 1, juce::dontSendNotification);
    profileBox.setSelectedId (processor.getProfileIndex() + 1, juce::dontSendNotification);
    speedSlider.setValue (processor.getSpeed(), juce::dontSendNotification);
    zoomSlider.setValue (processor.getZoom(), juce::dontSendNotification);
}

void TabPlayerAudioProcessorEditor::timerCallback()
{
    auto& playback = processor.getPlayback();
    playButton.setButtonText (playback.isPlaying() ? "Pause" : "Play");

    const auto m = playback.getLastMessage();
    juce::String text = "Beat " + juce::String (playback.getBeat(), 2) + "  |  ";
    if (m.note >= 0)
        text << "Last: " << PerformanceScore::getNoteName (m.note) << " (" << m.note << ")  str "
             << (m.string + 1) << " fret " << m.fret << "  vel " << m.velocity << "  ch " << m.channel
             << "  bend " << juce::String (m.bendSemitones, 2) << " st";
    else
        text << "No MIDI sent yet";
    text << "  |  Sounding: " << (int) playback.getActiveNotes().size();
    inspectorLabel.setText (text, juce::dontSendNotification);
}

void TabPlayerAudioProcessorEditor::buttonClicked (juce::Button* button)
{
    auto& playback = processor.getPlayback();

    if (button == &loadButton)
        chooseTab();
    else if (button == &openProjectButton)
        chooseProject (false);
    else if (button == &saveProjectButton)
        chooseProject (true);
    else if (button == &exportButton)
        chooseMidiExport();
    else if (button == &playButton)
    {
        if (playback.isPlaying())
            playback.stop();
        else
            playback.play();
    }
    else if (button == &stopButton)
    {
        playback.stop();
        playback.rewind();
    }
    else if (button == &loopToggle)
    {
        // With no selection yet, loop the whole piece.
        const double end = processor.getLoopEnd() > processor.getLoopStart()
                         ? processor.getLoopEnd() : playback.getTotalBeats();
        processor.setLoop (processor.getLoopStart(), end, loopToggle.getToggleState());
    }
    else if (button == &metronomeToggle)
        processor.setMetronome (metronomeToggle.getToggleState());
    else if (button == &followToggle)
        processor.setFollowHost (followToggle.getToggleState());
    else if (button == &transposeDown)
        processor.transposeScore (-1);
    else if (button == &transposeUp)
        processor.transposeScore (1);
}

void TabPlayerAudioProcessorEditor::sliderValueChanged (juce::Slider* slider)
{
    if (slider == &speedSlider)
        processor.setSpeed (speedSlider.getValue());
    else if (slider == &zoomSlider)
        processor.setZoom (zoomSlider.getValue());
}

void TabPlayerAudioProcessorEditor::comboBoxChanged (juce::ComboBox* box)
{
    if (box == &countInBox)
        processor.setCountInBars (countInBox.getSelectedId() - 1);
    else if (box == &profileBox)
        processor.selectProfile (profileBox.getSelectedId() - 1);
}

void TabPlayerAudioProcessorEditor::chooseTab()
{
    chooser = std::make_unique<juce::FileChooser> ("Load tablature", juce::File {}, "*.tab;*.txt");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<TabPlayerAudioProcessorEditor> (this)] (const juce::FileChooser& fc)
        {
            if (safe == nullptr || fc.getResult() == juce::File {})
                return;

            juce::String error;
            if (safe->processor.loadTabFile (fc.getResult(), &error))
                safe->refreshStatusAfterLoad (fc.getResult().getFileName());
            else
                safe->setStatus ("Load failed (previous tab kept): " + error);
        });
}

void TabPlayerAudioProcessorEditor::chooseProject (bool save)
{
    chooser = std::make_unique<juce::FileChooser> (save ? "Save project" : "Open project",
                                                   juce::File {}, "*.tabplayer");
    const auto flags = save ? (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting)
                            : (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles);
    chooser->launchAsync (flags,
        [safe = juce::Component::SafePointer<TabPlayerAudioProcessorEditor> (this), save] (const juce::FileChooser& fc)
        {
            if (safe == nullptr || fc.getResult() == juce::File {})
                return;

            auto file = fc.getResult();
            juce::String error;
            if (save)
            {
                if (file.getFileExtension().isEmpty())
                    file = file.withFileExtension ("tabplayer");
                safe->setStatus (safe->processor.saveProjectFile (file, &error)
                                     ? "Saved " + file.getFileName() : "Save failed: " + error);
            }
            else if (safe->processor.loadProjectFile (file, &error))
                safe->refreshStatusAfterLoad (file.getFileName());
            else
                safe->setStatus ("Open failed (previous project kept): " + error);
        });
}

void TabPlayerAudioProcessorEditor::chooseMidiExport()
{
    if (processor.getScore().getTotalNoteCount() == 0)
    {
        setStatus ("Nothing to export: load a tab first");
        return;
    }

    chooser = std::make_unique<juce::FileChooser> ("Export MIDI", juce::File {}, "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe = juce::Component::SafePointer<TabPlayerAudioProcessorEditor> (this)] (const juce::FileChooser& fc)
        {
            if (safe == nullptr || fc.getResult() == juce::File {})
                return;

            auto file = fc.getResult();
            if (file.getFileExtension().isEmpty())
                file = file.withFileExtension ("mid");
            juce::String error;
            safe->setStatus (safe->processor.exportMidiFile (file, &error)
                                 ? "Exported " + file.getFileName() : "Export failed: " + error);
        });
}
}
