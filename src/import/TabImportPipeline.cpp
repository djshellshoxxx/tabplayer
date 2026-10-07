#include "TabImportPipeline.h"
#include "TabDirectiveApplier.h"

namespace tabplayer
{

bool TabImportPipeline::read (const juce::String& source, PerformanceScore& destination,
                              TabImportDiagnostics* diagnostics)
{
    lastError.clear();
    lastDocument = {};

    TabDocumentNormalizer normalizer;
    if (! normalizer.normalize (source, lastDocument))
    {
        destination.clear();
        lastError = lastDocument.diagnostics.warnings.isEmpty()
                      ? juce::String ("Could not normalize that tablature.")
                      : lastDocument.diagnostics.warnings.joinIntoString ("; ");
        if (diagnostics != nullptr)
            *diagnostics = lastDocument.diagnostics;
        return false;
    }

    const auto prepared = TabSemanticAdapter::buildLegacyReaderText (lastDocument);

    AsciiTabReader reader;
    TabImportDiagnostics semantic;
    const bool ok = reader.read (prepared, destination, &semantic);

    auto merged = TabSemanticAdapter::mergeDiagnostics (lastDocument.diagnostics, semantic);

    if (ok)
        TabDirectiveApplier::apply (lastDocument, destination, merged);

    if (diagnostics != nullptr)
        *diagnostics = merged;
    lastDocument.diagnostics = merged;

    if (! ok)
    {
        lastError = reader.getLastError();
        return false;
    }

    return true;
}

bool TabImportPipeline::read (const juce::File& file, PerformanceScore& destination,
                              TabImportDiagnostics* diagnostics)
{
    lastError.clear();

    if (! file.existsAsFile())
    {
        lastDocument = {};
        destination.clear();
        lastError = "No such file: " + file.getFullPathName();
        if (diagnostics != nullptr)
            *diagnostics = {};
        return false;
    }

    const auto extension = file.getFileExtension().toLowerCase();
    if (extension.isNotEmpty() && extension != ".txt" && extension != ".tab")
    {
        lastDocument = {};
        destination.clear();
        lastError = "The resilient tab pipeline reads .txt and .tab files.";
        if (diagnostics != nullptr)
            *diagnostics = {};
        return false;
    }

    return read (file.loadFileAsString(), destination, diagnostics);
}

} // namespace tabplayer
