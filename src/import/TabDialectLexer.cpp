#include "TabDialectLexer.h"

namespace tabplayer
{
namespace
{
    bool isDigit (juce::juce_wchar c) noexcept
    {
        return c >= '0' && c <= '9';
    }

    const TabNotationDefinition* findDefinition (const NormalizedTabDocument& input,
                                                  const juce::String& pattern)
    {
        for (const auto& d : input.notation)
            if (d.pattern == pattern)
                return &d;
        return nullptr;
    }

    TabToken makeToken (TabTokenKind kind, int row, int start, int end,
                        const juce::String& raw, const NormalizedTabBlock& block)
    {
        TabToken t;
        t.kind = kind;
        t.row = row;
        t.columnStart = start;
        t.columnEnd = end;
        t.rawText = raw;
        const int sourceLine = block.source.sourceLineStart >= 0
                             ? block.source.sourceLineStart + row : -1;
        t.source = { sourceLine, sourceLine, start, end };
        t.confidence = block.confidence;
        return t;
    }

    int lastFretValue (const std::vector<TabToken>& row)
    {
        for (auto it = row.rbegin(); it != row.rend(); ++it)
            if (it->kind == TabTokenKind::fret)
                return it->integerValue;
        return -1;
    }

    void addTechnique (std::vector<TabToken>& tokens, int rowIndex, int column,
                       const juce::String& raw, const juce::String& meaning,
                       const NormalizedTabBlock& block, TabConfidence confidence = TabConfidence::exact)
    {
        auto t = makeToken (TabTokenKind::technique, rowIndex, column,
                            column + raw.length(), raw, block);
        t.canonicalMeaning = meaning;
        t.confidence = confidence;
        tokens.push_back (std::move (t));
    }

    std::vector<TabToken> lexRow (const NormalizedTabDocument& input,
                                  const NormalizedTabBlock& block,
                                  const juce::String& line, int rowIndex,
                                  TabImportDiagnostics& diagnostics)
    {
        std::vector<TabToken> tokens;
        int contentStart = line.indexOfChar ('|');
        contentStart = contentStart >= 0 ? contentStart + 1 : 0;

        for (int i = contentStart; i < line.length();)
        {
            const auto c = line[i];
            const int logicalColumn = i - contentStart;

            if (isDigit (c))
            {
                int j = i;
                while (j < line.length() && isDigit (line[j]))
                    ++j;

                if (j < line.length() && line[j] == '.'
                    && j + 1 < line.length() && isDigit (line[j + 1]))
                {
                    int k = j + 1;
                    while (k < line.length() && isDigit (line[k]))
                        ++k;
                    const auto raw = line.substring (i, k);
                    auto t = makeToken (TabTokenKind::fractionalPosition, rowIndex,
                                        logicalColumn, logicalColumn + raw.length(), raw, block);
                    t.numericValue = raw.getDoubleValue();
                    tokens.push_back (std::move (t));
                    ++diagnostics.fractionalPositions;
                    i = k;
                    continue;
                }

                const auto raw = line.substring (i, j);
                auto t = makeToken (TabTokenKind::fret, rowIndex, logicalColumn,
                                    logicalColumn + raw.length(), raw, block);
                t.integerValue = raw.getIntValue();
                t.numericValue = (double) t.integerValue;
                tokens.push_back (std::move (t));
                i = j;
                continue;
            }

            if (c == 'x' || c == 'X')
            {
                tokens.push_back (makeToken (TabTokenKind::deadNote, rowIndex,
                                             logicalColumn, logicalColumn + 1,
                                             juce::String::charToString (c), block));
                ++i;
                continue;
            }

            if (c == 'h' || c == 'H')
            {
                const bool hasPreviousFret = lastFretValue (tokens) >= 0;
                if (hasPreviousFret && c == 'H')
                    if (const auto* def = findDefinition (input, "<fret>H"))
                    {
                        addTechnique (tokens, rowIndex, logicalColumn, "H",
                                      def->canonicalMeaning, block, def->confidence);
                        ++i;
                        continue;
                    }

                addTechnique (tokens, rowIndex, logicalColumn,
                              juce::String::charToString (c), "hammerOn", block);
                ++i;
                continue;
            }

            if (c == 'p' || c == 'P')
            {
                addTechnique (tokens, rowIndex, logicalColumn,
                              juce::String::charToString (c), "pullOff", block);
                ++i;
                continue;
            }

            if (c == 'b' || c == 'B')
            {
                juce::String meaning = "bend";
                if (const auto* def = findDefinition (input, "<fret>B"))
                    meaning = def->canonicalMeaning;
                addTechnique (tokens, rowIndex, logicalColumn,
                              juce::String::charToString (c), meaning, block);
                ++i;
                continue;
            }

            if (c == '^')
            {
                int j = i + 1;
                while (j < line.length() && isDigit (line[j]))
                    ++j;

                const int from = lastFretValue (tokens);
                const int to = j > i + 1 ? line.substring (i + 1, j).getIntValue() : -1;
                juce::String meaning;

                if (const auto* def = findDefinition (input, "<fret>^<higher-fret>"))
                {
                    if (from >= 0 && to > from)
                        meaning = def->canonicalMeaning;
                }

                if (meaning.isEmpty() && from >= 0 && to >= 0)
                    meaning = to > from ? "hammerOn" : "pullOff";
                if (meaning.isEmpty())
                    meaning = "bend";

                addTechnique (tokens, rowIndex, logicalColumn, "^", meaning, block);
                ++i;
                continue;
            }

            if (c == '/')
            {
                juce::String meaning = "slideUp";
                if (const auto* def = findDefinition (input, "/"))
                    meaning = def->canonicalMeaning;
                addTechnique (tokens, rowIndex, logicalColumn, "/", meaning, block);
                ++i;
                continue;
            }

            if (c == '\\')
            {
                juce::String meaning = "slideDown";
                if (const auto* def = findDefinition (input, "\\"))
                    meaning = def->canonicalMeaning;
                addTechnique (tokens, rowIndex, logicalColumn, "\\", meaning, block);
                ++i;
                continue;
            }

            if (c == '~' || c == 'v')
            {
                addTechnique (tokens, rowIndex, logicalColumn,
                              juce::String::charToString (c), "vibrato", block);
                ++i;
                continue;
            }

            if ((c == 't' || c == 'T') && i + 1 < line.length() && isDigit (line[i + 1]))
            {
                addTechnique (tokens, rowIndex, logicalColumn,
                              juce::String::charToString (c), "tap", block);
                ++i;
                continue;
            }

            ++i;
        }

        return tokens;
    }
}

bool TabDialectLexer::lex (const NormalizedTabDocument& input,
                           TabTokenDocument& output) const
{
    output = {};
    output.metadata = input.metadata;
    output.notation = input.notation;
    output.diagnostics = input.diagnostics;

    for (const auto& block : input.blocks)
    {
        if (block.kind != TabBlockKind::staff)
            continue;

        TabTokenSystem system;
        system.sectionName = block.sectionName;
        system.partIndex = block.partIndex;
        system.confidence = block.confidence;
        system.rows.reserve ((size_t) block.lines.size());

        for (int row = 0; row < block.lines.size(); ++row)
            system.rows.push_back (lexRow (input, block, block.lines[row], row,
                                           output.diagnostics));

        output.systems.push_back (std::move (system));
    }

    return ! output.systems.empty();
}

} // namespace tabplayer
