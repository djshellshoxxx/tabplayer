#include "TabView.h"

namespace tabplayer
{
double TabView::beatsBeforeMeasure (const ScoreTrack& track, int measureIndex)
{
    double beats = 0.0;
    for (int i = 0; i < measureIndex && i < (int) track.measures.size(); ++i)
        beats += (double) track.measures[(size_t) i].timeSignatureNumerator * 4.0
               / (double) juce::jmax (1, track.measures[(size_t) i].timeSignatureDenominator);
    return beats;
}

juce::String TabView::techniqueGlyph (const ScoreNote& note)
{
    using T = ScoreTechnique::Type;
    if (note.hasTechnique (T::deadNote))     return "x";
    if (note.hasTechnique (T::hammerOn))     return "h";
    if (note.hasTechnique (T::pullOff))      return "p";
    if (note.hasTechnique (T::slideUp) || note.hasTechnique (T::slideLegato)) return "/";
    if (note.hasTechnique (T::slideDown))    return "\\";
    if (note.hasTechnique (T::bend) || note.hasTechnique (T::preBend)) return "b";
    if (note.hasTechnique (T::vibrato))      return "~";
    if (note.hasTechnique (T::palmMute))     return "PM";
    if (note.hasTechnique (T::tap))          return "T";
    if (note.hasTechnique (T::naturalHarmonic) || note.hasTechnique (T::artificialHarmonic)
        || note.hasTechnique (T::pinchHarmonic) || note.hasTechnique (T::tapHarmonic)) return "<>";
    if (note.hasTechnique (T::slap))         return "S";
    if (note.hasTechnique (T::pop))          return "P";
    if (note.hasTechnique (T::accent))       return ">";
    return {};
}

double TabView::visibleBeats() const noexcept
{
    return juce::jlimit (2.0, 256.0, 16.0 / processor.getZoom());
}

float TabView::beatToX (double beat) const noexcept
{
    return plotLeft + (float) ((beat - leftBeat) / visibleBeats()) * plotWidth;
}

double TabView::xToBeat (float x) const noexcept
{
    return leftBeat + (double) ((x - plotLeft) / plotWidth) * visibleBeats();
}

double TabView::snap (double beat) const noexcept
{
    return juce::jlimit (0.0, processor.getPlayback().getTotalBeats(), std::round (beat * 4.0) / 4.0);
}

void TabView::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    g.fillAll (juce::Colour::fromRGB (14, 16, 22));

    const auto& score = processor.getScore();
    if (score.getNumTracks() == 0 || score.getTotalNoteCount() == 0)
    {
        g.setColour (juce::Colours::lightgrey);
        g.setFont (18.0f);
        g.drawFittedText ("Load a .tab or .txt tablature file, or a .tabplayer project",
                          getLocalBounds(), juce::Justification::centred, 1);
        return;
    }

    const auto& track = score.getTrack (0);
    const int strings = juce::jlimit (1, kMaxStrings, track.numStrings);
    auto& playback = processor.getPlayback();
    const double totalBeats = juce::jmax (1.0, playback.getTotalBeats());
    const double currentBeat = playback.getBeat();
    const double visible = visibleBeats();

    // Follow the playhead while playing; otherwise keep the manual scroll position.
    if (playback.isPlaying() && (currentBeat < leftBeat || currentBeat > leftBeat + visible * 0.75))
        leftBeat = currentBeat - visible * 0.1;
    leftBeat = juce::jlimit (0.0, juce::jmax (0.0, totalBeats - visible), leftBeat);

    const float header = 52.0f;
    const float gutter = 34.0f;
    plotLeft = gutter + 10.0f;
    plotWidth = juce::jmax (1.0f, area.getRight() - 16.0f - plotLeft);
    const float lineGap = (area.getHeight() - header - 8.0f) / (float) strings;
    const float right = plotLeft + plotWidth;

    // Header: tuning and capo.
    juce::String tuning;
    for (int s = 0; s < strings; ++s)
        tuning << PerformanceScore::getNoteName (track.tuning[(size_t) s]) << (s + 1 < strings ? " " : "");
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.setFont (13.0f);
    g.drawText (score.getMeta().title + "   Tuning: " + tuning
                + (track.capoFret > 0 ? "   Capo " + juce::String (track.capoFret) : juce::String())
                + "   " + juce::String (currentBeat, 2) + " / " + juce::String (totalBeats, 2) + " beats",
                10, 4, getWidth() - 20, 18, juce::Justification::centredLeft);

    // Loop region.
    if (processor.isLoopEnabled())
    {
        const float x1 = juce::jmax (plotLeft, beatToX (processor.getLoopStart()));
        const float x2 = juce::jmin (right, beatToX (processor.getLoopEnd()));
        if (x2 > x1)
        {
            g.setColour (juce::Colours::yellow.withAlpha (0.12f));
            g.fillRect (x1, header, x2 - x1, area.getHeight() - header);
            g.setColour (juce::Colours::yellow.withAlpha (0.8f));
            g.fillRect (x1, header - 4.0f, x2 - x1, 3.0f);
        }
    }

