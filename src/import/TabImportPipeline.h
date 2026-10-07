#pragma once

#include "AsciiTabReader.h"
#include "TabDocumentNormalizer.h"
#include "TabSemanticAdapter.h"

namespace tabplayer
{

/** End-to-end resilient ASCII-tab import path.

    Recovery and document-wide metadata discovery happen first. The mature
    AsciiTabReader remains the semantic/timing backend until the token compiler
    fully supersedes it, which keeps existing technique behaviour stable. */
class TabImportPipeline
{
public:
    bool read (const juce::String& source, PerformanceScore& destination,
               TabImportDiagnostics* diagnostics = nullptr);

    /** Convenience entry point for the Practice/file-import surface. */
    bool read (const juce::File& file, PerformanceScore& destination,
               TabImportDiagnostics* diagnostics = nullptr);

    juce::String getLastError() const { return lastError; }
    const NormalizedTabDocument& getLastDocument() const noexcept { return lastDocument; }

private:
    juce::String lastError;
    NormalizedTabDocument lastDocument;
};

} // namespace tabplayer
