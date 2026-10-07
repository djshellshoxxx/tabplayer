#pragma once

#include "../core/PerformanceScore.h"
#include "TabDialectLexer.h"

namespace tabplayer
{

/** Converts lexer-level canonical technique names into score techniques.
    Keeping this mapping explicit prevents source glyph policy from leaking into
    the performance-score model. */
class TabTechniqueCompiler
{
public:
    static bool compile (const TabToken& token, ScoreTechnique& out) noexcept;
};

} // namespace tabplayer
