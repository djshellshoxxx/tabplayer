#include "TabSemanticAdapter.h"

namespace tabplayer
{
namespace
{
    juce::String tuningHeader (const TabDocumentMetadata& metadata)
    {
        if (metadata.tuningAmbiguous || metadata.tuningMidiHighFirst.empty())
            return {};

        if (metadata.tuningName.isNotEmpty())
            return "Tuning: " + metadata.tuningName.trim();

        juce::String notes;
        static const char* const names[12] = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };

        // The legacy reader expects tuning lists written low-to-high when they
        // follow a Tuning: header. The normalized representation is high-first.
        for (auto it = metadata.tuningMidiHighFirst.rbegin();
             it != metadata.tuningMidiHighFirst.rend(); ++it)
        {
            if (notes.isNotEmpty())
                notes << " ";
            notes << names[juce::jlimit (0, 127, *it) % 12];
        }
        return notes.isNotEmpty() ? "Tuning: " + notes : juce::String();
    }

    bool isOriginalTuningDeclaration (const juce::String& line,
                                      const TabDocumentMetadata& metadata)
    {
        const auto trimmed = line.trim();
        for (const auto& candidate : metadata.tuningCandidates)
            if (trimmed == candidate.rawText.trim())
                return true;
        return false;
    }

    bool isPromotedMetadataLine (const juce::String& line,
                                 const TabDocumentMetadata& metadata)
    {
        if (isOriginalTuningDeclaration (line, metadata))
            return true;

        const auto lower = line.trim().toLowerCase();
        if (lower.startsWith ("capo:") || lower.startsWith ("capo=")
            || lower.startsWith ("capo ") || lower == "no capo")
            return metadata.capoFret >= 0;

        if (lower.startsWith ("tempo:") || lower.startsWith ("tempo=")
            || lower.startsWith ("bpm:") || lower.startsWith ("bpm=")
            || lower.startsWith ("q=") || lower.startsWith ("q ="))
            return metadata.tempoBpm >= 20.0;

        if (lower.startsWith ("time:") || lower.startsWith ("time signature:")
            || lower.startsWith ("meter:") || lower.startsWith ("metre:"))
            return metadata.timeSignatureNumerator > 0
                && metadata.timeSignatureDenominator > 0;

        return false;
    }

    juce::String semanticBody (const NormalizedTabDocument& document)
    {
        juce::StringArray lines = juce::StringArray::fromLines (document.normalizedText);
        juce::StringArray kept;
        kept.ensureStorageAllocated (lines.size());

        for (const auto& line : lines)
            if (! isPromotedMetadataLine (line, document.metadata))
                kept.add (line);

        return kept.joinIntoString ("\n");
    }

    void appendWarningsUnique (juce::StringArray& destination, const juce::StringArray& source)
    {
        for (const auto& warning : source)
            if (! destination.contains (warning))
                destination.add (warning);
    }
}

juce::String TabSemanticAdapter::buildLegacyReaderText (const NormalizedTabDocument& document)
{
    juce::String out;

    if (const auto tuning = tuningHeader (document.metadata); tuning.isNotEmpty())
        out << tuning << "\n";

    if (document.metadata.capoFret >= 0)
        out << "Capo: " << juce::jlimit (0, 24, document.metadata.capoFret) << "\n";

    if (document.metadata.tempoBpm >= 20.0 && document.metadata.tempoBpm <= 300.0)
        out << "Tempo: " << juce::String (document.metadata.tempoBpm, 3).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") << " BPM\n";

    if (document.metadata.timeSignatureNumerator > 0
        && document.metadata.timeSignatureDenominator > 0)
    {
        out << "Time: " << document.metadata.timeSignatureNumerator
            << "/" << document.metadata.timeSignatureDenominator << "\n";
    }

    // Original metadata declarations are removed before the legacy semantic
    // pass. This prevents footer duplicates and, critically, prevents a pair of
    // conflicting tuning declarations from bypassing the normalizer's
    // ambiguity decision.
    out << semanticBody (document);
    return out;
}

TabImportDiagnostics TabSemanticAdapter::mergeDiagnostics (const TabImportDiagnostics& recovery,
                                                           const TabImportDiagnostics& semantic)
{
    auto merged = recovery;

    merged.totalLines = semantic.totalLines;
    merged.staffLines = semantic.staffLines;
    merged.headerLines = semantic.headerLines;
    merged.annotationLines = semantic.annotationLines;
    merged.skippedLines = semantic.skippedLines;
    merged.systems = semantic.systems;
    merged.measures = semantic.measures;
    merged.notes = semantic.notes;
    merged.numStrings = semantic.numStrings;
    merged.repeatsUnrolled = semantic.repeatsUnrolled;
    merged.ignoredGlyphs += semantic.ignoredGlyphs;
    merged.splitFrets += semantic.splitFrets;

    merged.tuningFromHeader = semantic.tuningFromHeader;
    merged.tuningFromStringNames = semantic.tuningFromStringNames;
    merged.tempoFromHeader = semantic.tempoFromHeader;
    merged.timeSignatureFromHeader = semantic.timeSignatureFromHeader;

    appendWarningsUnique (merged.warnings, semantic.warnings);
    return merged;
}

} // namespace tabplayer
