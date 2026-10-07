#include "TabDirectiveApplier.h"

namespace tabplayer
{
namespace
{
    bool meaningToTechnique (const juce::String& meaning, ScoreTechnique::Type& out) noexcept
    {
        using Type = ScoreTechnique::Type;
        if      (meaning == "naturalHarmonic") out = Type::naturalHarmonic;
        else if (meaning == "trill")           out = Type::trill;
        else if (meaning == "palmMute")        out = Type::palmMute;
        else if (meaning == "letRing")         out = Type::letRing;
        else                                     return false;
        return true;
    }

    void addOnce (ScoreNote& note, ScoreTechnique::Type type)
    {
        if (note.hasTechnique (type))
            return;

        ScoreTechnique technique;
        technique.type = type;
        note.techniques.push_back (std::move (technique));
    }

    void warnOnce (TabImportDiagnostics& diagnostics, const juce::String& message)
    {
        if (! diagnostics.warnings.contains (message))
            diagnostics.warnings.add (message);
    }
}

void TabDirectiveApplier::apply (const NormalizedTabDocument& document,
                                 PerformanceScore& score,
                                 TabImportDiagnostics& diagnostics)
{
    for (const auto& directive : document.metadata.directives)
    {
        ScoreTechnique::Type type {};
        if (! meaningToTechnique (directive.canonicalName, type))
        {
            warnOnce (diagnostics, "Tab instruction preserved but not yet rendered: "
                                   + (directive.rawText.isNotEmpty() ? directive.rawText
                                                                    : directive.canonicalName));
            continue;
        }

        if (directive.scope != DirectiveScope::section || directive.sectionName.isEmpty())
        {
            warnOnce (diagnostics, "Tab instruction scope is not yet safe to apply automatically: "
                                   + (directive.rawText.isNotEmpty() ? directive.rawText
                                                                    : directive.canonicalName));
            continue;
        }

        for (int trackIndex = 0; trackIndex < score.getNumTracks(); ++trackIndex)
        {
            // Normalizer part indices are human-facing and 1-based: Guitar 1,
            // Guitar 2, etc. A zero means the instruction did not name a part.
            if (directive.partIndex > 0 && trackIndex != directive.partIndex - 1)
                continue;

            auto& track = score.getTrack (trackIndex);
            for (auto& measure : track.measures)
            {
                if (! measure.sectionName.equalsIgnoreCase (directive.sectionName))
                    continue;

                for (auto& voice : measure.voices)
                    for (auto& note : voice.notes)
                        addOnce (note, type);
            }
        }
    }
}

} // namespace tabplayer
