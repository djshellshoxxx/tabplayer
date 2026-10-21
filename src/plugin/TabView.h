#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace tabplayer
{
/** Scrolling tab display (spec 11): staff lines, bar lines, chords, techniques,
    live playhead, click-to-seek, shift-drag loop selection and zoom. */
class TabView final : public juce::Component, private juce::Timer
{
public:
    explicit TabView (TabPlayerAudioProcessor& p) : processor (p) { startTimerHz (30); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    /** Short text glyph for a note's techniques, e.g. "h", "b", "~". */
    static juce::String techniqueGlyph (const ScoreNote& note);

    /** Beats from the start of the score to the start of a measure. */
    static double beatsBeforeMeasure (const ScoreTrack& track, int measureIndex);

private:
    void timerCallback() override { repaint(); }
    double visibleBeats() const noexcept;
    double xToBeat (float x) const noexcept;
    float beatToX (double beat) const noexcept;
    double snap (double beat) const noexcept;

    TabPlayerAudioProcessor& processor;
    double leftBeat = 0.0;
    double dragAnchor = 0.0;
    bool selectingLoop = false;
    float plotLeft = 20.0f, plotWidth = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TabView)
};
}
