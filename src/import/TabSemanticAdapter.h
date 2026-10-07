#pragma once

#include "../core/TabDocument.h"

namespace tabplayer
{

/** Bridges the recovered whole-document representation to the existing mature
    ASCII semantic reader. It deliberately keeps recovery policy out of the
    legacy note/timing parser. */
class TabSemanticAdapter
{
public:
    /** Returns text suitable for AsciiTabReader's existing semantic pass.
        Unambiguous document metadata is emitted before the recovered body so
        footer metadata affects notes before they are created. Conflicting
        tuning declarations are deliberately not promoted. */
    static juce::String buildLegacyReaderText (const NormalizedTabDocument& document);

    /** Preserves recovery diagnostics while adding semantic-reader counters and
        warnings. Counters owned by the semantic pass replace their recovery
        counterparts; recovery-only counters are retained. */
    static TabImportDiagnostics mergeDiagnostics (const TabImportDiagnostics& recovery,
                                                  const TabImportDiagnostics& semantic);
};

} // namespace tabplayer
