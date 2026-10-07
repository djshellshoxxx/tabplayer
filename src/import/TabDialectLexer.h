#pragma once

#include "../core/TabDocument.h"

#include <vector>

namespace tabplayer
{

enum class TabTokenKind
{
    fret,
    fractionalPosition,
    deadNote,
    bar,
    repeatStart,
    repeatEnd,
    repeatCount,
    technique,
    annotation,
    unknown
};

struct TabToken
{
    TabTokenKind kind = TabTokenKind::unknown;
    int row = -1;
    int columnStart = 0;
    int columnEnd = 0;
    int integerValue = 0;
    double numericValue = 0.0;
    juce::String rawText;
    juce::String canonicalMeaning;
    TabSourceSpan source;
    TabConfidence confidence = TabConfidence::exact;
};

struct TabTokenSystem
{
    std::vector<std::vector<TabToken>> rows;
    juce::String sectionName;
    int partIndex = 0;
    TabConfidence confidence = TabConfidence::low;
};

struct TabTokenDocument
{
    std::vector<TabTokenSystem> systems;
    TabDocumentMetadata metadata;
    std::vector<TabNotationDefinition> notation;
    TabImportDiagnostics diagnostics;
};

class TabDialectLexer
{
public:
    bool lex (const NormalizedTabDocument& input, TabTokenDocument& output) const;
};

} // namespace tabplayer
