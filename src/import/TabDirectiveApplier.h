#pragma once

#include "../core/PerformanceScore.h"
#include "../core/TabDocument.h"

namespace tabplayer
{

/** Applies prose-level technique directives after the legacy semantic reader.

    Only scopes that can be represented exactly in PerformanceScore are applied.
    Unsupported meanings/scopes remain diagnostics rather than being guessed. */
class TabDirectiveApplier
{
public:
    static void apply (const NormalizedTabDocument& document,
                       PerformanceScore& score,
                       TabImportDiagnostics& diagnostics);
};

} // namespace tabplayer
