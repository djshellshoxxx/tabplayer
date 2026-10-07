#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace tabplayer
{

struct TabSourceSpan
{
    int sourceLineStart = -1;
    int sourceLineEnd = -1;
    int sourceColumnStart = -1;
    int sourceColumnEnd = -1;
};

enum class TabConfidence
{
    exact,
    high,
    medium,
    low
};

enum class TabBlockKind
{
    staff,
    chordLyrics,
    lyric,
    sectionHeading,
    legend,
    proseInstruction,
    attribution,
    unknown
};

enum class DirectiveScope
{
    document,
    part,
    section,
    nextSystem,
    nextPhrase
};

struct TabDirective
{
    juce::String canonicalName;
    juce::String rawText;
    DirectiveScope scope = DirectiveScope::document;
    juce::String sectionName;
    int partIndex = 0;
    TabSourceSpan source;
    TabConfidence confidence = TabConfidence::low;
};

struct TabNotationDefinition
{
    juce::String pattern;
    juce::String canonicalMeaning;
    juce::String rawDefinition;
    TabSourceSpan source;
    TabConfidence confidence = TabConfidence::low;
};

struct TabTuningCandidate
{
    juce::String rawText;
    juce::String canonicalName;
    std::vector<int> midiHighFirst;
    int partIndex = 0;
    bool explicitlyListedNotes = false;
    TabSourceSpan source;
    TabConfidence confidence = TabConfidence::low;
};

struct TabDocumentMetadata
{
    juce::String tuningName;
    std::vector<int> tuningMidiHighFirst;
    std::vector<TabTuningCandidate> tuningCandidates;
    int numStrings = 0;
    int capoFret = -1;
    bool fretsRelativeToCapo = true;
    bool fretsRelativeToCapoExplicit = false;
    bool tuningAmbiguous = false;
    double tempoBpm = 0.0;
    int timeSignatureNumerator = 0;
    int timeSignatureDenominator = 0;
    juce::String instrumentName;
    juce::StringArray partNames;
    juce::StringArray sectionNames;
    std::vector<TabDirective> directives;
};

/** What the ASCII-tab pipeline did with the page. Existing fields are kept for
    source compatibility; newer counters describe recovery before semantics. */
struct TabImportDiagnostics
{
    int totalLines = 0;
    int staffLines = 0;
    int headerLines = 0;
    int annotationLines = 0;
    int skippedLines = 0;
    int systems = 0;
    int measures = 0;
    int notes = 0;
    int numStrings = 0;
    int repeatsUnrolled = 0;
    int ignoredGlyphs = 0;
    int splitFrets = 0;

    bool tuningFromHeader = false;
    bool tuningFromStringNames = false;
    bool tempoFromHeader = false;
    bool timeSignatureFromHeader = false;

    int entitiesDecoded = 0;
    int markdownWrappersRemoved = 0;
    int staffsReconstructed = 0;
    int wrappedRowsJoined = 0;
    int collapsedRowsSplit = 0;
    int inheritedStringLabels = 0;
    int legendEntries = 0;
    int proseDirectives = 0;
    int chordLyricBlocks = 0;
    int multiPartBlocks = 0;
    int fractionalPositions = 0;
    int lowConfidenceBlocksSkipped = 0;
    int ambiguousTokens = 0;
    int metadataConflicts = 0;

    juce::StringArray warnings;

    bool isPartial() const noexcept;
    juce::String summary() const;
};

struct NormalizedTabBlock
{
    TabBlockKind kind = TabBlockKind::unknown;
    juce::StringArray lines;
    TabSourceSpan source;
    TabConfidence confidence = TabConfidence::low;
    juce::String sectionName;
    int partIndex = 0;

    /** String identities inherited/recovered by the normalizer, highest string
        first. Empty for blocks that retained explicit labels or are not staffs. */
    juce::StringArray inferredStringLabels;
};

struct NormalizedTabDocument
{
    juce::String originalText;
    juce::String normalizedText;
    std::vector<NormalizedTabBlock> blocks;
    TabDocumentMetadata metadata;
    std::vector<TabNotationDefinition> notation;
    TabImportDiagnostics diagnostics;
};

} // namespace tabplayer