    // String lines and labels.
    g.setFont (12.0f);
    for (int s = 0; s < strings; ++s)
    {
        const float y = header + lineGap * ((float) s + 0.5f);
        g.setColour (juce::Colours::grey.withAlpha (0.65f));
        g.drawHorizontalLine ((int) y, plotLeft, right);
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.drawText (PerformanceScore::getNoteName (track.tuning[(size_t) s]), 4, (int) y - 8,
                    (int) gutter, 16, juce::Justification::centredRight);
    }

    // Measures, bar lines, sections, chords, notes.
    for (int m = 0; m < (int) track.measures.size(); ++m)
    {
        const auto& measure = track.measures[(size_t) m];
        const double base = beatsBeforeMeasure (track, m);
        const double len = (double) measure.timeSignatureNumerator * 4.0
                         / (double) juce::jmax (1, measure.timeSignatureDenominator);
        if (base + len < leftBeat || base > leftBeat + visible)
            continue;

        const float bx = beatToX (base);
        if (bx >= plotLeft && bx <= right)
        {
            g.setColour (juce::Colours::white.withAlpha (0.45f));
            g.drawVerticalLine ((int) bx, header + lineGap * 0.5f, header + lineGap * ((float) strings - 0.5f));
            g.setFont (11.0f);
            g.drawText (juce::String (m + 1), (int) bx + 2, (int) header - 16, 30, 12, juce::Justification::left);
            if (measure.sectionName.isNotEmpty())
            {
                g.setColour (juce::Colours::lightgreen);
                g.drawText (measure.sectionName, (int) bx + 24, (int) header - 28, 140, 12, juce::Justification::left);
            }
        }

        for (const auto& chord : measure.chordSymbols)
        {
            const float cx = beatToX (base + chord.first);
            if (cx >= plotLeft && cx <= right)
            {
                g.setColour (juce::Colours::orange);
                g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
                g.drawText (chord.second, (int) cx, (int) header - 16, 80, 14, juce::Justification::left);
            }
        }

        for (const auto& voice : measure.voices)
            for (const auto& note : voice.notes)
            {
                const double beat = base + note.startBeat;
                if (beat < leftBeat || beat > leftBeat + visible)
                    continue;

                const float x = beatToX (beat);
                const float y = header + lineGap * ((float) note.stringIndex + 0.5f);
                const bool active = currentBeat >= beat && currentBeat < beat + note.durationBeats;

                // Active notes get colour AND a heavier outline and larger box (not colour alone).
                g.setColour (active ? juce::Colours::cyan : juce::Colour::fromRGB (230, 230, 235));
                const float w = active ? 24.0f : 18.0f;
                g.fillRoundedRectangle (x - w * 0.5f, y - 10.0f, w, 20.0f, 5.0f);
                if (active)
                {
                    g.setColour (juce::Colours::white);
                    g.drawRoundedRectangle (x - w * 0.5f, y - 10.0f, w, 20.0f, 5.0f, 2.5f);
                }
                g.setColour (juce::Colours::black);
                g.setFont (active ? 16.0f : 14.0f);
                g.drawFittedText (note.hasTechnique (ScoreTechnique::Type::ghostNote)
                                      ? "(" + juce::String (note.fret) + ")" : juce::String (note.fret),
                                  juce::Rectangle<int> ((int) (x - w * 0.5f), (int) y - 10, (int) w, 20),
                                  juce::Justification::centred, 1);

                const auto glyph = techniqueGlyph (note);
                if (glyph.isNotEmpty())
                {
                    g.setColour (juce::Colours::lightsalmon);
                    g.setFont (11.0f);
                    g.drawText (glyph, (int) x - 14, (int) y - 24, 28, 12, juce::Justification::centred);
                }
            }
    }

    const float playX = beatToX (currentBeat);
    if (playX >= plotLeft && playX <= right)
    {
        g.setColour (juce::Colours::orange.withAlpha (0.95f));
        g.fillRect (playX - 1.0f, header, 2.0f, area.getBottom() - header - 4.0f);
        juce::Path tri;
        tri.addTriangle (playX - 6.0f, header - 6.0f, playX + 6.0f, header - 6.0f, playX, header + 2.0f);
        g.fillPath (tri);
    }
}

void TabView::mouseDown (const juce::MouseEvent& e)
{
    selectingLoop = e.mods.isShiftDown();
    dragAnchor = snap (xToBeat ((float) e.x));
    if (! selectingLoop)
        processor.getPlayback().seekBeat (dragAnchor);
}

void TabView::mouseDrag (const juce::MouseEvent& e)
{
    const double beat = snap (xToBeat ((float) e.x));
    if (selectingLoop)
        processor.setLoop (juce::jmin (dragAnchor, beat), juce::jmax (dragAnchor, beat), true);
    else
        processor.getPlayback().seekBeat (beat);
}

void TabView::mouseUp (const juce::MouseEvent&)
{
    selectingLoop = false;
}

void TabView::mouseDoubleClick (const juce::MouseEvent&)
{
    processor.setLoop (0.0, 0.0, false);
}

void TabView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& d)
{
    if (e.mods.isCtrlDown() || e.mods.isCommandDown())
        processor.setZoom (processor.getZoom() * (d.deltaY > 0 ? 1.15 : 1.0 / 1.15));
    else
        leftBeat += (double) (d.deltaX != 0.0f ? d.deltaX : -d.deltaY) * visibleBeats() * 0.25;
    repaint();
}
}
