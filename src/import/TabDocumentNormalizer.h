#pragma once

#include "../core/TabDocument.h"

namespace tabplayer
{

/** Recovers document-wide structure and metadata from real-world ASCII-tab text.
    It does not create score notes or assign final technique semantics. */
class TabDocumentNormalizer
{
public:
    struct Options
    {
        int maxInputBytes = 2 * 1024 * 1024;
        int maxLines = 20000;
        int maxRecoveredSystems = 2048;
        int maxColumnsPerSystem = 8192;
        bool recoverWrappedStaffs = true;
        bool recoverCollapsedRows = true;
        bool detectEmbeddedChords = true;
    };

    bool normalize (const juce::String& source, NormalizedTabDocument& out) const;
    bool normalize (const juce::String& source, NormalizedTabDocument& out,
                    const Options& options) const;
};

} // namespace tabplayer
